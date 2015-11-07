rosservice call /apple/apple_hqp_vel_controller/activate_hqp_control false

#clean the controller
rosservice call /apple/apple_hqp_vel_controller/reset_hqp_control 

#load the persistent tasks
rosservice call /apple/apple_hqp_vel_controller/load_tasks "task_definitions"

#set the joint setpoint tasks
rosservice call /apple/apple_hqp_vel_controller/set_tasks '{tasks: [ {t_type: 1, priority: 2, name: ee_on_horizontal_plane, is_equality_task: true, task_frame: "world", ds: 0.0, di: 1.0, dynamics: {d_type: 1, d_data: [-0.3]}, t_links: [{link_frame: "world", geometries: [{g_type: 3, g_data: [0, 0, 1, 0.35]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 1, g_data: [0, 0, 0]}]}]},{t_type: 1, priority: 2, name: "sphere_sphere_task", is_equality_task: false, task_frame: "world", ds: 0.0, di: 1.0, dynamics: {d_type: 1, d_data: [-0.1]}, t_links: [{link_frame: "velvet_fingers_palm", geometries: [{g_type: 10, g_data: [0.2, 0, 0, 0.2]}]}, {link_frame: "world", geometries: [{g_type: 10, g_data: [1, -0.9, -0.1, 0.5]}]}]} ]}'


#visualize the tasks
rosservice call /apple/apple_hqp_vel_controller/visualize_task_geometries '{ids: [13, 14]}'

rosservice call /apple/apple_hqp_vel_controller/activate_hqp_control true
