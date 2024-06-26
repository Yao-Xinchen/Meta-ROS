#include "rclcpp/rclcpp.hpp"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <rclcpp/timer.hpp>

#include "behavior_interface/msg/shoot.hpp"
#include "device_interface/msg/motor_goal.hpp"
#include "device_interface/msg/motor_state.hpp"

#define NaN std::nan("")

#define PUB_FREQ 10 // ms

class ShootLoad : public rclcpp::Node
{
public:
    ShootLoad() : Node("shoot_load")
    {
        init_stop_command();
        get_param();

        shoot_sub_ = this->create_subscription<behavior_interface::msg::Shoot>("shoot",
            10, std::bind(&ShootLoad::goal_callback, this, std::placeholders::_1));
        feeback_sub_ = this->create_subscription<device_interface::msg::MotorState>("motor_state",
            10, std::bind(&ShootLoad::feedback_callback, this, std::placeholders::_1));
        motor_pub_ = this->create_publisher<device_interface::msg::MotorGoal>("motor_goal", 10);
        pub_timer_ = this->create_wall_timer(std::chrono::milliseconds(PUB_FREQ),
            std::bind(&ShootLoad::pub_timer_callback, this)
        );
        update_timer_ = this->create_wall_timer(std::chrono::milliseconds(PUB_FREQ),
            std::bind(&ShootLoad::update_timer_callback, this)
        );

        RCLCPP_INFO(this->get_logger(), "ShootLoad initialized.");
    }

private:
    rclcpp::Subscription<behavior_interface::msg::Shoot>::SharedPtr shoot_sub_;
    rclcpp::Subscription<device_interface::msg::MotorState>::SharedPtr feeback_sub_;
    rclcpp::Publisher<device_interface::msg::MotorGoal>::SharedPtr motor_pub_;
    rclcpp::TimerBase::SharedPtr pub_timer_;
    rclcpp::TimerBase::SharedPtr update_timer_;

    device_interface::msg::MotorGoal stop;
    double last_rec = 0.0; // the last time a command is received

    // ros params, constant after initialization
    double feed_max_vel_l = 5.0, feed_max_vel_r = 5.0;
    double fric_max_vel_u = 300.0, fric_max_vel_d = 300.0;
    double feed_ratio = 36.0, fric_ratio = 1.0;
    int64_t drawback_delay; // ms
    double jammed_threshold = 0.05; // rad/s, without deceleration

    // local vars
    double feed_goal_vel_l = 0.0, feed_goal_vel_r;
    double fric_goal_vel_u = 0.0, fric_goal_vel_d = 0.0;
    double last_fine_moment_l, last_fine_moment_r;
    double feed_fb_vel_l = 0.0, feed_fb_vel_r = 0.0;
    bool active_fric = false, active_feed = false;

    void get_param()
    {
        feed_max_vel_l = this->declare_parameter("gimbal.feed_vel_l", feed_max_vel_l);
        feed_max_vel_r = this->declare_parameter("gimbal.feed_vel_r", feed_max_vel_r);
        fric_max_vel_u = this->declare_parameter("gimbal.fric_vel_u", fric_max_vel_u);
        fric_max_vel_d = this->declare_parameter("gimbal.fric_vel_d", fric_max_vel_d);
        feed_ratio = this->declare_parameter("gimbal.feed_ratio", feed_ratio);
        fric_ratio = this->declare_parameter("gimbal.fric_ratio", fric_ratio);
        drawback_delay = this->declare_parameter("gimbal.drawback_delay", 0x1000000); // no draw back by default
        jammed_threshold = this->declare_parameter("gimbal.jammed_threshold", 0.05);
        RCLCPP_INFO(this->get_logger(), "Feed velocity: %f and %f, Friction velocity: %f and %f", feed_max_vel_l, feed_max_vel_r, fric_max_vel_u, fric_max_vel_d);
    }

    void goal_callback(const behavior_interface::msg::Shoot::SharedPtr msg)
    {
        last_rec = rclcpp::Clock().now().seconds();
        active_feed = msg->feed_state;
        active_fric = msg->fric_state;
    }

