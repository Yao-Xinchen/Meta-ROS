#include "rclcpp/rclcpp.hpp"

#include "dm_controller/can_driver.hpp"
#include "dm_controller/dm_driver.h"

#include "motor_interface/msg/dm_goal.hpp"
#include <memory>

#define VEL_MODE 0
#define MIT_MODE 1

class DmController : public rclcpp::Node
{
public:
    DmController() : Node("dm_controller")
    {
        motor_init();
        sub_ = this->create_subscription<motor_interface::msg::DmGoal>(
            "dm_goal", 10, [this](motor_interface::msg::DmGoal::SharedPtr msg){
                this->msg_callback(msg);
            });
    }

    ~DmController()
    {
        for (auto& driver : dm_driver_)
        {
            driver->turn_off();
        }
    }

private:
    int dm_motor_count = 0;
    rclcpp::Subscription<motor_interface::msg::DmGoal>::SharedPtr sub_;
    std::vector<std::unique_ptr<DmDriver>> dm_driver_; // std::unique_ptr<DmDriver> dm_driver_[8];
    std::vector<double> kps{}, kis{};

    void motor_init()
    {
        int motor_count = this->declare_parameter("motor_count", 0);

        std::vector<int64_t> motor_brands {};
        motor_brands = this->declare_parameter("motor_brands", motor_brands);
        std::vector<int64_t> motor_ids {};
        motor_ids = this->declare_parameter("motor_ids", motor_ids);
        std::vector<int64_t> motor_types {};
        motor_types= this->declare_parameter("motor_types", motor_types);
        kps = this->declare_parameter("p2v_kps", kps);
        kis = this->declare_parameter("p2v_kis", kis);

        for (int i = 0; i < motor_count; i++)
        {
            if (motor_brands[i] != 1) continue; // only create drivers for DM motors

            dm_motor_count++;
            int rid = motor_ids[i];

            if (motor_types[i] == VEL_MODE)
            {
                dm_driver_.push_back(std::make_unique<DmVelDriver>(rid));
            }
            else if (motor_types[i] == MIT_MODE)
            {
                dm_driver_.push_back(std::make_unique<DmMitDriver>(rid, kps[i], kis[i]));
            }
            else {
                RCLCPP_WARN(this->get_logger(), "Motor %d type %d not supported", rid, static_cast<int>(motor_types[i]));
            }
        }

        for (auto& driver : dm_driver_)
        {
            driver->turn_on();
        }
    }

    void msg_callback(motor_interface::msg::DmGoal::SharedPtr msg)
    {
        int count = msg->motor_id.size();
        for (int i = 0; i < count; i++)
        {
            int rid = msg->motor_id[i];
            float pos = msg->goal_pos[i];
            float vel = msg->goal_vel[i];

            // find corresponding driver
            auto iter = std::find_if(dm_driver_.begin(), dm_driver_.end(),
                [rid](const std::unique_ptr<DmDriver>& driver){
                    return driver->rid == rid;
                });

            // set goal
            if (iter != dm_driver_.end())
            {
                auto driver = iter->get();
                driver->set_position(pos);
                driver->set_velocity(vel);
            }
            else {
                // not found, do nothing
                RCLCPP_WARN(this->get_logger(), "Motor %d not found", rid);
            }
        }
    }
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<DmController>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}