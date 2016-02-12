rosservice call /lwr/lwr_velvet_hqp_vel_controller/activate_hqp_control false

#clean the controller
rosservice call /lwr/lwr_velvet_hqp_vel_controller/reset_hqp_control 

#load the persistent tasks
#rosservice call /lwr/lwr_velvet_hqp_vel_controller/load_tasks "task_definitions"

#set the joint setpoint tasks
rosservice call /lwr/lwr_velvet_hqp_vel_controller/set_tasks '{tasks: [{t_type: 1, priority: 2, name: "ee_outside_small_sphere", is_equality_task: false, task_frame: "world", ds: 0.0, di: 0.02, dynamics: {d_type: 1, d_data: [-0.1]}, t_links: [{link_frame: "velvet_fingers_palm", geometries: [{g_type: 10, g_data: [0, 0, 0,0.001]}]}, {link_frame: "world", geometries: [{g_type: 10, g_data: [0.7, 0, 0, 0.3]}]}]}, {t_type: 1, priority: 2, name: "ee_inside_large_sphere", is_equality_task: false, task_frame: "world", ds: 0.0, di: 0.02, dynamics: {d_type: 1, d_data: [-0.1]}, t_links: [{link_frame: "world", geometries: [{g_type: 10, g_data: [0.7, 0, 0,0.5]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 10, g_data: [0, 0, 0, 0.001]}]}]} ]}'


#visualize the tasks
rosservice call /lwr/lwr_velvet_hqp_vel_controller/visualize_task_geometries '{ids: [0]}'

rosservice call /lwr/lwr_velvet_hqp_vel_controller/activate_hqp_control true
