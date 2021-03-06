#include <grasping_experiments/grasping_experiments.h>
#include <gazebo_msgs/SetPhysicsProperties.h>
#include <math.h>
#include <limits>
#include <time.h>
#include <boost/assign/std/vector.hpp>
#include <hqp_controllers_msgs/TaskGeometry.h>
#include <hqp_controllers_msgs/RemoveTasks.h>
#include <hqp_controllers_msgs/ActivateHQPControl.h>
#include <hqp_controllers_msgs/VisualizeTaskGeometries.h>
#include <hqp_controllers_msgs/LoadTasks.h>
#include <hqp_controllers_msgs/FindCanTask.h>

namespace grasping_experiments
{
using namespace boost::assign;
//-----------------------------------------------------------------
GraspingExperiments::GraspingExperiments() : task_error_tol_(0.0), task_diff_tol_(1e-5), task_timeout_tol_(0.5)
{

    //handle to home
    nh_ = ros::NodeHandle("~");
    //global handle
    n_ = ros::NodeHandle();

    //get params
    nh_.param<bool>("with_gazebo", with_gazebo_,false);
    if(with_gazebo_)
        ROS_INFO("Grasping experiments running in Gazebo.");

    //initialize variables
    task_status_changed_ = false;
    task_success_ = false;

    //register general callbacks
    start_demo_srv_ = nh_.advertiseService("start_demo", &GraspingExperiments::startDemo, this);
    gimme_beer_srv_ = nh_.advertiseService("gimme_beer", &GraspingExperiments::gimmeBeer, this);
    lets_dance_srv_ = nh_.advertiseService("lets_dance", &GraspingExperiments::letsDance, this);
    look_what_i_found_srv_ = nh_.advertiseService("look_what_i_found", &GraspingExperiments::lookWhatIFound, this);
    task_status_sub_ = n_.subscribe("task_status_array", 1, &GraspingExperiments::taskStatusCallback, this);
    joint_state_sub_ = n_.subscribe("joint_states", 1, &GraspingExperiments::jointStateCallback, this);
    set_tasks_clt_ = n_.serviceClient<hqp_controllers_msgs::SetTasks>("set_tasks");
    remove_tasks_clt_ = n_.serviceClient<hqp_controllers_msgs::RemoveTasks>("remove_tasks");
    activate_hqp_control_clt_ = n_.serviceClient<hqp_controllers_msgs::ActivateHQPControl>("activate_hqp_control");
    visualize_task_geometries_clt_ = n_.serviceClient<hqp_controllers_msgs::VisualizeTaskGeometries>("visualize_task_geometries");
    set_gazebo_physics_clt_ = n_.serviceClient<gazebo_msgs::SetPhysicsProperties>("set_physics_properties");
    load_tasks_clt_ = n_.serviceClient<hqp_controllers_msgs::LoadTasks>("load_tasks");
    reset_hqp_control_clt_ = n_.serviceClient<std_srvs::Empty>("reset_hqp_control");

    switch_controller_clt_ = n_.serviceClient<controller_manager_msgs::SwitchController>("switch_controller");
    if(!with_gazebo_)
    {
        get_grasp_interval_clt_ = n_.serviceClient<hqp_controllers_msgs::FindCanTask>("get_grasp_interval");
        velvet_pos_clt_ = n_.serviceClient<velvet_interface_node::VelvetToPos>("velvet_pos");
        velvet_grasp_clt_ = n_.serviceClient<velvet_interface_node::SmartGrasp>("velvet_grasp");
        set_stiffness_clt_ = n_.serviceClient<lbr_fri::SetStiffness>("set_stiffness");
        next_truck_task_clt_ = n_.serviceClient<std_srvs::Empty>("execute_truck_task");
	
	get_grasp_interval_clt_.waitForExistence();
        velvet_pos_clt_.waitForExistence();
        velvet_grasp_clt_.waitForExistence();
        set_stiffness_clt_.waitForExistence();
        next_truck_task_clt_.waitForExistence();
    }
    else
    {
        //if gazebo is used, set the simulated gravity to zero in order to prevent gazebo's joint drifting glitch
        set_gazebo_physics_clt_.waitForExistence();

        gazebo_msgs::SetPhysicsProperties properties;
        properties.request.time_step = 0.001;
        properties.request.max_update_rate = 1000;
        properties.request.gravity.x = 0.0;
        properties.request.gravity.y = 0.0;
        properties.request.gravity.z = 0.0;
        properties.request.ode_config.auto_disable_bodies = false;
        properties.request.ode_config. sor_pgs_precon_iters = 0;
        properties.request.ode_config. sor_pgs_iters = 50;
        properties.request.ode_config. sor_pgs_w = 1.3;
        properties.request.ode_config.sor_pgs_rms_error_tol = 0.0;
        properties.request.ode_config.contact_surface_layer = 0.001;
        properties.request.ode_config.contact_max_correcting_vel = 100.0;
        properties.request.ode_config.cfm = 0.0;
        properties.request.ode_config.erp = 0.2;
        properties.request.ode_config.max_contacts= 20.0;

        set_gazebo_physics_clt_.call(properties);
        if(!properties.response.success)
        {
            ROS_ERROR("Couldn't set Gazebo physics properties, status message: %s!", properties.response.status_message.c_str());
            ros::shutdown();
        }
        else
            ROS_INFO("Disabled gravity in Gazebo.");
    }

    set_tasks_clt_.waitForExistence();
    remove_tasks_clt_.waitForExistence();
    activate_hqp_control_clt_.waitForExistence();
    visualize_task_geometries_clt_.waitForExistence();
    load_tasks_clt_.waitForExistence();
    reset_hqp_control_clt_.waitForExistence();

    //PRE-DEFINED JOINT CONFIGURATIONS
    //configs have to be within the safety margins of the joint limits
#ifdef HQP_GRIPPER_JOINT
    unsigned int n_jnts = 8;
#else
    unsigned int n_jnts = 7;
#endif

    transfer_config_ = std::vector<double>(n_jnts);
    transfer_config_[0] = 0;
    transfer_config_[1] = -1.57;
    transfer_config_[2] = 2.42;
    transfer_config_[3] = -1.0;
    transfer_config_[4] = 0;
    transfer_config_[5] = 0.52;
    transfer_config_[6] = 0.0;
#ifdef HQP_GRIPPER_JOINT
    transfer_config_[7] = 0.1;
#endif

    sensing_config_ = std::vector<double>(n_jnts);
    sensing_config_[0] = 2.26;
    sensing_config_[1] = 1.6;
    sensing_config_[2] = 0.09;
    sensing_config_[3] = 1.69;
    sensing_config_[4] = -0.09;
    sensing_config_[5] = -1.69;
    sensing_config_[6] = -1.47;
#ifdef HQP_GRIPPER_JOINT
    sensing_config_[7] = 0.1;
#endif

    gimme_beer_config_ = std::vector<double>(n_jnts);
    gimme_beer_config_[0] = 2.26;
    gimme_beer_config_[1] = 1.6;
    gimme_beer_config_[2] = 0.09;
    gimme_beer_config_[3] = 1.69;
    gimme_beer_config_[4] = -0.09;
    gimme_beer_config_[5] = -1.69;
    gimme_beer_config_[6] = -1.47;
#ifdef HQP_GRIPPER_JOINT
    gimme_beer_config_[7] = 0.1;
#endif

    //DEFAULT GRASP
    grasp_.obj_frame_ = "world"; //object frame
    grasp_.e_frame_ = "velvet_fingers_palm"; //endeffector frame
    grasp_.e_.setZero(); //endeffector point expressed in the endeffector frame

#ifdef PILE_GRASPING
    grasp_.a_(0) = 0.0; grasp_.a_(1) = -1.0; grasp_.a_(2) = 0.0;
    grasp_.p_(0) = -0.75; grasp_.p_(1) = 0.3; grasp_.p_(2) = 1.065;
#else
    grasp_.v_(0) = 0.0; grasp_.v_(1) = 0.0; grasp_.v_(2) = 1.0; //cylinder normal
    grasp_.p_(0) = 0.9; grasp_.p_(1) = -0.9; grasp_.p_(2) = 0.16; //reference point on the cylinder axis
    grasp_.r1_ = 0.05; grasp_.r2_ = 0.15; //cylinder radii
    grasp_.n1_ =grasp_.v_; grasp_.n2_ = -grasp_.v_; //plane normals
    grasp_.d1_ = 0.2; grasp_.d2_= -0.35; //plane offsets
#endif

    //PLACEMENT ZONES
    PlaceInterval place;
    place.place_frame_ = "world";
    place.e_frame_ = "velvet_fingers_palm";
    place.e_(0) = 0.16; place.e_(1) = 0.0; place.e_(2) = 0.0;
    place.v_(0) = 0.0; place.v_(1) = 0.0; place.v_(2) = 1.0;
    place.p_(0) = 0.75; place.p_(1) = 0.2; place.p_(2) = 0.16;
    place.r_ = 0.02;
    place.n_(0) = 0.0; place.n_(1) = 0.0; place.n_(2) = 1.0;
    place.d_ = 0.26;
    place.joints_ += 1.81, 1.01, -0.75, -1.28, 0.79, 0.85, -2.26;
    //place_zones_.push_back(place);

    place.joints_.clear();
    place.p_(1) = 0.0;
    place.joints_ += 2.11, 0.58, -1.00, -1.71, 0.58, 0.82, -2.20;
    place_zones_.push_back(place);

    place.joints_.clear();
    place.p_(1) = -0.2;
    place.joints_ += 0.038, -0.26, 0.94, -1.88, 0.51, 1.01, -2.29;
    //place_zones_.push_back(place);
}
//-----------------------------------------------------------------
bool GraspingExperiments::setCartesianStiffness(double sx, double sy, double sz, double sa, double sb, double sc)
{
    if(!with_gazebo_)
    {
        lbr_fri::SetStiffness cart_stiffness;

        cart_stiffness.request.sx = sx;
        cart_stiffness.request.sy = sy;
        cart_stiffness.request.sz = sz;
        cart_stiffness.request.sa = sa;
        cart_stiffness.request.sb = sb;
        cart_stiffness.request.sc = sc;
        if(!set_stiffness_clt_.call(cart_stiffness))
        {
            ROS_ERROR("Could not set the cartesian stiffness!");
            return false;
        }
    }

    return true;
}
//-----------------------------------------------------------------
void GraspingExperiments::activateHQPControl()
{
    hqp_controllers_msgs::ActivateHQPControl controller_status;
    controller_status.request.active = true;
    activate_hqp_control_clt_.call(controller_status);
}
//-----------------------------------------------------------------
void GraspingExperiments::deactivateHQPControl()
{
    hqp_controllers_msgs::ActivateHQPControl controller_status;
    controller_status.request.active = false;
    activate_hqp_control_clt_.call(controller_status);
}
//-----------------------------------------------------------------
void GraspingExperiments::safeShutdown()
{
    deactivateHQPControl();
    resetState();
    std_srvs::Empty srv;
    reset_hqp_control_clt_.call(srv);
    pers_task_vis_ids_.clear();
    ROS_BREAK(); //I must break you ... ros::shutdown() doesn't seem to do the job
}
//-----------------------------------------------------------------
bool GraspingExperiments::getGraspInterval()
{
    //get the grasp intervall
    get_grasp_interval_clt_.waitForExistence();
    hqp_controllers_msgs::FindCanTask grasp;
    get_grasp_interval_clt_.call(grasp);

    if(!grasp.response.success)
        return false;

#ifdef PILE_GRASPING
    ROS_ASSERT(grasp.response.CanTask.size()==2);
    std::vector<double> data;
    grasp_.obj_frame_ = grasp.response.reference_frame;

    ROS_ASSERT(grasp.response.CanTask[0].g_type == hqp_controllers_msgs::TaskGeometry::POINT);
    data = grasp.response.CanTask[0].g_data;
    grasp_.p_(0) = data[0]; grasp_.p_(1) = data[1]; grasp_.p_(2) = data[2];

    ROS_ASSERT(grasp.response.CanTask[1].g_type == hqp_controllers_msgs::TaskGeometry::LINE);
    data = grasp.response.CanTask[1].g_data;
    grasp_.a_(0) = data[3]; grasp_.a_(1) = data[4]; grasp_.a_(2) = data[5];
#else
    ROS_ASSERT(grasp.response.CanTask.size()==4);
    std::vector<double> data;
    grasp_.obj_frame_ = grasp.response.reference_frame;

    //BOTTOM PLANE
    ROS_ASSERT(grasp.response.CanTask[0].g_type == hqp_controllers_msgs::TaskGeometry::PLANE);
    data = grasp.response.CanTask[0].g_data;
    ROS_ASSERT(data.size() == 4);
    grasp_.n1_(0) = data[0];  grasp_.n1_(1) = data[1]; grasp_.n1_(2) = data[2];
    grasp_.d1_ = data[3];

    //TOP PLANE
    ROS_ASSERT(grasp.response.CanTask[1].g_type == hqp_controllers_msgs::TaskGeometry::PLANE);
    data = grasp.response.CanTask[1].g_data;
    ROS_ASSERT(data.size() == 4);
    grasp_.n2_(0) = data[0];  grasp_.n2_(1) = data[1]; grasp_.n2_(2) = data[2];
    grasp_.d2_ = data[3];

    //INNER GRASP CYLINDER
    ROS_ASSERT(grasp.response.CanTask[2].g_type == hqp_controllers_msgs::TaskGeometry::CYLINDER);
    data = grasp.response.CanTask[2].g_data;
    ROS_ASSERT(data.size() == 7);
    grasp_.p_(0) = data[0];  grasp_.p_(1) = data[1]; grasp_.p_(2) = data[2];
    grasp_.v_(0) = data[3];  grasp_.v_(1) = data[4]; grasp_.v_(2) = data[5];
    grasp_.r1_ = data[6];

    //OUTER GRASP CYLINDER
    ROS_ASSERT(grasp.response.CanTask[3].g_type == hqp_controllers_msgs::TaskGeometry::CYLINDER);
    data = grasp.response.CanTask[3].g_data;
    ROS_ASSERT(data.size() == 7);
    grasp_.r2_ = data[6];

    //Plane normals need to point in opposit directions to give a closed interval
    ROS_ASSERT((grasp_.n1_.transpose() * grasp_.n2_)  <= 0.0);

#endif
    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::resetState()
{
    hqp_controllers_msgs::RemoveTasks rem_t_srv;
    for(unsigned int i=0; i<tasks_.response.ids.size(); i++)
        rem_t_srv.request.ids.push_back(tasks_.response.ids[i]);

    remove_tasks_clt_.call(rem_t_srv);
    if(!rem_t_srv.response.success)
    {
        ROS_ERROR("GraspingExperiments::resetStateTasks(): could not remove tasks!");
        return false;
    }
    //clean up the task message which is used as a container
    tasks_.response.ids.clear();
    tasks_.response.success = false;
    tasks_.request.tasks.clear();

    //clean up the monitored tasks
    monitored_tasks_.clear();

    //clean up the previous task progress vector;
    t_prog_prev_.resize(0);

    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::visualizeStateTasks(std::vector<unsigned int> const& ids)
{
    //send a visualization message to show the tasks in Rviz
    hqp_controllers_msgs::VisualizeTaskGeometries vis_srv;
    for(unsigned int i=0; i<ids.size();i++)
        vis_srv.request.ids.push_back(ids[i]);

    visualize_task_geometries_clt_.call(vis_srv);
    if(!vis_srv.response.success)
    {
        ROS_ERROR("GraspingExperiments::setStateTasks(): could not start visualization!");
        return false;
    }
    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::sendStateTasks()
{
    //sends the tasks to the controller
    set_tasks_clt_.call(tasks_);
    if(!tasks_.response.success)
    {
        ROS_ERROR("GraspingExperiments::setStateTasks(): could not set tasks!");
        return false;
    }
    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::setJointConfiguration(std::vector<double> const& joints)
{
#ifdef HQP_GRIPPER_JOINT
    ROS_ASSERT(joints.size() == 8);//7 joints for the lbr iiwa + 1 velvet fingers joint
#else
    ROS_ASSERT(joints.size() == 7);
#endif

    hqp_controllers_msgs::Task task;
    hqp_controllers_msgs::TaskLink t_link;
    hqp_controllers_msgs::TaskGeometry t_geom;

#if 0
    //LINK 4 ABOVE HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "lwr_link_4_above_horizontal_plane (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 3);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(SAFETY_HEIGHT);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0.12);
    t_link.link_frame = "lwr_link_4";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //LINK 5 ABOVE HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "lwr_link_5_above_horizontal_plane (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 3);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(SAFETY_HEIGHT);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0.075);
    t_link.link_frame = "lwr_link_5";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //LINK 6 ABOVE HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "lwr_link_6_above_horizontal_plane (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 3);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(SAFETY_HEIGHT);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0.1);
    t_link.link_frame = "lwr_link_6";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //PALM ABOVE HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "velvet_fingers_palm_above_horizontal_plane (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 3);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(SAFETY_HEIGHT);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0.04); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0.025);
    t_geom.g_data.push_back(0.11);
    t_link.link_frame = "velvet_fingers_palm";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

  //POINT BEHIND VERTICAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "point_behind_vertical_plane (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 3);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(-1); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(-grasp_.p_(0));
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(0.265); t_geom.g_data.push_back(0); t_geom.g_data.push_back(-0.0675);
    t_link.link_frame = "velvet_fingers_palm";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

      //PALM ABOVE HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "velvet_fingers_palm_above_horizontal_plane (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 3);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(SAFETY_HEIGHT);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0.04); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0.025);
    t_geom.g_data.push_back(0.11);
    t_link.link_frame = "velvet_fingers_palm";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

      //RIGHT FINGER ABOVE HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "right_finger_above_horizontal_plane (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN );

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(0.2);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0.11); t_geom.g_data.push_back(-0.11); t_geom.g_data.push_back(-0.02);
    t_geom.g_data.push_back(0.06);
    t_link.link_frame = "velvet_fingers_palm";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

     //LEFT FINGER ABOVE HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "velvet_fingers_palm_above_horizontal_plane (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN );

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(0.2);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0.11); t_geom.g_data.push_back(0.11); t_geom.g_data.push_back(-0.02);
    t_geom.g_data.push_back(0.06);
    t_link.link_frame = "velvet_fingers_palm";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

   //RIGHT FINGER BEER AVOIDANCE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "right_finger_avoid_beer (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.5;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 0.5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0.11); t_geom.g_data.push_back(-0.11); t_geom.g_data.push_back(-0.02);
    t_geom.g_data.push_back(0.06);
    t_link.link_frame = "velvet_fingers_palm";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(grasp_.p_(0)+BEER_RADIUS); t_geom.g_data.push_back(-0.9); t_geom.g_data.push_back(BEER_HEIGHT);
    t_geom.g_data.push_back(BEER_RADIUS);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

   //LEFT FINGER BEER AVOIDANCE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "right_finger_avoid_beer (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.5;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 0.5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0.11); t_geom.g_data.push_back(0.11); t_geom.g_data.push_back(-0.02);
    t_geom.g_data.push_back(0.06);
    t_link.link_frame = "velvet_fingers_palm";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(grasp_.p_(0)+BEER_RADIUS); t_geom.g_data.push_back(-0.9); t_geom.g_data.push_back(BEER_HEIGHT);
    t_geom.g_data.push_back(BEER_RADIUS);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

   //LINK 4 BEER AVOIDANCE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "lwr_link_4_avoid_beer (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.5;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 0.5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0.12);
    t_link.link_frame = "lwr_link_4";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(grasp_.p_(0)+BEER_RADIUS); t_geom.g_data.push_back(-0.9); t_geom.g_data.push_back(BEER_HEIGHT);
    t_geom.g_data.push_back(BEER_RADIUS);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //LINK 5 BEER AVOIDANCE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "lwr_link_5_avoid_beer (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.5;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 0.5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0.075);
    t_link.link_frame = "lwr_link_5";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(grasp_.p_(0)+BEER_RADIUS); t_geom.g_data.push_back(-0.9); t_geom.g_data.push_back(BEER_HEIGHT);
    t_geom.g_data.push_back(BEER_RADIUS);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //LINK 6 BEER AVOIDANCE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "lwr_link_6_avoid_beer (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.5;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 0.5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0.1);
    t_link.link_frame = "lwr_link_6";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(grasp_.p_(0)+BEER_RADIUS); t_geom.g_data.push_back(-0.9); t_geom.g_data.push_back(BEER_HEIGHT);
    t_geom.g_data.push_back(BEER_RADIUS);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //PALM BEER AVOIDANCE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "velvet_fingers_palm_avoid_beer (joint configuration)";
    task.is_equality_task = false;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 0.5;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 0.5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(0.04); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0.025);
    t_geom.g_data.push_back(0.11);
    t_link.link_frame = "velvet_fingers_palm";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::SPHERE;
    t_geom.g_data.push_back(grasp_.p_(0)+BEER_RADIUS); t_geom.g_data.push_back(-0.9); t_geom.g_data.push_back(BEER_HEIGHT);
    t_geom.g_data.push_back(BEER_RADIUS);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);
#endif

    //SET JOINT VALUES
    t_link.geometries.resize(1);
    t_geom.g_data.resize(1);
    task.t_links.clear();
    task.dynamics.d_data.clear();
    task.t_type = hqp_controllers_msgs::Task::JOINT_SETPOINT;
    task.priority = 3;
    task.name = "joint_setpoints";
    task.is_equality_task = true;
    task.task_frame = "world";
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::JOINT_POSITION;

    t_geom.g_data[0] = joints[0];
    t_link.geometries[0] = t_geom;
    t_link.link_frame = "lwr_1_link";
    task.t_links.push_back(t_link);

    t_geom.g_data[0] = joints[1];
    t_link.geometries[0] = t_geom;
    t_link.link_frame = "lwr_2_link";
    task.t_links.push_back(t_link);

    t_geom.g_data[0] = joints[2];
    t_link.geometries[0] = t_geom;
    t_link.link_frame = "lwr_3_link";
    task.t_links.push_back(t_link);

    t_geom.g_data[0] = joints[3];
    t_link.geometries[0] = t_geom;
    t_link.link_frame = "lwr_4_link";
    task.t_links.push_back(t_link);

    t_geom.g_data[0] = joints[4];
    t_link.geometries[0] = t_geom;
    t_link.link_frame = "lwr_5_link";
    task.t_links.push_back(t_link);

    t_geom.g_data[0] = joints[5];
    t_link.geometries[0] = t_geom;
    t_link.link_frame = "lwr_6_link";
    task.t_links.push_back(t_link);

    t_geom.g_data[0] = joints[6];
    t_link.geometries[0] = t_geom;
    t_link.link_frame = "lwr_7_link";
    task.t_links.push_back(t_link);

#ifdef HQP_GRIPPER_JOINT
    t_geom.g_data[0] = joints[7];
    t_link.geometries[0] = t_geom;
    t_link.link_frame = "velvet_fingers_right";
    task.t_links.push_back(t_link);
#endif

    tasks_.request.tasks.push_back(task);

    //send the filled task message to the controller
    if(!sendStateTasks())
        return false;

    //monitor only the last task
    monitored_tasks_.push_back(tasks_.response.ids.back());

    //visualize all tasks except of the last one
    std::vector<unsigned int> ids = pers_task_vis_ids_;
    for(unsigned int i=0; i<tasks_.response.ids.size() - 1;i++)
        ids.push_back(tasks_.response.ids[i]);

    if(!visualizeStateTasks(ids))
        return false;

    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::setObjectPlace(PlaceInterval const& place)
{
    hqp_controllers_msgs::Task task;
    hqp_controllers_msgs::TaskLink t_link;
    hqp_controllers_msgs::TaskGeometry t_geom;

    //EE ON HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "ee_on_horizontal_plane (place)";
    task.is_equality_task = true;
    task.task_frame = place.place_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(place.n_(0)); t_geom.g_data.push_back(place.n_(1)); t_geom.g_data.push_back(place.n_(2));
    t_geom.g_data.push_back(place.d_);
    t_link.link_frame = place.place_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(place.e_(0)); t_geom.g_data.push_back(place.e_(1)); t_geom.g_data.push_back(place.e_(2));
    t_link.link_frame = place.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //PLACEMENT_CYLINDER
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "ee_in_placement_cylinder (place)";
    task.is_equality_task = false;
    task.task_frame = place.place_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(place.e_(0)); t_geom.g_data.push_back(place.e_(1)); t_geom.g_data.push_back(place.e_(2));
    t_link.link_frame = place.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CYLINDER;
    t_geom.g_data.push_back(place.p_(0)); t_geom.g_data.push_back(place.p_(1)); t_geom.g_data.push_back(place.p_(2));
    t_geom.g_data.push_back(place.v_(0)); t_geom.g_data.push_back(place.v_(1)); t_geom.g_data.push_back(place.v_(2));
    t_geom.g_data.push_back(place.r_);
    t_link.link_frame = "world";
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //GRIPPER APPROACH AXIS ALIGNMENT
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.name = "gripper_approach_axis_alignment";
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back( DYNAMICS_GAIN / 6);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(-0.707); t_geom.g_data.push_back(0.707); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(ALIGNMENT_ANGLE * 10);
    t_link.link_frame = place.place_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(1); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //GRIPPER VERTICAL AXIS ALIGNMENT
    task.t_links.clear();
    task.dynamics.d_data.clear();
    task.name = "gripper_vertical_axis_alignment";
    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 2);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(0.0);
    t_link.link_frame = place.place_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //send the filled task message to the controller
    if(!sendStateTasks())
        return false;

    //monitor all tasks
    for(unsigned int i=0; i<tasks_.response.ids.size();i++)
        monitored_tasks_.push_back(tasks_.response.ids[i]);

    //visualize all tasks
    std::vector<unsigned int> ids = pers_task_vis_ids_;
    for(unsigned int i=0; i<tasks_.response.ids.size();i++)
        ids.push_back(tasks_.response.ids[i]);

    if(!visualizeStateTasks(ids))
        return false;

    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::setObjectExtract()
{
    hqp_controllers_msgs::Task task;
    hqp_controllers_msgs::TaskLink t_link;
    hqp_controllers_msgs::TaskGeometry t_geom;


    //EE ON ATTACK POINT
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "ee_on_attack_point";
    task.is_equality_task = true;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 1);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(grasp_.p_(0) - grasp_.a_(0)*0.25); t_geom.g_data.push_back(grasp_.p_(1) - grasp_.a_(1)*0.2); t_geom.g_data.push_back(grasp_.p_(2) + 0.2);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(grasp_.e_(0)); t_geom.g_data.push_back(grasp_.e_(1)); t_geom.g_data.push_back(grasp_.e_(2));
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //GRIPPER APPROACH AXIS ALIGNMENT
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.name = "gripper_approach_axis_alignment";
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 4);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(grasp_.p_(0)); t_geom.g_data.push_back(grasp_.p_(1)); t_geom.g_data.push_back(grasp_.p_(2));
    //project the approach vector on the x/y plane
    Eigen::Vector3d a;
    a.setZero();
    a(0) = grasp_.a_(0); a(1) = grasp_.a_(1);
    a.normalize();
    t_geom.g_data.push_back(a(0)); t_geom.g_data.push_back(a(1)); t_geom.g_data.push_back(a(2));
    t_geom.g_data.push_back(ALIGNMENT_ANGLE);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(1); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //GRIPPER VERTICAL AXIS ALIGNMENT
    task.t_links.clear();
    task.dynamics.d_data.clear();
    task.name = "gripper_vertical_axis_alignment";
    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(ALIGNMENT_ANGLE / 4);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //send the filled task message to the controller
    if(!sendStateTasks())
        return false;

