import os
from pathlib import Path
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, GroupAction,
                            IncludeLaunchDescription, SetEnvironmentVariable)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('application'),
        'config',
        'sentry_config.yaml'
    )

    ahrs_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('fdilink_ahrs'),
                        'launch/ahrs_driver.launch.py')
        )
    )

    vision_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('solais_interpreter'),
                        'launch/vision_bringup.launch.py')
        )
    )

    ld = LaunchDescription([
        Node(
            package="auto_sentry",
            executable="auto_sentry",
            name="auto_sentry",
        ),
        Node(
            package="uni_gimbal",
            executable="uni_gimbal",
            name="uni_gimbal",
            parameters=[config],
        ),
    ])

    ld.add_action(ahrs_launch)
    ld.add_action(vision_launch)

    return ld