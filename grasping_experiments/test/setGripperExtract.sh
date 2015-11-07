rosservice call /apple/apple_hqp_vel_controller/activate_hqp_control false

#clean the controller
rosservice call /apple/apple_hqp_vel_controller/reset_hqp_control 

#load the persistent tasks
rosservice call /apple/apple_hqp_vel_controller/load_tasks "task_definitions"

rosservice call /apple/iiwa_hw_interface/set_stiffness "{sx: 1000.0, sy: 500.0, sz: 500.0, sa: 200.0, sb: 200.0, sc: 200.0}"

rosservice call /apple/apple_hqp_vel_controller/set_tasks '{tasks: [ {t_type: 1, priority: 2, name: ee_on_horizontal_plane, is_equality_task: true, task_frame: "world", ds: 0.0, di: 0.05, dynamics: {d_type: 1, d_data: [-0.5]}, t_links: [{link_frame: "world", geometries: [{g_type: 3, g_data: [0, 0, 1, 0.6]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 1, g_data: [0, 0, 0]}]}]}, {t_type: 1, priority: 2, name: ee_outside_cylinder, is_equality_task: false, task_frame: "world", ds: 0.0, di: 0.05, dynamics: {d_type: 1, d_data: [-0.5]}, t_links: [{link_frame: "world", geometries: [{g_type: 9, g_data: [0.8, -0.4, 0.13, 0, 0, 1, 0.05]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 1, g_data: [0, 0, 0]}]}]}, {t_type: 2, priority: 2, name: gripper_vertical_axis_alignment, is_equality_task: false, task_frame: "world", ds: 0.0, di: 1, dynamics: {d_type: 1, d_data: [-0.25]}, t_links: [{link_frame: "world", geometries: [{g_type: 8, g_data: [0, 0, 0, 0, 0, 1, 0.2]}]}, {link_frame: "velvet_fingers_palm", geometries: [{g_type: 2, g_data: [0, 0, 0, 0, 0, 1]}]}]}]}'

#visualize the tasks
rosservice call /apple/apple_hqp_vel_controller/visualize_task_geometries '{ids: [7, 8, 9, 10, 11, 12, 13, 14, 15]}'

rosservice call /apple/apple_hqp_vel_controller/activate_hqp_control true