    //monitor all tasks
    for(unsigned int i=0; i<tasks_.response.ids.size();i++)
        monitored_tasks_.push_back(tasks_.response.ids[i]);

    //visualize all tasks
    std::vector<unsigned int> ids = pers_task_vis_ids_;
    for(unsigned int i=0; i<tasks_.response.ids.size();i++)
        ids.push_back(tasks_.response.ids[i]);

    if(!visualizeStateTasks(ids))
        return false;

    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::setGripperExtract(PlaceInterval const& place)
{
    hqp_controllers_msgs::Task task;
    hqp_controllers_msgs::TaskLink t_link;
    hqp_controllers_msgs::TaskGeometry t_geom;


    //EE ON ATTACK POINT
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "ee_on_attack_point";
    task.is_equality_task = true;
    task.task_frame = place.place_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 1.5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(place.p_(0)); t_geom.g_data.push_back(place.p_(1) - 0.15); t_geom.g_data.push_back(0.6);
    t_link.link_frame = place.place_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(place.e_(0)); t_geom.g_data.push_back(place.e_(1)); t_geom.g_data.push_back(place.e_(2));
    t_link.link_frame = place.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //GRIPPER APPROACH AXIS ALIGNMENT
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.name = "gripper_approach_axis_alignment";
    task.is_equality_task = false;
    task.task_frame = place.place_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 6);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(place.p_(0)); t_geom.g_data.push_back(place.p_(1)); t_geom.g_data.push_back(place.p_(2));
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(1); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(ALIGNMENT_ANGLE * 10);
    t_link.link_frame = place.place_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(1); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_link.link_frame = place.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);
