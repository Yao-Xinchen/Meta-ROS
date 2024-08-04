# Copyright 2021 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, FindExecutable, LaunchConfiguration, PathJoinSubstitution
from launch.conditions import IfCondition, UnlessCondition

from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

# Necessary dirty work that lets us import modules from the metav_bringup package
import os
import sys
from ament_index_python.packages import get_package_share_directory
sys.path.append(os.path.join(get_package_share_directory('metav_bringup'), 'launch'))

from launch_utils import load_controller, register_loading_order, register_sequential_loading

ARGUMENTS = [
    DeclareLaunchArgument(
        'enable_simulation',
        default_value='false',
        description='If true, the simulation will be started'),
]

def generate_launch_description():
    # Launch Arguments
    enable_simulation = LaunchConfiguration('enable_simulation')

    # Get URDF via xacro
    robot_description_content = Command(
        [
            PathJoinSubstitution([FindExecutable(name='xacro')]),
            ' ',
            PathJoinSubstitution(
                [FindPackageShare('metav_description'),
                 'urdf', 'sentry', 'sentry.xacro']
            ),
            ' ',
            'is_simulation:=', enable_simulation,
        ]
    )

    robot_controllers = PathJoinSubstitution(
        [
            FindPackageShare("metav_description"),
            "config",
            "sentry.yaml",
        ]
    )

    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[
            {'use_sim_time': enable_simulation,
            'robot_description': robot_description_content,
            'publish_frequency': 100.0}
        ],
        output='both',
        emulate_tty=True
    )

    controller_manager = Node(
        package="controller_manager",
        executable="ros2_control_node",
        parameters=[robot_controllers],
        remappings=[
            ("~/robot_description", "/robot_description"),
        ],
        output='both',
        emulate_tty=True,
        condition=UnlessCondition(enable_simulation)
    )

    gz_spawn_robot = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=['-topic', '/robot_description',
                   '-name', 'sentry', '-allow_renaming', 'true',
                   '-x', '6.35',
                    '-y', '7.6',
                    '-z', '0.2',
                    '-Y', '0.0'],
        output='both',
        emulate_tty=True,
        condition=IfCondition(enable_simulation)
    )

    load_joint_state_broadcaster = load_controller('joint_state_broadcaster')

    # List of controllers to be loaded sequentially
    # Order in this list is IMPORTANT
    load_controllers = [
        load_controller('wheels_pid_controller'),
        load_controller('gimbal_controller'),
        load_controller('omni_chassis_controller'),
    ]

    # Gazebo related launch
    gazebo_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            [PathJoinSubstitution([FindPackageShare('ros_gz_sim'),
                                    'launch',
                                    'gz_sim.launch.py'])]),
        launch_arguments=[
            ('gz_args', [PathJoinSubstitution([FindPackageShare('metav_gazebo'), 'worlds', 'rmuc2024.sdf']),
                        ' -r',
                        ' -v 3',])
        ],
        condition=IfCondition(enable_simulation)
    )

    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        parameters=[{
            'config_file': PathJoinSubstitution([FindPackageShare('metav_bringup'), 'config', 'ros_gz_bridge.yaml']),
            'qos_overrides./tf_static.publisher.durability': 'transient_local',
        }],
        output='both',
        emulate_tty=True,
        condition=IfCondition(enable_simulation)
    )

    dbus_control_node = Node(
        package='dbus_control',
        executable='dbus_control_node',
        name='dbus_control_node',
        parameters=[robot_controllers],
        output='both',
        emulate_tty=True,
    )

    # auto_sentry_node = Node(
    #     package='auto_sentry',
    #     executable='auto_sentry',
    #     name='auto_sentry_node',
    #     parameters=[robot_controllers],
    # )

    dbus_vehicle_node = Node(
        package='dbus_vehicle',
        executable='dbus_vehicle',
        name='dbus_vehicle',
        parameters=[robot_controllers],
        output='both',
        emulate_tty=True,
    )

    ahrs_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('fdilink_ahrs'),
                        'launch/ahrs_driver.launch.py')
        ),
        condition=UnlessCondition(enable_simulation)
    )

    return LaunchDescription([
        # Launch Arguments
        *ARGUMENTS,
        # Launch gazebo environment
        gazebo_launch,
        # Gazebo Gazebo ROS 2 bridge
        bridge,
        # Load robot state publisher
        node_robot_state_publisher,
        # Spawn robot in Gazebo (this will automatically start controller manager)
        gz_spawn_robot,
        # Launch controller manager (if not in simulation)
        controller_manager,
        # Load joint state broadcaster
        load_joint_state_broadcaster,
        # Load controllers
        *register_sequential_loading(load_joint_state_broadcaster, *load_controllers),
        dbus_control_node,
        # auto_sentry_node,
        dbus_vehicle_node,
        ahrs_launch,
    ])
