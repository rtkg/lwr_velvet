#include <group_effort_controllers/joint_group_velocity_controller.h>
#include <pluginlib/class_list_macros.h>

namespace group_effort_controllers
{
  //-----------------------------------------------------------------------
  JointGroupVelocityController::JointGroupVelocityController() {}
  //-----------------------------------------------------------------------
  JointGroupVelocityController::~JointGroupVelocityController() {}
  //-----------------------------------------------------------------------
  bool JointGroupVelocityController::init(hardware_interface::EffortJointInterface *hw, ros::NodeHandle &n)
  {
    n_ = n;
    // Get the list of controlled joints
    std::string param_name = "joints";
    if(!n.getParam(param_name, joint_names_))
      {
        ROS_ERROR_STREAM("Failed to getParam '" << param_name << "' (namespace: " << n.getNamespace() << ").");
        return false;
      }
    n_joints_ = joint_names_.size();

    for(unsigned int i=0; i<n_joints_; i++)
      {
        try
	  {
            joints_.push_back(hw->getHandle(joint_names_[i]));
	  }
        catch (const hardware_interface::HardwareInterfaceException& e)
	  {
            ROS_ERROR_STREAM("Exception thrown: " << e.what());
            return false;
	  }
      }

    //initialize the PID controllers
    std::string pid_ = ("pid_");
    pids_.resize(n_joints_);
    for (unsigned int i=0; i<n_joints_;i++)
      if (!pids_[i].init(ros::NodeHandle(n, pid_ + joints_[i].getName())))
	{
	  ROS_ERROR("Error initializing the PID for joint %d",i);
	  return false;
	}

    // Start realtime state publishers
    for (unsigned int i=0; i<n_joints_;i++)
      {
	boost::shared_ptr<realtime_tools::RealtimePublisher<control_msgs::JointControllerState> > ptr( new realtime_tools::RealtimePublisher<control_msgs::JointControllerState>(n, joints_[i].getName()+"/state", 1));
	c_state_pub_.push_back(ptr);
      }


    //============================================== REGISTER CALLBACKS =========================================

 for (unsigned int i=0; i<n_joints_;i++)
   {
     boost::shared_ptr<ros::Subscriber> ptr(new ros::Subscriber(n.subscribe<std_msgs::Float64>(joints_[i].getName()+"/command", 1, boost::bind(&JointGroupVelocityController::setCommandCB, this, _1, i))));
	// sub[i] = node.subscribe(sub_name, 1, boost::bind(&MyClass::callback, this, _1, i));
     command_sub_.push_back(ptr);
   }

    return true;
  }

  //-----------------------------------------------------------------------
  void JointGroupVelocityController::starting(const ros::Time& time)
  {
    // Start controller with 0.0 velocities
    commands_.resize(n_joints_);
    commands_.setZero();
    for (unsigned int i=0; i<n_joints_;i++)
      pids_[i].reset();
  }


  //-----------------------------------------------------------------------
  void JointGroupVelocityController::update(const ros::Time& time, const ros::Duration& period)
  {

    // Set the PID error and compute the PID command with nonuniform time
    // step size. The derivative error is computed from the change in the error
    // and the timestep dt.
    for(unsigned int i=0; i<n_joints_; i++)
      {
	double error = commands_(i) - joints_[i].getVelocity();
	double commanded_effort = pids_[i].computeCommand(error, period);
	joints_[i].setCommand(commanded_effort);
      }

    if (PUBLISH_RATE > 0.0 && last_publish_time_ + ros::Duration(1.0/PUBLISH_RATE) < time)
      {
	//increment time
	last_publish_time_ = last_publish_time_ + ros::Duration(1.0/PUBLISH_RATE);

	// publish the tracking controller stuff
	for (unsigned int i=0; i<n_joints_;i++)
	  {
	    if (c_state_pub_[i]->trylock())
	      {
		c_state_pub_[i]->msg_.header.stamp = time;
		c_state_pub_[i]->msg_.set_point = commands_(i);
		c_state_pub_[i]->msg_.process_value_dot = (c_state_pub_[i]->msg_.process_value - joints_[i].getVelocity())/period.toSec();
		c_state_pub_[i]->msg_.process_value = joints_[i].getVelocity();
		c_state_pub_[i]->msg_.error = commands_(i) -joints_[i].getVelocity();
		c_state_pub_[i]->msg_.time_step = period.toSec();
		c_state_pub_[i]->msg_.command = joints_[i].getCommand();

		double dummy;
		  pids_[i].getGains(c_state_pub_[i]->msg_.p,
				    c_state_pub_[i]->msg_.i,
				    c_state_pub_[i]->msg_.d,
				    c_state_pub_[i]->msg_.i_clamp,
				    dummy);

		c_state_pub_[i]->unlockAndPublish();
	      }
	  }
      }

  }
  //-----------------------------------------------------------------------

  ///////////////
  // CALLBACKS //
  ///////////////

  //-----------------------------------------------------------------------
  void JointGroupVelocityController::setCommandCB(const std_msgs::Float64ConstPtr& msg, unsigned int i)
  {
    commands_(i) = msg->data;
}

  //-----------------------------------------------------------------------
} //end namespace hqp_controllers

PLUGINLIB_EXPORT_CLASS(group_effort_controllers::JointGroupVelocityController,controller_interface::ControllerBase)
