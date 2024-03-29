#include "agv_chassis/agv_kinematics.hpp"
#include <cmath>

float AgvKinematics::wheel_r = 0.05;
float AgvKinematics::cha_r = 0.3;
float AgvKinematics::decel_ratio = 20.0;

unordered_map<string, float> AgvKinematics::vel = 
{
    // {rid, vel}
    {"LF_V", 0.0},
    {"RF_V", 0.0},
    {"LB_V", 0.0},
    {"RB_V", 0.0},
}; // m/s

unordered_map<string, float> AgvKinematics::pos = 
{
    // {rid, pos}
    {"LF_D", 0.0},
    {"RF_D", 0.0},
    {"LB_D", 0.0},
    {"RB_D", 0.0},
}; // rad

unordered_map<string, pair<float, bool>> AgvKinematics::offsets =
{
    // {rid, {offset, found}}
    {"LF_D", {0.0, false}},
    {"RF_D", {0.0, false}},
    {"LB_D", {0.0, false}},
    {"RB_D", {0.0, false}},
}; // rad

MotorGoal AgvKinematics::natural_decompo(const behavior_interface::msg::Move::SharedPtr msg, float yaw_diff)
{
    MotorGoal motor_goal;
    clear_goal(motor_goal);

    float trans_n = msg->vel_x * sin(yaw_diff) + msg->vel_y * cos(yaw_diff);
    float trans_tao = msg->vel_x * cos(yaw_diff) - msg->vel_y * sin(yaw_diff);
    float rot_vel = msg->omega * cha_r / sqrt(2);

    add_group_goal(motor_goal, "LF", trans_n + rot_vel, trans_tao - rot_vel);
    add_group_goal(motor_goal, "RF", trans_n + rot_vel, trans_tao + rot_vel);
    add_group_goal(motor_goal, "LB", trans_n - rot_vel, trans_tao - rot_vel);
    add_group_goal(motor_goal, "RB", trans_n - rot_vel, trans_tao + rot_vel);

    return motor_goal;
}

MotorGoal AgvKinematics::absolute_decompo(const behavior_interface::msg::Move::SharedPtr msg, float chassis_yaw)
{
    MotorGoal motor_goal;
    clear_goal(motor_goal);

    float trans_n = msg->vel_x * cos(chassis_yaw) - msg->vel_y * sin(chassis_yaw);
    float trans_tao = msg->vel_x * sin(chassis_yaw) + msg->vel_y * cos(chassis_yaw);
    float rot_vel = msg->omega * cha_r / sqrt(2);

    add_group_goal(motor_goal, "LF", trans_n + rot_vel, trans_tao - rot_vel);
    add_group_goal(motor_goal, "RF", trans_n + rot_vel, trans_tao + rot_vel);
    add_group_goal(motor_goal, "LB", trans_n - rot_vel, trans_tao - rot_vel);
    add_group_goal(motor_goal, "RB", trans_n - rot_vel, trans_tao + rot_vel);
    
    return motor_goal;
}

MotorGoal AgvKinematics::chassis_decompo(const behavior_interface::msg::Move::SharedPtr msg)
{
    MotorGoal motor_goal;
    clear_goal(motor_goal);

    float trans_n = msg->vel_y;
    float trans_tao = msg->vel_x;
    float rot_vel = msg->omega * cha_r / sqrt(2);

    add_group_goal(motor_goal, "LF", trans_n + rot_vel, trans_tao - rot_vel);
    add_group_goal(motor_goal, "RF", trans_n + rot_vel, trans_tao + rot_vel);
    add_group_goal(motor_goal, "LB", trans_n - rot_vel, trans_tao - rot_vel);
    add_group_goal(motor_goal, "RB", trans_n - rot_vel, trans_tao + rot_vel);
    
    return motor_goal;
}

void AgvKinematics::clear_goal(MotorGoal &motor_goal)
{
    motor_goal.motor_id.clear();
    motor_goal.goal_vel.clear();
    motor_goal.goal_pos.clear();
}

void AgvKinematics::add_vel_goal(MotorGoal &motor_goal, const string& rid, const float goal_vel)
{
    motor_goal.motor_id.push_back(rid);
    motor_goal.goal_vel.push_back(goal_vel / wheel_r * decel_ratio); // convert to rad/s
    motor_goal.goal_pos.push_back(NaN);
}

void AgvKinematics::add_pos_goal(MotorGoal &motor_goal, const string &rid, const float goal_pos)
{
    motor_goal.motor_id.push_back(rid);
    motor_goal.goal_vel.push_back(NaN);
    motor_goal.goal_pos.push_back(goal_pos + offsets[rid].first); // already in rad
}

void AgvKinematics::add_group_goal(MotorGoal &motor_goal, const string& which, float vx, float vy)
{
    string dir_id = which + "_D";
    string vel_id = which + "_V";

    float velocity = rss(vx, vy);
    if (is_zero(velocity)) // if vel is 0, keep the direction
    {
        // pos[dir_id] does not change
        vel[vel_id] = 0;
    }
    else if (is_zero(vy)) // if vy is 0
    {
        pos[dir_id] = 1E-3; // not zero, to enable pos control
        vel[vel_id] = vx;
    }
    else if (is_zero(vx)) // if vx is 0, vx/vy would be 0/0
    {
        pos[dir_id] = PI / 2;
        vel[vel_id] = vy;
    }
    else // if vy is not 0, calculate the direction
    {
        pos[dir_id] = atan(vx / vy);
        vel[vel_id] = velocity;
    }

    add_vel_goal(motor_goal, vel_id, vel[vel_id]); // MY_TODO: check this
    add_pos_goal(motor_goal, dir_id, pos[dir_id]);
    
}

float AgvKinematics::rss(float x, float y)
{
    return std::sqrt(std::pow(x, 2) + std::pow(y, 2));
}

bool AgvKinematics::is_zero(float x)
{
    return std::abs(x) < TRIGGER;
}