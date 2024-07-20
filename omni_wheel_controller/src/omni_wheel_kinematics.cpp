#include "omni_wheel_controller/omni_wheel_kinematics.hpp"
#include <iostream>

namespace omni_wheel_controller
{
OmniWheelKinematics::OmniWheelKinematics(std::vector<double> omni_wheel_angles, double omni_wheel_distance,
                                         double omni_wheel_radius)
  : omni_wheel_angles_(omni_wheel_angles), omni_wheel_distance_(omni_wheel_distance), omni_wheel_radius_(omni_wheel_radius)
{
  init();
}

void OmniWheelKinematics::init()
{
  motion_mat_ = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(omni_wheel_angles_.size()), 3);
  for (size_t i = 0; i < omni_wheel_angles_.size(); i++)
  {
    motion_mat_(static_cast<Eigen::Index>(i), 0) = cos(omni_wheel_angles_[i] - M_PI / 2.0);
    motion_mat_(static_cast<Eigen::Index>(i), 1) = sin(omni_wheel_angles_[i] - M_PI / 2.0);
    motion_mat_(static_cast<Eigen::Index>(i), 2) = - omni_wheel_distance_;
  }
  std::cout << "motion_mat_ = " << std::endl << motion_mat_ << std::endl;
  motion_mat_ /= omni_wheel_radius_;
}

std::vector<double> OmniWheelKinematics::inverse(const double linear_x, const double linear_y, const double angular_z)
{
  std::vector<double> wheels_vel(omni_wheel_angles_.size(), 0.0);
  Eigen::VectorXd vel(3);
  vel << linear_x, linear_y, angular_z;
  Eigen::VectorXd wheels_vel_eigen = motion_mat_ * vel;
  for (size_t i = 0; i < omni_wheel_angles_.size(); i++)
  {
    wheels_vel[i] = wheels_vel_eigen(static_cast<Eigen::Index>(i));
  }
  return wheels_vel;
}

}  // namespace omni_wheel_controller