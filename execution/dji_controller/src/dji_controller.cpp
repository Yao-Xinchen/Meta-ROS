#include "rclcpp/rclcpp.hpp"
#include "dji_controller/dji_driver.h"
#include <cmath>
#include <cstdint>
#include <linux/can.h>
#include <memory>
#include <string>
#include <vector>

#include "motor_interface/msg/motor_goal.hpp"
#include "motor_interface/msg/motor_state.hpp"

#define ENABLE_PUB TRUE
#define PUB_R 20 // ms

class DjiController : public rclcpp::Node
{
public:
    DjiController() : Node("DjiController")
    {
        motor_init();
        goal_sub_ = this->create_subscription<motor_interface::msg::MotorGoal>(
            "motor_goal", 10, [this](const motor_interface::msg::MotorGoal::SharedPtr msg){
                goal_callback(msg);
            });
        control_timer_ = this->create_wall_timer(
            std::chrono::milliseconds(CONTROL_R), [this](){
                control_timer_callback();
            });
        feedback_timer_ = this->create_wall_timer(
            std::chrono::milliseconds(FEEDBACK_R), [this](){
                feedback_timer_callback();
            });
#if ENABLE_PUB == TRUE
        pub_timer_ = this->create_wall_timer(
            std::chrono::milliseconds(PUB_R), [this](){
                pub_timer_callback();
            });
        state_pub_ = this->create_publisher<motor_interface::msg::MotorState>("motor_state", 10);
#endif
        RCLCPP_INFO(this->get_logger(), "DjiController initialized");
    }

private:
    rclcpp::Subscription<motor_interface::msg::MotorGoal>::SharedPtr goal_sub_;
    rclcpp::TimerBase::SharedPtr control_timer_; // send control frame regularly
    rclcpp::TimerBase::SharedPtr feedback_timer_; // receive feedback frame regularly
#if ENABLE_PUB == TRUE
    rclcpp::TimerBase::SharedPtr pub_timer_;
    rclcpp::Publisher<motor_interface::msg::MotorState>::SharedPtr state_pub_;
#endif
    int dji_motor_count;
    std::vector<std::unique_ptr<DjiDriver>> drivers_; // std::unique_ptr<DjiDriver> drivers_[8];
    std::vector<double> p2v_kps{}, p2v_kis{}, p2v_kds{};
    std::vector<double> v2c_kps{}, v2c_kis{}, v2c_kds{};

    void control_timer_callback()
    {
        for (auto& driver : drivers_) driver->write_tx();
        DjiDriver::tx();
    }

    void feedback_timer_callback()
    {
        DjiDriver::rx();
        for (auto& driver : drivers_) driver->process_rx();
    }
    
    void goal_callback(const motor_interface::msg::MotorGoal::SharedPtr msg)
    {
        // RCLCPP_INFO(this->get_logger(), "Received motor goal");
        int count = msg->motor_id.size();
        for (int i = 0; i < count; i++)
        {
            std::string rid = msg->motor_id[i];
            float pos = msg->goal_pos[i];
            float vel = msg->goal_vel[i];

            // find corresponding driver
            auto iter = std::find_if(drivers_.begin(), drivers_.end(),
                [rid](const std::unique_ptr<DjiDriver>& driver){
                    return driver->rid == rid;
                });

            // set goal
            if (iter != drivers_.end())
            {
                auto driver = iter->get();
                driver->set_goal(pos, vel);
                // RCLCPP_INFO(this->get_logger(), "%s receive goal", rid.c_str());
            }
            else {
                // not found, may be a dm motor
                // RCLCPP_WARN(this->get_logger(), "Motor %d not found", rid);
            }
        }
    }

#if ENABLE_PUB == TRUE
    void pub_timer_callback()
    {
        // publish feedback
        motor_interface::msg::MotorState msg;
        for (auto& driver : drivers_)
        {
            msg.motor_id.push_back(driver->rid);
            auto [pos, vel, cur] = driver->get_state();
            msg.present_pos.push_back(pos);
            msg.present_vel.push_back(vel);
            msg.present_tor.push_back(cur);
        }
        state_pub_->publish(msg);
    }
#endif

    void motor_init()
    {
        int motor_count = this->declare_parameter("motor.count", 0);

        std::vector<int64_t> motor_brands{};
        motor_brands = this->declare_parameter("motor.brands", motor_brands);
        std::vector<std::string> motor_rids{};
        motor_rids = this->declare_parameter("motor.rids", motor_rids);
        std::vector<int64_t> motor_hids{};
        motor_hids = this->declare_parameter("motor.hids", motor_hids);
        std::vector<int64_t> motor_types{};
        motor_types = this->declare_parameter("motor.types", motor_types);
        
        p2v_kps = this->declare_parameter("motor.p2v.kps", p2v_kps);
        p2v_kis = this->declare_parameter("motor.p2v.kis", p2v_kis);
        p2v_kds = this->declare_parameter("motor.p2v.kds", p2v_kds);
        v2c_kps = this->declare_parameter("motor.v2c.kps", v2c_kps);
        v2c_kis = this->declare_parameter("motor.v2c.kis", v2c_kis);
        v2c_kds = this->declare_parameter("motor.v2c.kds", v2c_kds);

        // initialize the drivers
        for (int i = 0; i < motor_count; i++)
        {
            if (motor_brands[i] != 0) continue; // only create drivers for DJI motors
            
            dji_motor_count++;
            MotorType type = static_cast<MotorType>(motor_types[i]);
            std::string rid = motor_rids[i];
            int hid = motor_hids[i];
            drivers_.push_back(std::make_unique<DjiDriver>(rid, hid, type));
            drivers_.back()->set_p2v_pid(p2v_kps[i], p2v_kis[i], p2v_kds[i]);
            drivers_.back()->set_v2c_pid(v2c_kps[i], v2c_kis[i], v2c_kds[i]);

            RCLCPP_INFO(this->get_logger(), "Motor rid %s hid %d initialized, with %f %f %f %f %f %f",
                drivers_.back()->rid.c_str(), drivers_.back()->hid,
                p2v_kps[i], p2v_kis[i], p2v_kds[i], v2c_kps[i], v2c_kis[i], v2c_kds[i]);
        }
    }
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<DjiController>());
    rclcpp::shutdown();
    return 0;
}