#if 0
    //GRIPPER VERTICAL AXIS ALIGNMENT
    task.t_links.clear();
    task.dynamics.d_data.clear();
    task.name = "gripper_vertical_axis_alignment";
    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = place.place_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(ALIGNMENT_ANGLE / 4);
    t_link.link_frame = place.place_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_link.link_frame = place.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);
#endif

    //send the filled task message to the controller
    if(!sendStateTasks())
        return false;

    //monitor all tasks
    for(unsigned int i=0; i<tasks_.response.ids.size();i++)
        monitored_tasks_.push_back(tasks_.response.ids[i]);

    //visualize all tasks
    std::vector<unsigned int> ids = pers_task_vis_ids_;
    for(unsigned int i=0; i<tasks_.response.ids.size();i++)
        ids.push_back(tasks_.response.ids[i]);

    if(!visualizeStateTasks(ids))
        return false;

    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::setGraspApproach()
{
    hqp_controllers_msgs::Task task;
    hqp_controllers_msgs::TaskLink t_link;
    hqp_controllers_msgs::TaskGeometry t_geom;

#ifdef PILE_GRASPING
    ROS_ASSERT(grasp_.p_(0) <= 0.0);
    ROS_ASSERT(grasp_.p_(2) >= 1.06);
    ROS_ASSERT(grasp_.a_(1) < 0.0);

    //EE ON HORIZONTAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "ee_on_horizontal_plane";
    task.is_equality_task = true;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(0.0); t_geom.g_data.push_back(0.0); t_geom.g_data.push_back(1.0);
    t_geom.g_data.push_back(grasp_.p_(2));
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(grasp_.e_(0)); t_geom.g_data.push_back(grasp_.e_(1)); t_geom.g_data.push_back(grasp_.e_(2));
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //CONSTRAINT CYLINDER
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.name = "ee_in_constraint_cylinder";
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN * 3/2);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(grasp_.e_(0)); t_geom.g_data.push_back(grasp_.e_(1)); t_geom.g_data.push_back(grasp_.e_(2));
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CYLINDER;
    t_geom.g_data.push_back(grasp_.p_(0)); t_geom.g_data.push_back(grasp_.p_(1)); t_geom.g_data.push_back(0.16);
    t_geom.g_data.push_back(0.0); t_geom.g_data.push_back(0.0); t_geom.g_data.push_back(1.0);
    t_geom.g_data.push_back(0.005);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //GRIPPER APPROACH AXIS ALIGNMENT
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.name = "gripper_approach_axis_alignment";
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(grasp_.p_(0)); t_geom.g_data.push_back(grasp_.p_(1)); t_geom.g_data.push_back(grasp_.p_(2));
    //project the approach vector on the x/y plane
    Eigen::Vector3d a;
    a.setZero();
    a(0) = grasp_.a_(0); a(1) = grasp_.a_(1);
    a.normalize();
    t_geom.g_data.push_back(a(0)); t_geom.g_data.push_back(a(1)); t_geom.g_data.push_back(a(2));
    t_geom.g_data.push_back(ALIGNMENT_ANGLE);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(1); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //GRIPPER VERTICAL AXIS ALIGNMENT
    task.t_links.clear();
    task.dynamics.d_data.clear();
    task.name = "gripper_vertical_axis_alignment";
    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 1;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(ALIGNMENT_ANGLE);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

