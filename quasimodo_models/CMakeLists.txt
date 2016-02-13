cmake_minimum_required(VERSION 2.8.3)
project(quasimodo_models)

#find_package(catkin REQUIRED)# quasimodo_msgs)# COMPONENTS quasimodo_msgs)
set(CMAKE_CXX_FLAGS "-O4 -g -pg -Wunknown-pragmas -Wno-unknown-pragmas -Wsign-compare -fPIC -std=c++0x -o popcnt -mssse3")

find_package(catkin REQUIRED COMPONENTS
	#metaroom_xml_parser
	#semantic_map
	pcl_ros
)

##############################################
##################### INCLUDES ###############
##############################################

# Temp: g2o
#include_directories(/home/nbore/Workspace/dynamic_objects/g2o)

# PCL
find_package(PCL 1.7 REQUIRED)
include_directories(${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
add_definitions(${PCL_DEFINITIONS})

#Opencv stuff
link_directories(/usr/lib/x86_64-linux-gnu)
include_directories(/usr/include/)

find_package(OpenCV)
include_directories(${OpenCV_INCLUDE_DIRS})

include_directories(src)
include_directories(src/registration)
include_directories(src/weightfunctions)
include_directories(src/core)
include_directories(src/model)
include_directories(src/modelupdater)
include_directories(src/modelupdater2)

include_directories(include/)
include_directories(include/registration)
include_directories(include/weightfunctions)
include_directories(include/core)
include_directories(include/model)
include_directories(include/modelupdater)
include_directories(include/modelupdater2)

#QT
#find_package(Qt4 REQUIRED)
#INCLUDE(${QT_USE_FILE})

#ROS INDIGO STUFF
include_directories(/opt/ros/indigo/include)
link_directories(/opt/ros/indigo/lib)

#g2o
link_directories(/usr/local/lib/)
include_directories(/usr/local/include/)

add_library(quasimodo_SICP src/registration/ICP.cpp)
target_link_libraries(quasimodo_SICP ${catkin_LIBRARIES})

add_library(quasimodo_weightlib src/weightfunctions/DistanceWeightFunction2.cpp src/weightfunctions/DistanceWeightFunction2PPR.cpp)
target_link_libraries(quasimodo_weightlib quasimodo_SICP ceres glog ${catkin_LIBRARIES})

add_library(quasimodo_reglib src/registration/Registration.cpp src/registration/RegistrationSICP.cpp src/registration/RegistrationPPR.cpp src/registration/RegistrationGOICP.cpp
	#src/registration/GoICP_V1.3/src/jly_main.cpp
	src/registration/GoICP_V1.3/src/jly_goicp.cpp
	src/registration/GoICP_V1.3/src/jly_3ddt.cpp
	src/registration/GoICP_V1.3/src/matrix.cpp
	src/registration/GoICP_V1.3/src/ConfigMap.cpp
	src/registration/GoICP_V1.3/src/StringTokenizer.cpp
	#src/registration/GoICP_V1.3/src/CostAndWeight.cpp
)
target_link_libraries(quasimodo_reglib quasimodo_SICP quasimodo_weightlib ceres glog ${catkin_LIBRARIES})

add_library(quasimodo_core  src/core/Camera.cpp src/core/RGBDFrame.cpp)
target_link_libraries(quasimodo_core  ${OpenCV_LIBRARIES} ${catkin_LIBRARIES})

add_library(quasimodo_massreglib src/registration/MassRegistration.cpp src/registration/MassRegistrationPPR.cpp)
target_link_libraries(quasimodo_massreglib quasimodo_reglib quasimodo_core)# g2o_cli g2o_interactive g2o_solver_cholmod g2o_solver_slam2d_linear g2o_types_sba g2o_types_slam3d_addons g2o_core g2o_interface g2o_solver_csparse g2o_solver_structure_only g2o_types_sclam2d g2o_types_slam3d g2o_csparse_extension g2o_opengl_helper g2o_solver_dense g2o_stuff g2o_types_sim3 g2o_viewer g2o_ext_freeglut_minimal g2o_parser g2o_solver_eigen g2o_types_data g2o_types_slam2d_addons g2o_incremental g2o_simulator g2o_solver_pcg g2o_types_icp g2o_types_slam2d)

add_library(quasimodo_model src/model/Model.cpp)
target_link_libraries(quasimodo_model quasimodo_core quasimodo_reglib ${catkin_LIBRARIES})

add_library(quasimodo_ModelUpdater src/modelupdater/ModelUpdater.cpp src/modelupdater/ModelUpdaterBasic.cpp src/modelupdater/ModelUpdaterBasicFuse.cpp)
target_link_libraries(quasimodo_ModelUpdater quasimodo_model quasimodo_reglib quasimodo_massreglib ${catkin_LIBRARIES})

catkin_package(
  INCLUDE_DIRS include
#  LIBRARIES quasimodo_models
#  CATKIN_DEPENDS other_catkin_pkg
#  DEPENDS system_lib
)