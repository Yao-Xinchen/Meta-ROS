#include "rclcpp/rclcpp.hpp"

#include "agv_chassis/agv_kinematics.hpp"
#include <rclcpp/publisher.hpp>

#define YAW 8

class AgvChassis : public rclcpp::Node
{
private:
    rclcpp::Subscription<movement_interface::msg::NaturalMove>::SharedPtr nat_sub_;
    rclcpp::Subscription<movement_interface::msg::AbsoluteMove>::SharedPtr abs_sub_;
    rclcpp::Publisher<motor_interface::msg::DmGoal>::SharedPtr dm_pub_;
    rclcpp::Publisher<motor_interface::msg::DjiGoal>::SharedPtr dji_pub_;
    rclcpp::Client<gyro_interface::srv::GimbalPosition>::SharedPtr gimbal_cli_;
    rclcpp::Client<motor_interface::srv::MotorPresent>::SharedPtr motor_cli_;

    void nat_callback(const movement_interface::msg::NaturalMove::SharedPtr nat_msg)
    {
        auto request_motor = std::make_shared<motor_interface::srv::MotorPresent::Request>();
        request_motor->motor_id.clear();
        request_motor->motor_id.push_back(YAW);
        auto result_motor = motor_cli_->async_send_request(request_motor);
        float motor_pos = result_motor.get()->present_pos[YAW];

        dm_pub_->publish(AgvKinematics::natural_dm_decompo(nat_msg, motor_pos));
        dji_pub_->publish(AgvKinematics::natural_dji_decompo(nat_msg, motor_pos));
    }

    void abs_callback(const movement_interface::msg::AbsoluteMove::SharedPtr abs_msg)
    {
        auto request_gimbal = std::make_shared<gyro_interface::srv::GimbalPosition::Request>();
        auto result_gimbal = gimbal_cli_->async_send_request(request_gimbal);
        float gimbal_yaw = result_gimbal.get()->yaw;

        auto request_motor = std::make_shared<motor_interface::srv::MotorPresent::Request>();
        request_motor->motor_id.clear();
        request_motor->motor_id.push_back(YAW);
        auto result_motor = motor_cli_->async_send_request(request_motor);
        float motor_pos = result_motor.get()->present_pos[YAW];
        
        dm_pub_->publish(AgvKinematics::absolute_dm_decompo(abs_msg, gimbal_yaw + motor_pos));
        dji_pub_->publish(AgvKinematics::absolute_dji_decompo(abs_msg, gimbal_yaw + motor_pos));
    }
public:
    AgvChassis() : Node("AgvChassis")
    {
        nat_sub_ = this->create_subscription<movement_interface::msg::NaturalMove>(
            "natural_move", 10, [this](const movement_interface::msg::NaturalMove::SharedPtr msg){
                this->nat_callback(msg);
            });
        abs_sub_ = this->create_subscription<movement_interface::msg::AbsoluteMove>(
            "absolute_move", 10, [this](const movement_interface::msg::AbsoluteMove::SharedPtr msg){
                this->abs_callback(msg);
            });

        // motor_pub_ = this->create_publisher<motor_interface::msg::MotorGoal>("motor_goal", 10);

        gimbal_cli_ = this->create_client<gyro_interface::srv::GimbalPosition>("gimbal_position");
        gimbal_cli_->wait_for_service(std::chrono::seconds(2));

        motor_cli_ = this->create_client<motor_interface::srv::MotorPresent>("motor_present");
        motor_cli_->wait_for_service(std::chrono::seconds(2));
    }
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AgvChassis>());
    rclcpp::shutdown();
    return 0;
}