#else
    ROS_ASSERT(grasp_.r1_ <= grasp_.r2_);
    ROS_ASSERT(grasp_.n1_.transpose() * grasp_.n2_ < 0.0);

    //LOWER GRASP INTERVAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.02;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN / 5);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(grasp_.n1_(0)); t_geom.g_data.push_back(grasp_.n1_(1)); t_geom.g_data.push_back(grasp_.n1_(2));
    t_geom.g_data.push_back(grasp_.d1_);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(grasp_.e_(0)); t_geom.g_data.push_back(grasp_.e_(1)); t_geom.g_data.push_back(grasp_.e_(2));
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //UPPER GRASP INTERVAL PLANE
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::PLANE;
    t_geom.g_data.push_back(grasp_.n2_(0)); t_geom.g_data.push_back(grasp_.n2_(1)); t_geom.g_data.push_back(grasp_.n2_(2));
    t_geom.g_data.push_back(grasp_.d2_);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(grasp_.e_(0)); t_geom.g_data.push_back(grasp_.e_(1)); t_geom.g_data.push_back(grasp_.e_(2));
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //INNER CONSTRAINT CYLINDER
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CYLINDER;
    t_geom.g_data.push_back(grasp_.p_(0)); t_geom.g_data.push_back(grasp_.p_(1)); t_geom.g_data.push_back(grasp_.p_(2));
    t_geom.g_data.push_back(grasp_.v_(0)); t_geom.g_data.push_back(grasp_.v_(1)); t_geom.g_data.push_back(grasp_.v_(2));
    t_geom.g_data.push_back(grasp_.r1_);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(grasp_.e_(0)); t_geom.g_data.push_back(grasp_.e_(1)); t_geom.g_data.push_back(grasp_.e_(2));
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //OUTER CONSTRAINT CYLINDER
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PROJECTION;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::POINT;
    t_geom.g_data.push_back(grasp_.e_(0)); t_geom.g_data.push_back(grasp_.e_(1)); t_geom.g_data.push_back(grasp_.e_(2));
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CYLINDER;
    t_geom.g_data.push_back(grasp_.p_(0)); t_geom.g_data.push_back(grasp_.p_(1)); t_geom.g_data.push_back(grasp_.p_(2));
    t_geom.g_data.push_back(grasp_.v_(0)); t_geom.g_data.push_back(grasp_.v_(1)); t_geom.g_data.push_back(grasp_.v_(2));
    t_geom.g_data.push_back(grasp_.r2_);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //COPLANAR LINES CONSTRAINT
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::COPLANAR;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(2 * DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(grasp_.p_(0)); t_geom.g_data.push_back(grasp_.p_(1)); t_geom.g_data.push_back(grasp_.p_(2));
    t_geom.g_data.push_back(grasp_.v_(0)); t_geom.g_data.push_back(grasp_.v_(1)); t_geom.g_data.push_back(grasp_.v_(2));
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(1); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(ALIGNMENT_ANGLE);
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);

    //CONE CONSTRAINT
    task.t_links.clear();
    task.dynamics.d_data.clear();

    task.t_type = hqp_controllers_msgs::Task::PARALLEL;
    task.priority = 2;
    task.is_equality_task = false;
    task.task_frame = grasp_.obj_frame_;
    task.ds = 0.0;
    task.di = 0.05;
    task.dynamics.d_type = hqp_controllers_msgs::TaskDynamics::LINEAR_DYNAMICS;
    task.dynamics.d_data.push_back(4 * DYNAMICS_GAIN);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::CONE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_geom.g_data.push_back(ALIGNMENT_ANGLE);
    t_link.link_frame = grasp_.obj_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    t_link.geometries.clear();
    t_geom.g_data.clear();
    t_geom.g_type = hqp_controllers_msgs::TaskGeometry::LINE;
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(0);
    t_geom.g_data.push_back(0); t_geom.g_data.push_back(0); t_geom.g_data.push_back(1);
    t_link.link_frame = grasp_.e_frame_;
    t_link.geometries.push_back(t_geom);
    task.t_links.push_back(t_link);

    tasks_.request.tasks.push_back(task);
