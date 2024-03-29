#include "dji_controller/dji_driver.h"
#include "dji_controller/can_driver.hpp"
#include <cstdint>
#include <deque>
#include <linux/can.h>
#include <memory>
#include <queue>
#include <rclcpp/utilities.hpp>
#include <string>
#include <tuple>

#define DT CALC_FREQ / 1000 // the time interval, in seconds

std::unique_ptr<CanDriver> DjiDriver::can_0 = std::make_unique<CanDriver>(0);
std::unique_ptr<can_frame> DjiDriver::tx_frame_200 = init_frame(0x200);
std::unique_ptr<can_frame> DjiDriver::tx_frame_1ff = init_frame(0x1ff);
std::unique_ptr<can_frame> DjiDriver::tx_frame_2ff = init_frame(0x2ff);
can_frame DjiDriver::rx_frame;

DjiDriver::DjiDriver(const std::string& rid, const int hid, MotorType type) :
    p2v_prm(0.1, 0.01, 0.1),
    v2c_prm(0.004, 0.00003, 0.1),
    hid(hid),
    rid(rid)
{
    this->motor_type = type;
    this->p2v_out = PidOutput();
    this->v2c_out = PidOutput();
}

void DjiDriver::set_goal(float goal_pos, float goal_vel)
{
    this->goal_pos = goal_pos;
    this->goal_vel = goal_vel;
}

void DjiDriver::set_p2v_pid(float kp, float ki, float kd)
{
    p2v_prm.kp = kp;
    p2v_prm.ki = ki;
    p2v_prm.kd = kd;
}

void DjiDriver::set_v2c_pid(float kp, float ki, float kd)
{
    v2c_prm.kp = kp;
    v2c_prm.ki = ki;
    v2c_prm.kd = kd;
}

void DjiDriver::vel2current()
{
    float prev_error = vel_error;
    vel_error = goal_vel - present_data.velocity;

    v2c_out.p = v2c_prm.kp * vel_error;
    v2c_out.i += v2c_prm.ki * vel_error * DT; curb(v2c_out.i, I_MAX / 4);
    v2c_out.d = v2c_prm.kd * (vel_error - prev_error) / DT;

    this->current = v2c_out.sum();

    // restrict the current
    curb(current, I_MAX);
}

void DjiDriver::pos2velocity()
{
    float prev_error = pos_error;
    pos_error = goal_pos - present_data.position;

    p2v_out.p = p2v_prm.kp * pos_error;
    p2v_out.i += p2v_prm.ki * pos_error * DT; curb(p2v_out.i, V_MAX / 4);
    p2v_out.d = p2v_prm.kd * (pos_error - prev_error) / DT;

    this->goal_vel = p2v_out.sum();

    // restrict the velocity
    curb(goal_vel, V_MAX);
}

std::unique_ptr<can_frame> DjiDriver::init_frame(int frame_id)
{
    std::unique_ptr<can_frame> frame = std::make_unique<can_frame>();
    frame->can_id = frame_id;
    frame->can_dlc = 8;
    for (int i = 0; i < 8; i++) frame->data[i] = 0;
    return frame;
}

void DjiDriver::process_rx()
{
    int16_t pos_raw = (static_cast<int16_t>(rx_frame.data[0]) << 8) | (rx_frame.data[1] & 0xFF);
    int16_t vel_raw = (static_cast<int16_t>(rx_frame.data[2]) << 8) | (rx_frame.data[3] & 0xFF);
    int16_t tor_raw = (static_cast<int16_t>(rx_frame.data[4]) << 8) | (rx_frame.data[5] & 0xFF);

    if (motor_type == M3508)
    {
        if ((int)rx_frame.can_id != 0x200 + hid) return;
    }
    else if (motor_type == M6020)
    {
        if ((int)rx_frame.can_id != 0x204 + hid) return;
    }
    else if (motor_type == M2006)
    {
        if ((int)rx_frame.can_id != 0x200 + hid) return;
    }
    
    present_data.update_pos((float)pos_raw / 8192.0f * 2 * 3.1415926f); // rad
    present_data.velocity = (float)vel_raw * 3.1415926f / 30.0f; // rpm to rad/s
    present_data.torque = (float)tor_raw / 16384 * 20; // actually current, Ampere
}

void DjiDriver::write_tx()
{
    if (goal_pos != NaN && goal_vel == NaN) pos2velocity();
    vel2current();

    if (motor_type == M3508)
    {
        int16_t current_data = static_cast<int16_t>(current / I_MAX * 16384);
        if (hid <= 4)
        {
            tx_frame_200->data[2 * hid - 2] = (uint8_t)(current_data >> 8);
            tx_frame_200->data[2 * hid - 1] = (uint8_t)(current_data & 0xff);
        } else {
            tx_frame_1ff->data[2 * (hid - 4) - 2] = (uint8_t)(current_data >> 8);
            tx_frame_1ff->data[2 * (hid - 4) - 1] = (uint8_t)(current_data & 0xff);
        }
    }
    else if (motor_type == M6020)
    {
        int16_t current_data = static_cast<int16_t>(current / I_MAX * 30000);
        if (hid <= 4)
        {
            tx_frame_1ff->data[2 * hid - 2] = (uint8_t)(current_data >> 8);
            tx_frame_1ff->data[2 * hid - 1] = (uint8_t)(current_data & 0xff);
        } else {
            tx_frame_2ff->data[2 * (hid - 4) - 2] = (uint8_t)(current_data >> 8);
            tx_frame_2ff->data[2 * (hid - 4) - 1] = (uint8_t)(current_data & 0xff);
        }
    }
    else if (motor_type == M2006)
    {
        int16_t current_data = static_cast<int16_t>(current / I_MAX * 16384);
        if (hid <= 4)
        {
            tx_frame_200->data[2 * hid - 2] = (uint8_t)(current_data >> 8);
            tx_frame_200->data[2 * hid - 1] = (uint8_t)(current_data & 0xff);
        } else {
            tx_frame_1ff->data[2 * (hid - 4) - 2] = (uint8_t)(current_data >> 8);
            tx_frame_1ff->data[2 * (hid - 4) - 1] = (uint8_t)(current_data & 0xff);
        }
    }
}

void DjiDriver::tx()
{
    can_0->send_frame(*tx_frame_200);
    rclcpp::sleep_for(std::chrono::milliseconds(1));
    can_0->send_frame(*tx_frame_1ff);
    rclcpp::sleep_for(std::chrono::milliseconds(1));
    can_0->send_frame(*tx_frame_2ff);
}

void DjiDriver::rx()
{
    can_0->get_frame(rx_frame);
}

std::tuple<float, float, float> DjiDriver::get_state()
{
    return std::make_tuple(
        present_data.position,
        present_data.velocity,
        present_data.torque
    );
}

void DjiDriver::curb(float &val, float limit)
{
    if (val > limit) val = limit;
    else if (val < -limit) val = -limit;
}

void DjiDriver::stop_all()
{
    for (auto &current: tx_frame_200->data) current = 0;
    for (auto &current: tx_frame_1ff->data) current = 0;
    for (auto &current: tx_frame_2ff->data) current = 0;
    DjiDriver::tx();
}