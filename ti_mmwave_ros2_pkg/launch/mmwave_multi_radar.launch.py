import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():
    multi_rviz = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            get_package_share_directory('ti_mmwave_ros2_pkg'), 'launch'),
            '/eloquent_multi_only_rviz.py'])
        )

    center_radar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            get_package_share_directory('ti_mmwave_ros2_pkg'), 'launch'),
            '/foxy_multi_center.launch.py'])
        )

    left_radar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            get_package_share_directory('ti_mmwave_ros2_pkg'), 'launch'),
            '/foxy_multi_left.launch.py'])
        )

    right_radar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([os.path.join(
            get_package_share_directory('ti_mmwave_ros2_pkg'), 'launch'),
            '/foxy_multi_right.launch.py'])
        )

    return LaunchDescription([
        multi_rviz,
        center_radar,
        left_radar,
        right_radar
    ])