#endif

    //send the filled task message to the controller
    if(!sendStateTasks())
        return false;

    //monitor all tasks
    for(unsigned int i=0; i<tasks_.response.ids.size();i++)
        monitored_tasks_.push_back(tasks_.response.ids[i]);

    //visualize all tasks
    std::vector<unsigned int> ids = pers_task_vis_ids_;
    for(unsigned int i=0; i<tasks_.response.ids.size();i++)
        ids.push_back(tasks_.response.ids[i]);

    if(!visualizeStateTasks(ids))
        return false;

    return true;
}
//-----------------------------------------------------------------
void GraspingExperiments::taskStatusCallback( const hqp_controllers_msgs::TaskStatusArrayPtr& msg)
{
    boost::mutex::scoped_lock lock(manipulator_tasks_m_, boost::try_to_lock);
    if(!lock) return;

    static struct timeval t_stag;
    struct timeval t;
    gettimeofday(&t,0);
    static bool stagnation_flag = false;

    if(!stagnation_flag)
        t_stag = t;

    stagnation_flag = false;
    // double curr = t.tv_sec + 0.000001*t.tv_usec;
    // double stag = t_stag.tv_sec + 0.000001*t_stag.tv_usec;

    // std::cerr<<"t: "<<curr<<std::endl;
    // std::cerr<<"t_stag: "<<stag<<std::endl;

    // std::cerr<<"monitored tasks: ";
    // for(unsigned int i=0; i<monitored_tasks_.size(); i++)
    //   std::cerr<<monitored_tasks_[i]<<" ";

    // std::cerr<<std::endl;
    // std::cerr<<"received tasks: ";
    // std::vector<hqp_controllers_msgs::TaskStatus>::const_iterator status_it2;
    // for(status_it2 = msg->statuses.begin(); status_it2!=msg->statuses.end(); ++status_it2)
    // 	std::cerr<<status_it2->id<<" "<<status_it2->name<<" "<<std::endl;

    // std::cerr<<std::endl;

    Eigen::VectorXd t_prog(monitored_tasks_.size());

    //form the maximum norm over all errors

    for(unsigned int i=0; i<monitored_tasks_.size(); i++)
    {
        //try to find the monitored task id in the given task status message
        std::vector<hqp_controllers_msgs::TaskStatus>::const_iterator status_it;
        for(status_it = msg->statuses.begin(); status_it!=msg->statuses.end(); ++status_it)
            if(monitored_tasks_[i] == status_it->id)
            {
                t_prog(i)=status_it->progress;
                break;
            }

        if(status_it==msg->statuses.end())
        {
            ROS_WARN("No status feedback for monitored task id %d!", monitored_tasks_[i]);
            return; //just so we don't give a false positive task success
        }

    }

    double e = 0.0;
    double e_diff = INFINITY;


    if(monitored_tasks_.size() > 0)
    {
        //task error
        e = t_prog.cwiseAbs().maxCoeff();

        //find the task progress difference between iterations
        if(t_prog_prev_.size() > 0)
            e_diff = (t_prog - t_prog_prev_).cwiseAbs().maxCoeff();

        //std::cerr<<"t_prog: "<<t_prog.transpose()<<"e: "<<e<<std::endl;
        //std::cerr<<"t_prog_prev_: "<<t_prog_prev_.transpose()<<"e_diff: "<<e_diff<<std::endl;
        t_prog_prev_ = t_prog;
    }
    //  std::cout<<" t - t_stag: "<<t.tv_sec - t_stag.tv_sec + 0.000001 * (t.tv_usec - t_stag.tv_usec)<<" task_timeout_tol_: "<<task_timeout_tol_<<std::endl;
    //std::cout<<"e_diff: "<<e_diff<<" e_diff_tol_: "<<task_diff_tol_<<std::endl;

    if(e <= task_error_tol_)
    {
        std::cerr<<std::endl<<"STATE CHANGE:"<<std::endl<<"monitored tasks: ";
        for(unsigned int i=0; i<monitored_tasks_.size(); i++)
            std::cerr<<monitored_tasks_[i]<<" ";

        std::cerr<<std::endl<<"task statuses: "<<std::endl;
        for( std::vector<hqp_controllers_msgs::TaskStatus>::iterator it = msg->statuses.begin(); it!=msg->statuses.end(); ++it)
            std::cerr<<"id: "<<it->id<<" name: "<<it->name<<" progress: "<<it->progress<<std::endl;

        std::cerr<<"e: "<<e<<std::endl<<std::endl;

        // ROS_INFO("Task status switch!");
        task_status_changed_ = true;
        task_success_ = true;
        cond_.notify_one();
    }
    else if(e_diff <= task_diff_tol_) //(task progresses ain't changing no more)
    {
        stagnation_flag = true;
        std::cerr<<"task progress stagnating since:"<<t.tv_sec - t_stag.tv_sec + 0.000001 * (t.tv_usec - t_stag.tv_usec)<<" s, e_diff is: "<<e_diff<<std::endl;
        if((t.tv_sec - t_stag.tv_sec + 0.000001 * (t.tv_usec - t_stag.tv_usec)) > task_timeout_tol_ )
        {
            task_status_changed_ = true;
            task_success_ = true;
            ROS_WARN("Task execution timeout!");
            std::cerr<<"monitored tasks: ";
            for(unsigned int i=0; i<monitored_tasks_.size(); i++)
                std::cerr<<monitored_tasks_[i]<<" ";

            std::cerr<<std::endl<<"task statuses: "<<std::endl;
            for( std::vector<hqp_controllers_msgs::TaskStatus>::iterator it = msg->statuses.begin(); it!=msg->statuses.end(); ++it)
                std::cerr<<"id: "<<it->id<<" name: "<<it->name<<" progress: "<<it->progress<<std::endl;

            std::cerr<<"e: "<<e<<std::endl<<std::endl;
            //std::cerr<<"t: "<<"t - t_stag: "<<t.tv_sec - t_stag.tv_sec + 0.000001 * (t.tv_usec - t_stag.tv_usec)<<" task_timeout_tol_: "<<task_timeout_tol_<<std::endl;
            //std::cerr<<"e_diff: "<<e_diff<<" e_diff_tol_: "<<task_diff_tol_<<std::endl<<std::endl;

            stagnation_flag = false;
            cond_.notify_one();
        }
    }

}
//-----------------------------------------------------------------
void GraspingExperiments::jointStateCallback( const sensor_msgs::JointStatePtr& msg)
{
    boost::mutex::scoped_lock lock(force_change_m_, boost::try_to_lock);
    if(!lock) return;

    static sensor_msgs::JointState prev_state = *msg;



    //    static struct timeval t_stag;
    //    struct timeval t;
    //    gettimeofday(&t,0);
    //    static bool stagnation_flag = false;

    //    if(!stagnation_flag)
    //        t_stag = t;

    //    stagnation_flag = false;
    //    // double curr = t.tv_sec + 0.000001*t.tv_usec;
    //    // double stag = t_stag.tv_sec + 0.000001*t_stag.tv_usec;

    //    // std::cerr<<"t: "<<curr<<std::endl;
    //    // std::cerr<<"t_stag: "<<stag<<std::endl;

    //    // std::cerr<<"monitored tasks: ";
    //    // for(unsigned int i=0; i<monitored_tasks_.size(); i++)
    //    //   std::cerr<<monitored_tasks_[i]<<" ";

    //    // std::cerr<<std::endl;
    //    // std::cerr<<"received tasks: ";
    //    // std::vector<hqp_controllers_msgs::TaskStatus>::const_iterator status_it2;
    //    // for(status_it2 = msg->statuses.begin(); status_it2!=msg->statuses.end(); ++status_it2)
    //    // 	std::cerr<<status_it2->id<<" "<<status_it2->name<<" "<<std::endl;

    //    // std::cerr<<std::endl;

    //    Eigen::VectorXd t_prog(monitored_tasks_.size());

    //    //form the maximum norm over all errors

    //    for(unsigned int i=0; i<monitored_tasks_.size(); i++)
    //    {
    //        //try to find the monitored task id in the given task status message
    //        std::vector<hqp_controllers_msgs::TaskStatus>::const_iterator status_it;
    //        for(status_it = msg->statuses.begin(); status_it!=msg->statuses.end(); ++status_it)
    //            if(monitored_tasks_[i] == status_it->id)
    //            {
    //                t_prog(i)=status_it->progress;
    //                break;
    //            }

    //        if(status_it==msg->statuses.end())
    //        {
    //            ROS_WARN("No status feedback for monitored task id %d!", monitored_tasks_[i]);
    //            return; //just so we don't give a false positive task success
    //        }

    //    }

    //    double e = 0.0;
    //    double e_diff = INFINITY;


    //    if(monitored_tasks_.size() > 0)
    //    {
    //        //task error
    //        e = t_prog.cwiseAbs().maxCoeff();

    //        //find the task progress difference between iterations
    //        if(t_prog_prev_.size() > 0)
    //            e_diff = (t_prog - t_prog_prev_).cwiseAbs().maxCoeff();

    //        //std::cerr<<"t_prog: "<<t_prog.transpose()<<"e: "<<e<<std::endl;
    //        //std::cerr<<"t_prog_prev_: "<<t_prog_prev_.transpose()<<"e_diff: "<<e_diff<<std::endl;
    //        t_prog_prev_ = t_prog;
    //    }
    //    //  std::cout<<" t - t_stag: "<<t.tv_sec - t_stag.tv_sec + 0.000001 * (t.tv_usec - t_stag.tv_usec)<<" task_timeout_tol_: "<<task_timeout_tol_<<std::endl;
    //    //std::cout<<"e_diff: "<<e_diff<<" e_diff_tol_: "<<task_diff_tol_<<std::endl;

    //    if(e <= task_error_tol_)
    //    {
    //        std::cerr<<std::endl<<"STATE CHANGE:"<<std::endl<<"monitored tasks: ";
    //        for(unsigned int i=0; i<monitored_tasks_.size(); i++)
    //            std::cerr<<monitored_tasks_[i]<<" ";

    //        std::cerr<<std::endl<<"task statuses: "<<std::endl;
    //        for( std::vector<hqp_controllers_msgs::TaskStatus>::iterator it = msg->statuses.begin(); it!=msg->statuses.end(); ++it)
    //            std::cerr<<"id: "<<it->id<<" name: "<<it->name<<" progress: "<<it->progress<<std::endl;

    //        std::cerr<<"e: "<<e<<std::endl<<std::endl;

    //        // ROS_INFO("Task status switch!");
    //        task_status_changed_ = true;
    //        task_success_ = true;
    //        cond_.notify_one();
    //    }
    //    else if(e_diff <= task_diff_tol_) //(task progresses ain't changing no more)
    //    {
    //        stagnation_flag = true;
    //        std::cerr<<"task progress stagnating since:"<<t.tv_sec - t_stag.tv_sec + 0.000001 * (t.tv_usec - t_stag.tv_usec)<<" s, e_diff is: "<<e_diff<<std::endl;
    //        if((t.tv_sec - t_stag.tv_sec + 0.000001 * (t.tv_usec - t_stag.tv_usec)) > task_timeout_tol_ )
    //        {
    //            task_status_changed_ = true;
    //            task_success_ = true;
    //            ROS_WARN("Task execution timeout!");
    //            std::cerr<<"monitored tasks: ";
    //            for(unsigned int i=0; i<monitored_tasks_.size(); i++)
    //                std::cerr<<monitored_tasks_[i]<<" ";

    //            std::cerr<<std::endl<<"task statuses: "<<std::endl;
    //            for( std::vector<hqp_controllers_msgs::TaskStatus>::iterator it = msg->statuses.begin(); it!=msg->statuses.end(); ++it)
    //                std::cerr<<"id: "<<it->id<<" name: "<<it->name<<" progress: "<<it->progress<<std::endl;

    //            std::cerr<<"e: "<<e<<std::endl<<std::endl;
    //            //std::cerr<<"t: "<<"t - t_stag: "<<t.tv_sec - t_stag.tv_sec + 0.000001 * (t.tv_usec - t_stag.tv_usec)<<" task_timeout_tol_: "<<task_timeout_tol_<<std::endl;
    //            //std::cerr<<"e_diff: "<<e_diff<<" e_diff_tol_: "<<task_diff_tol_<<std::endl<<std::endl;

    //            stagnation_flag = false;
    //            cond_.notify_one();
    //        }
    //    }

    prev_state = *msg;

}
//-----------------------------------------------------------------
bool GraspingExperiments::loadPersistentTasks()
{
    hqp_controllers_msgs::LoadTasks persistent_tasks;
    persistent_tasks.request.task_definitions = "task_definitions";
    if(!load_tasks_clt_.call(persistent_tasks))
        return false;

    //visualize (some of) the loaded tasks
    for(unsigned int i=0; i<6;i++)
        pers_task_vis_ids_.push_back(persistent_tasks.response.ids[i+7]);

    if(!visualizeStateTasks(pers_task_vis_ids_))
        return false;

    return true;
}
//-----------------------------------------------------------------
bool GraspingExperiments::startDemo(std_srvs::Empty::Request  &req, std_srvs::Empty::Response &res )
{
    std_srvs::Empty srv;
    //PICK EMPTY PALLET
    ROS_INFO("Picking up empty pallet");
    next_truck_task_clt_.call(srv);
    //MOVE TO UNLOADING POSE
    ROS_INFO("Moving to unloading pose");
    next_truck_task_clt_.call(srv);


#if 0
#endif
    deactivateHQPControl();
    resetState();
    reset_hqp_control_clt_.call(srv);
    pers_task_vis_ids_.clear();

    if(!loadPersistentTasks())
    {
        ROS_ERROR("Could not load persistent tasks!");
        safeShutdown();
        return false;
    }

    if(!with_gazebo_)
    {
        //VELVET INITIAL POSE
        velvet_interface_node::VelvetToPos poscall;
        poscall.request.angle = 0.3;

        if(!velvet_pos_clt_.call(poscall))
        {
            ROS_ERROR("could not call velvet to pos");
            ROS_BREAK();
        }
    }

    for(unsigned int i=0; i<place_zones_.size(); i++)
    {
        bool grasp_success = false;
        while(!grasp_success)
        {
            {//MANIPULATOR SENSING CONFIGURATION
                ROS_INFO("Trying to put the manipulator in sensing configuration.");
                boost::mutex::scoped_lock lock(manipulator_tasks_m_);
                task_status_changed_ = false;
                task_success_ = false;
                deactivateHQPControl();
                if(!resetState())
                {
                    ROS_ERROR("Could not reset the state!");
                    safeShutdown();
                    return false;
                }
                if(!setCartesianStiffness(1000, 1000, 1000, 100, 100, 100))
                {
                    safeShutdown();
                    return false;
                }

                if(!setJointConfiguration(sensing_config_))
                {
                    ROS_ERROR("Could not set manipulator sensing state!");
                    safeShutdown();
                    return false;
                }
                task_error_tol_ = 1e-2;
                activateHQPControl();

                while(!task_status_changed_)
                    cond_.wait(lock);

                if(!task_success_)
                {
                    ROS_ERROR("Could not complete the manipulator sensing state tasks!");
                    safeShutdown();
                    return false;
                }
                ROS_INFO("Manipulator sensing state tasks executed successfully.");
            }

            {//GRASP APPROACH
                ROS_INFO("Trying grasp approach.");
                boost::mutex::scoped_lock lock(manipulator_tasks_m_);
                task_status_changed_ = false;
                task_success_ = false;
                deactivateHQPControl();
                if(!resetState())
                {
                    ROS_ERROR("Could not reset the state!");
                    safeShutdown();
                    return false;
                }
		 if(!with_gazebo_)
                    if(!getGraspInterval())
                        ROS_WARN("Could not obtain the grasp intervall - using default interval!");

#if 0
#endif

                if(!setCartesianStiffness(1000, 1000, 100, 100, 100, 100))
                {
                    safeShutdown();
                    return false;
                }

                if(!setGraspApproach())
                {
                    ROS_ERROR("Could not set the grasp approach!");
                    safeShutdown();
                    return false;
                }
                task_error_tol_ =  1e-3;
                activateHQPControl();

                while(!task_status_changed_)
                    cond_.wait(lock);

                if(!task_success_)
                {
                    ROS_ERROR("Could not complete the grasp approach tasks!");
                    safeShutdown();
                    return false;
                }

                ROS_INFO("Grasp approach tasks executed successfully.");
            }


            if(!with_gazebo_)
            {
                //SET GRASP STIFFNESS
                if(!setCartesianStiffness(1000, 50, 30, 100, 100, 10))
                {
                    safeShutdown();
                    return false;
                }

                deactivateHQPControl();
                //VELVET GRASP_
                velvet_interface_node::SmartGrasp graspcall;
                graspcall.request.current_threshold_contact = 20;
                graspcall.request.current_threshold_final = 35;
                graspcall.request.max_belt_travel_mm = 90;
                graspcall.request.phalange_delta_rad = 0.02;
                graspcall.request.gripper_closed_thresh = 1.5;
                graspcall.request.check_phalanges = true;

                if(!velvet_grasp_clt_.call(graspcall)) {
                    ROS_ERROR("could not call grasping");
                    ROS_BREAK();
                }
                if(!graspcall.response.success)
                    ROS_ERROR("Grasp failed!");
                else
                {
                    grasp_success = true;
                    ROS_INFO("Grasp aquired.");
                }
#if 0
		grasp_success = true; //RRRRRRRRREEEEEEEEEEEMMMMMMMMMOOOOOOVVVVVVEEEEEEEE!!!!!!!!!!
#endif
            }
            else
                grasp_success = true;

        }
        {//OBJECT EXTRACT
            ROS_INFO("Trying object extract.");
            boost::mutex::scoped_lock lock(manipulator_tasks_m_);
            task_status_changed_ = false;
            task_success_ = false;
            deactivateHQPControl();
            if(!resetState())
            {
                ROS_ERROR("Could not reset the state!");
                safeShutdown();
                return false;
            }
            if(!setCartesianStiffness(1000, 1000, 1000, 100, 100, 100))
            {
                safeShutdown();
                return false;
            }
            if(!setObjectExtract())
            {
                ROS_ERROR("Could not set the object extract!");
                safeShutdown();
                return false;
            }

            task_error_tol_ = 1e-2;
            activateHQPControl();

            while(!task_status_changed_)
                cond_.wait(lock);

            if(!task_success_)
            {
                ROS_ERROR("Could not complete the object extract tasks!");
                safeShutdown();
                return false;
            }
            ROS_INFO("Object extract tasks executed successfully.");
        }

        {//OBJECT TRANSFER
            ROS_INFO("Trying object transfer configuration.");

            boost::mutex::scoped_lock lock(manipulator_tasks_m_);
            task_status_changed_ = false;
            task_success_ = false;
            deactivateHQPControl();
            if(!resetState())
            {
                ROS_ERROR("Could not reset the state!");
                safeShutdown();
                return false;
            }
            if(!setCartesianStiffness(1000, 1000, 1000, 100, 100, 100))
            {
                safeShutdown();
                return false;
            }

            if(!setJointConfiguration(place_zones_[i].joints_))
            {
                ROS_ERROR("Could not set the object transfer configuration!");
                safeShutdown();
                return false;
            }
            task_error_tol_ = 1e-3;
            activateHQPControl();

            while(!task_status_changed_)
                cond_.wait(lock);

            if(!task_success_)
            {
                ROS_ERROR("Could not complete the object transfer configuration!");
                safeShutdown();
                return false;
            }
            ROS_INFO("Object transfer tasks executed successfully.");
        }

        {//OBJECT PLACE
            ROS_INFO("Trying object place.");
            boost::mutex::scoped_lock lock(manipulator_tasks_m_);
            task_status_changed_ = false;
            task_success_ = false;
            deactivateHQPControl();
            if(!resetState())
            {
                ROS_ERROR("Could not reset the state!");
                safeShutdown();
                return false;
            }
            if(!setCartesianStiffness(100, 1000, 1000, 100, 100, 100))
            {
                safeShutdown();
                return false;
            }

            if(!setObjectPlace(place_zones_[i]))
            {
                ROS_ERROR("Could not set the object place!");
                safeShutdown();
                return false;
            }
            task_error_tol_ = 1e-4;
            task_diff_tol_ = 1e-5;
            activateHQPControl();

            while(!task_status_changed_)
                cond_.wait(lock);

            if(!task_success_)
            {
                ROS_ERROR("Could not complete the object place tasks!");
                safeShutdown();
                return false;
            }
            ROS_INFO("Object place tasks executed successfully.");
        }

        if(!with_gazebo_)
        {
            velvet_interface_node::VelvetToPos poscall2;
            poscall2.request.angle = 0.2;

            if(!velvet_pos_clt_.call(poscall2))
            {
                ROS_ERROR("could not call velvet to pos");
                ROS_BREAK();
            }
        }

        {//GRIPPER EXTRACT
            ROS_INFO("Trying gripper extract.");
            boost::mutex::scoped_lock lock(manipulator_tasks_m_);
            task_status_changed_ = false;
            task_success_ = false;
            deactivateHQPControl();
            if(!resetState())
            {
                ROS_ERROR("Could not reset the state!");
                safeShutdown();
                return false;
            }
            if(!setCartesianStiffness(1000, 1000, 1000, 100, 100, 100))
            {
                safeShutdown();
                return false;
            }

            if(!setGripperExtract(place_zones_[i]))
            {
                ROS_ERROR("Could not set the gripper extract!");
                safeShutdown();
                return false;
            }

            task_error_tol_ = 5 * 1e-3;
            activateHQPControl();

            while(!task_status_changed_)
                cond_.wait(lock);

            if(!task_success_)
            {
                ROS_ERROR("Could not complete the gripper extract tasks!");
                safeShutdown();
                return false;
            }
            ROS_INFO("Gripper extract tasks executed successfully.");
        }
    }

    {//MANIPULATOR TRANSFER CONFIGURATION
        ROS_INFO("Trying to put the manipulator in transfer configuration.");

        boost::mutex::scoped_lock lock(manipulator_tasks_m_);
        task_status_changed_ = false;
        task_success_ = false;
        deactivateHQPControl();
        if(!resetState())
        {
            ROS_ERROR("Could not reset the state!");
            safeShutdown();
            return false;
        }
        if(!setCartesianStiffness(1000, 1000, 1000, 100, 100, 100))
        {
            safeShutdown();
            return false;
        }

        if(!setJointConfiguration(transfer_config_))
        {
            ROS_ERROR("Could not set manipulator transfer configuration!");
            safeShutdown();
            return false;
        }
        task_error_tol_ = 1e-2;
        activateHQPControl();

        while(!task_status_changed_)
            cond_.wait(lock);

        if(!task_success_)
        {
            ROS_ERROR("Could not complete the manipulator transfer configuration tasks!");
            safeShutdown();
            return false;
        }
        ROS_INFO("Manipulator transfer configuration tasks executed successfully.");
    }

    deactivateHQPControl();
    resetState();
    reset_hqp_control_clt_.call(srv);
    pers_task_vis_ids_.clear();
    //MOVE TO DROP OFF
    ROS_INFO("Moving to drop-off pose");
    next_truck_task_clt_.call(srv);

    //MOVE HOME 
    ROS_INFO("Moving back home");
    next_truck_task_clt_.call(srv);

#if 0
#endif
    ROS_INFO("DEMO FINISHED.");

    return true;
}
//--------------------------------------------------------------------------
}//end namespace grasping_experiments


/////////////////////////////////
//           MAIN              //
/////////////////////////////////


//---------------------------------------------------------------------
int main(int argc, char **argv)
{
    ros::init(argc, argv, "grasping_experiments");

    grasping_experiments::GraspingExperiments experiments;

    ROS_INFO("Grasping experiments node ready");
    ros::AsyncSpinner spinner(4); // Use 4 threads
    spinner.start();
    ros::waitForShutdown();

    return 0;
}
//---------------------------------------------------------------------
