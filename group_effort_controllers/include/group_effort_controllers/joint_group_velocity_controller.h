#ifndef JOINT_GROUP_VELOCITY_CONTROLLER_H
#define JOINT_GROUP_VELOCITY_CONTROLLER_H

#include <vector>
#include <string>
#include <Eigen/Geometry>
#include <ros/node_handle.h>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <hardware_interface/joint_command_interface.h>
#include <controller_interface/controller.h>
#include <control_msgs/JointControllerState.h>
#include <std_msgs/Float64.h>
#include <control_toolbox/pid.h>
#include <realtime_tools/realtime_publisher.h>

namespace group_effort_controllers
{
#define PUBLISH_RATE 50 //The rate to publish
//--------------------------------------------------------
/**
   * \brief velocity controller for a set of joints.
   *
   */
class JointGroupVelocityController: public controller_interface::Controller<hardware_interface::EffortJointInterface>
{
public:

    JointGroupVelocityController();
    ~JointGroupVelocityController();

    std::vector< std::string > joint_names_;
    std::vector< hardware_interface::JointHandle  > joints_;

    Eigen::VectorXd commands_;
    unsigned int n_joints_;

    bool init(hardware_interface::EffortJointInterface *hw, ros::NodeHandle &n);
    void starting(const ros::Time& time);
    void update(const ros::Time& time, const ros::Duration& period);

private:

    //    boost::mutex lock_;
    ros::NodeHandle n_;
    std::vector<control_toolbox::Pid> pids_;

    std::vector<boost::shared_ptr<ros::Subscriber> > command_sub_;
    std::vector<boost::shared_ptr<realtime_tools::RealtimePublisher<control_msgs::JointControllerState> > > c_state_pub_;
    ros::Time last_publish_time_;

    //**Helper function to read joint limits from the parameter server and generate the corresponding tasks.*/
   // bool jointLimitsParser(ros::NodeHandle &n);

    ///////////////
    // CALLBACKS //
    ///////////////

 void setCommandCB(const std_msgs::Float64ConstPtr& msg, unsigned int i);

};


} //end namespace group_effort_controllers

#endif
