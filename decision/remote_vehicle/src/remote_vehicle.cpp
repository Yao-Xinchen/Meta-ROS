#include "rclcpp/rclcpp.hpp"
#include <rclcpp/logging.hpp>
#include <rclcpp/subscription.hpp>
#include <vision_interface/msg/detail/auto_aim__struct.hpp>
#include "remote_vehicle/remote_interpreter.hpp"

#include "operation_interface/msg/remote_control.hpp"
#include "behavior_interface/msg/move.hpp"
#include "behavior_interface/msg/shoot.hpp"
#include "behavior_interface/msg/aim.hpp"
#include "vision_interface/msg/auto_aim.hpp"

using operation_interface::msg::RemoteControl;
using behavior_interface::msg::Move;
using behavior_interface::msg::Shoot;
using behavior_interface::msg::Aim;
using vision_interface::msg::AutoAim;

class RemoteVehicle : public rclcpp::Node
{
public:
    RemoteVehicle() : Node("remote_vehicle")
    {
        double max_vel = this->declare_parameter("control.trans_vel", 2.0);
        double max_omega = this->declare_parameter("control.rot_vel", 3.0);
        double aim_sens = this->declare_parameter("control.aim_sensitivity", 1.57);
        double interfere_sens = this->declare_parameter("control.interfere_sensitivity", 0.2);
        RCLCPP_INFO(this->get_logger(), "max_vel: %f, max_omega: %f, aim_sens: %f, interfere_sens: %f",
            max_vel, max_omega, aim_sens, interfere_sens);

        interpreter = std::make_unique<RemoteInterpreter>(max_vel, max_omega, aim_sens, interfere_sens);

        // pub and sub
        pub_move = this->create_publisher<Move>("move", 10);
        pub_shoot = this->create_publisher<Shoot>("shoot", 10);
        pub_aim = this->create_publisher<Aim>("aim", 10);
        remote_sub_ = this->create_subscription<RemoteControl>(
            "remote_control", 10,
            std::bind(&RemoteVehicle::remote_callback, this, std::placeholders::_1));
        vision_sub_ = this->create_subscription<AutoAim>(
            "auto_aim", 10,
            std::bind(&RemoteVehicle::vision_callback, this, std::placeholders::_1));

        RCLCPP_INFO(this->get_logger(), "RemoteVehicle initialized");
    }

private:
    rclcpp::Subscription<RemoteControl>::SharedPtr remote_sub_;
    rclcpp::Subscription<AutoAim>::SharedPtr vision_sub_;
    rclcpp::Publisher<Move>::SharedPtr pub_move;
    rclcpp::Publisher<Shoot>::SharedPtr pub_shoot;
    rclcpp::Publisher<Aim>::SharedPtr pub_aim;

    std::unique_ptr<RemoteInterpreter> interpreter;

    void remote_callback(const RemoteControl::SharedPtr msg)
    {
        interpreter->manual_input(msg);
    }

    void timer_callback()
    {
        pub_move->publish(*interpreter->get_move());
        pub_shoot->publish(*interpreter->get_shoot());
        pub_aim->publish(*interpreter->get_aim());
    }

    void vision_callback(const AutoAim::SharedPtr msg)
    {
        interpreter->vision_input(msg);
    }
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RemoteVehicle>());
    rclcpp::shutdown();
    return 0;
}