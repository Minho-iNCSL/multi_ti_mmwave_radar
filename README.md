# How to use?

Instruction

1. Clone this repo.

```bash
# In your workspace, such as... '<workspace dir>/src'
git clone https://github.com/Minho-iNCSL/multi_ti_mmwave_radar.git
```

2. Install apt Dependencies

```bash
# install linux pkgs
sudo apt-get install libpthread-stubs0-dev
sudo apt install ros-eloquent-perception-pcl -y
sudo apt install ros-eloquent-composition -y
```

3. Build ROS 2 Packages

```bash
# cd `<workspace dir>/src`
git clone https://github.com/kimsooyoung/mmwave_ti_ros.git

# pkg build
colcon build --symlink-install --packages-select serial
source install/local_setup.bash

colcon build --symlink-install --packages-select ti_mmwave_ros2_interfaces
source install/local_setup.bash

colcon build --symlink-install --packages-select ti_mmwave_ros2_pkg
source install/local_setup.bash

# For this package, pcl common is required, But don't be afraid, ROS Installing contains PCL
colcon build --symlink-install --packages-select ti_mmwave_ros2_examples
source install/local_setup.bash
```

4. Launch ROS 2 Command

Before launch command, check each radar port in launch file (ti_mmwave_ros2_pkg/launch/foxy_multi_[center,left,right].launch.py)

* Command Port with boadrate 115200
* Data Port with boadrate 921600
 
```bash
ros2 launch ti_mmwave_ros2_pkg mmwave_multi_radar.launch.py
```

