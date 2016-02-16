#disable gravity in gazebo
rosservice call /gazebo/set_physics_properties '{time_step: 0.001, max_update_rate: 1000, gravity: [0.0, 0.0, 0.0], ode_config: {auto_disable_bodies: false, sor_pgs_precon_iters: 0, sor_pgs_iters: 50, sor_pgs_w: 1.3, sor_pgs_rms_error_tol: 0.0, contact_surface_layer: 0.001, contact_max_correcting_vel: 100.0, cfm: 0.0, erp: 0.2,  max_contacts: 20} }'

rosservice call /lwr/lwr_velvet_hqp_vel_controller/activate_hqp_control false

#clean the controller
rosservice call /lwr/lwr_velvet_hqp_vel_controller/reset_hqp_control 

#load the persistent tasks
rosservice call /lwr/lwr_velvet_hqp_vel_controller/load_tasks "task_definitions"


#set the joint setpoint tasks
rosservice call /lwr/lwr_velvet_hqp_vel_controller/set_tasks '{tasks: [{t_type: 3, priority: 2, name: joint_setpoints, is_equality_task: true, task_frame: "world", ds: 0.0, di: 1.0, dynamics: {d_type: 1, d_data: [-0.4]}, t_links: [{link_frame: "lwr_1_link", geometries: [{g_type: 6, g_data: [0]} ]}, {link_frame: "lwr_2_link", geometries: [{g_type: 6, g_data: [0]}]}, {link_frame: "lwr_3_link", geometries: [{g_type: 6, g_data: [0]} ]}, {link_frame: "lwr_4_link", geometries: [{g_type: 6, g_data: [0]} ]}, {link_frame: "lwr_5_link", geometries: [{g_type: 6, g_data: [0]} ]}, {link_frame: "lwr_6_link", geometries: [{g_type: 6, g_data: [0]} ]}, {link_frame: "lwr_7_link", geometries: [{g_type: 6, g_data: [0]} ]} ]} ]}'

#visualize the loaded tasks
rosservice call /lwr/lwr_velvet_hqp_vel_controller/visualize_task_geometries '{ids: [7, 8, 9, 10, 11, 12]}'

rosservice call /lwr/lwr_velvet_hqp_vel_controller/activate_hqp_control true