    void feedback_callback(device_interface::msg::MotorState::SharedPtr feedback)
    {
        for (auto i = 0; i < static_cast<int>(feedback->motor_id.size()); i++)
        {
            if (feedback->motor_id[i] == "FEED_L") feed_fb_vel_l = feedback->present_vel[i];
            if (feedback->motor_id[i] == "FEED_R") feed_fb_vel_r = feedback->present_vel[i];
        }
    }

    void update_timer_callback()
    {
        // feed
        if (active_feed == true) // to shoot
        {
            // left
            if (abs(feed_fb_vel_l) < jammed_threshold) // jammed
            {
                // reverse rotation if jammed for long enough
                auto jammed_duration = rclcpp::Clock().now().seconds() - last_fine_moment_l;
                if (jammed_duration * 1000 > drawback_delay) feed_goal_vel_l = - feed_max_vel_l;
            }
            else // not jammed
            {
                last_fine_moment_l = rclcpp::Clock().now().seconds();
                feed_goal_vel_l = feed_max_vel_l;
            }

            // right
            if (abs(feed_fb_vel_r) < jammed_threshold) // jammed
            {
                // reverse rotation if jammed for long enough
                auto jammed_duration = rclcpp::Clock().now().seconds() - last_fine_moment_r;
                if (jammed_duration * 1000 > drawback_delay) feed_goal_vel_r = - feed_max_vel_r;
            }
            else // not jammed
            {
                last_fine_moment_r = rclcpp::Clock().now().seconds();
                feed_goal_vel_r = feed_max_vel_r;
            }
        }
        else // not to shoot
        {
            feed_goal_vel_l = false;
            feed_goal_vel_r = false;
        }

        // fric
        if (active_fric == true)
        {
            fric_goal_vel_d = fric_max_vel_d;
            fric_goal_vel_u = fric_max_vel_u;
        }
        else // not to shoot
        {
            fric_goal_vel_d = 0.0;
            fric_goal_vel_u = 0.0;
        }
    }

    void pub_timer_callback()
    {
        if (rclcpp::Clock().now().seconds() - last_rec > 0.1)
        {
            motor_pub_->publish(stop);
            return;
        }

        device_interface::msg::MotorGoal motor_msg;
        clear(motor_msg);

        motor_msg.motor_id.push_back("FRIC_U");
        motor_msg.goal_vel.push_back(fric_goal_vel_u * fric_ratio);
        motor_msg.goal_pos.push_back(NaN);
        motor_msg.goal_tor.push_back(NaN);

        motor_msg.motor_id.push_back("FRIC_D");
        motor_msg.goal_vel.push_back(fric_goal_vel_d * fric_ratio);
        motor_msg.goal_pos.push_back(NaN);
        motor_msg.goal_tor.push_back(NaN);

        motor_msg.motor_id.push_back("FEED_L");
        motor_msg.goal_vel.push_back(feed_goal_vel_l * feed_ratio);
        motor_msg.goal_pos.push_back(NaN);
        motor_msg.goal_tor.push_back(NaN);

        motor_msg.motor_id.push_back("FEED_R");
        motor_msg.goal_vel.push_back(feed_goal_vel_r * feed_ratio);
        motor_msg.goal_pos.push_back(NaN);
        motor_msg.goal_tor.push_back(NaN);
        
        motor_pub_->publish(motor_msg);
    }

    void clear(device_interface::msg::MotorGoal msg)
    {
        msg.motor_id.clear();
        msg.goal_vel.clear();
        msg.goal_pos.clear();
    }

    void init_stop_command()
    {
        stop = [] {
            device_interface::msg::MotorGoal stop_;
            auto set_zero_vel = [&stop_](const std::string &rid) {
                stop_.motor_id.push_back(rid);
                stop_.goal_tor.push_back(NaN);
                stop_.goal_vel.push_back(0.0);
                stop_.goal_pos.push_back(NaN);
            };
            std::array rids = {"FRIC_U", "FRIC_D", "FEED_L", "FEED_R"};
            for (const auto &id : rids) set_zero_vel(id);
            return stop_;
        }();
    }
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node_ = std::make_shared<ShootLoad>();
    rclcpp::spin(node_);
    rclcpp::shutdown();
    return 0;
}