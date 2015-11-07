 #!/bin/bash

rosservice call /apple/iiwa_hw_interface/set_stiffness "{sx: 2000.0, sy: 2000.0, sz: 5.0, sa: 200.0, sb: 200.0, sc: 5.0}" 

rosservice call /velvet_node/velvet_grasp "{current_threshold_contact: 15, current_threshold_final: 30, max_belt_travel_mm: 100, phalange_delta_rad: 0.03, gripper_closed_thresh: 1.5, check_phalanges: true}" 



./setPileGrasp.sh

