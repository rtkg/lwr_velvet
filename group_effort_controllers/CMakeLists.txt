cmake_minimum_required(VERSION 2.8.3)
project(group_effort_controllers)

#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE RelWithDebInfo)
#set(CMAKE_BUILD_TYPE Release)

# Load catkin and all dependencies required for this package
find_package(catkin REQUIRED COMPONENTS
     cmake_modules
     roscpp
     controller_interface
     control_toolbox
     control_msgs
     realtime_tools
)

find_package(Eigen REQUIRED)
find_package(Boost REQUIRED)

## System dependencies are found with CMake's conventions
# find_package(Boost REQUIRED COMPONENTS system)

# Declare catkin package
catkin_package(
     CATKIN_DEPENDS
     roscpp
     controller_interface
     control_toolbox
     control_msgs
     realtime_tools
     INCLUDE_DIRS include ${EIGEN_INCLUDE_DIRS}
     LIBRARIES ${PROJECT_NAME}
)


###########
## Build ##
###########


# include_directories(include)
include_directories(SYSTEM ${EIGEN_INCLUDE_DIRS})
include_directories(include ${Boost_INCLUDE_DIR} ${catkin_INCLUDE_DIRS})

# link_directories

add_library(${PROJECT_NAME} src/joint_group_velocity_controller.cpp
                            include/group_effort_controllers/joint_group_velocity_controller.h)

target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})

## Add cmake target dependencies of the executable/library
## as an example, message headers may need to be generated before nodes
# add_dependencies(hqp_velocity_controller_node hqp_velocity_controller_generate_messages_cpp)



#############
## Install ##
#############

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION})


install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
  )

install(FILES group_effort_controllers_plugins.xml
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION})

