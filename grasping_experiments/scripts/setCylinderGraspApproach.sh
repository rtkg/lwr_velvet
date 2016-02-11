rosservice call /lwr/lwr_velvet_hqp_vel_controller/activate_hqp_control false

#clean the controller
rosservice call /lwr/lwr_velvet_hqp_vel_controller/reset_hqp_control 

#load the persistent tasks
#rosservice call /lwr/lwr_velvet_hqp_vel_controller/load_tasks "task_definitions"

#set the joint setpoint tasks
rosservice call /lwr/lwr_velvet_hqp_vel_controller/set_tasks '{tasks: [{t_type: 1, priority: 2, name: "ee_above_lower_plane", is_equality_task: false, task_frame: "world", ds: 0.0, di: 0.02, dynamics: {d_type: 1, d_data: [-0.1]}, t_links: [{link_frame: "world", geometries: [{g_type: 3, g_data: [0, 0, 1, 0.22]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 1, g_data: [0, 0, 0]}]}]}, {t_type: 1, priority: 2, name: "ee_below_upper_plane", is_equality_task: false, task_frame: "world", ds: 0.0, di: 0.05, dynamics: {d_type: 1, d_data: [-0.5]}, t_links: [{link_frame: "world", geometries: [{g_type: 3, g_data: [0, 0, -1, -0.24]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 1, g_data: [0, 0, 0]}]}]}, {t_type: 1, priority: 2, name: "ee_outside_smaller_cylinder", is_equality_task: false, task_frame: "world", ds: 0.0, di: 0.02, dynamics: {d_type: 1, d_data: [-0.01]}, t_links: [{link_frame: "world", geometries: [{g_type: 9, g_data: [1, -0.9, 0.14, 0, 0, 1, 0.1]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 1, g_data: [0, 0, 0]}]}]}, {t_type: 1, priority: 2, name: "ee_inside_larger_cylinder", is_equality_task: false, task_frame: "world", ds: 0.0, di: 0.05, dynamics: {d_type: 1, d_data: [-0.5]}, t_links: [{link_frame: "velvet_fingers_palm", geometries: [{g_type: 1, g_data: [0, 0, 0]}]}, {link_frame: "world", geometries: [{g_type: 9, g_data: [1, -0.9, 0.14, 0, 0, 1, 0.15]}]}]}, {t_type: 5, priority: 2, is_equality_task: false, task_frame: "world", ds: 0.0, di: 1, dynamics: {d_type: 1, d_data: [-1.0]}, t_links: [{link_frame: "world", geometries: [{g_type: 2, g_data: [1.0, -0.9, 0.14, 0.0, 0.0, 1.0]} ]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 8, g_data: [0, 0.0, 0, 1.0, 0.0, 0.0, 0.1]}]}]}, {t_type: 2, priority: 2, is_equality_task: false, task_frame: "world", ds: 0.0, di: 1, dynamics: {d_type: 1, d_data: [-1.0]}, t_links: [{link_frame: "world", geometries: [{g_type: 8, g_data: [0, 0, 0, 0, 0, 1, 0.05]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 2, g_data: [0, 0, 0, 0, 0, 1]}]}]} ]}'


#visualize the tasks
rosservice call /lwr/lwr_velvet_hqp_vel_controller/visualize_task_geometries '{ids: [0,1,2,3,4,5]}'

rosservice call /lwr/lwr_velvet_hqp_vel_controller/activate_hqp_control true
