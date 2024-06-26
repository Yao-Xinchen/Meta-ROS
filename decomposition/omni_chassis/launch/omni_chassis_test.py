import os
from ament_index_python import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('omni_chassis'),
        'config',
        'omni_chassis_test.yaml'
    )
    return LaunchDescription([
        Node(
            package='omni_chassis',
            executable='omni_chassis',
            name='omni_chassis',
            parameters=[config],
        ),
        Node(
            package='dji_controller',
            executable='dji_controller',
            name='dji_controller',
            parameters=[config],
        ),
    ])