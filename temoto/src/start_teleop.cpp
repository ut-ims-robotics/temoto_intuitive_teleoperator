// Copyright (c) 2015-2016, The University of Texas at Austin
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
// 
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
// 
//     * Neither the name of the copyright holder nor the names of its
//       contributors may be used to endorse or promote products derived from
//       this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "start_teleop.h"

#define HEIGHT_OF_ZERO 0.2		///< Height of zero position above Leap Motion Controller.




/** Function that actually makes the service call to temoto/move_robot_service.
 *  @param action_type determines what is requested from MoveGroup, i.e. PLAN (0x01), EXECUTE PLAN (0x02), or PLAN&EXECUTE (0x03). 
 */
void Teleoperator::callPlanAndMove(uint8_t action_type)
{
  // Create a service request
  temoto::Goal move;	
  move.request.goal = live_pose_;		// set live_pose_ as the requested pose for the motion planner
  move.request.action_type = action_type;	// set action_type

  // Adjust orientation for inverted control mode, i.e. translate leap_motion to leap_motion_on_robot
  if (!using_natural_control_)							// fix orientation for inverted view only
  {
    tf::Quaternion invert_palm_rotation(0, 1, 0, 0);				// 180° turn around Y axis
    tf::Quaternion palm_orientation;						// incoming palm orientation
    tf::quaternionMsgToTF(move.request.goal.pose.orientation, palm_orientation);// convert incoming quaternion msg to tf qauternion
    tf::Quaternion final_rotation = palm_orientation * invert_palm_rotation;	// apply invert_palm_rotation to incoming palm rotation
    final_rotation.normalize();							// normalize quaternion
    tf::quaternionTFToMsg(final_rotation, move.request.goal.pose.orientation);	// convert tf quaternion to quaternion msg
  }
  
  if (move_robot_client.call(move))			// call for the temoto/move_robot_service
  {
    ROS_INFO("Successfully called temoto/move_robot_service");
  }
  else
  {
    ROS_ERROR("Failed to call temoto/move_robot_service");
  }
  return;
} // end callPlanAndMove

/** Function that actually makes the service call to /temoto/move_robot_service.
 *  @param action_type determines what is requested from MoveGroup, i.e. PLAN (0x01), EXECUTE PLAN (0x02), or PLAN&EXECUTE (0x03).
 *  @param named_target uses this named target for target pose.
 */
void Teleoperator::callPlanAndMoveNamedTarget(uint8_t action_type, std::string named_target)
{
  temoto::Goal move;				// create a service request
  move.request.action_type = action_type;	// set action_type
  move.request.named_target = named_target;	// set named_target as the goal
  
  if (move_robot_client.call(move))			// call for the service to move robot group
  {
    ROS_INFO("Successfully called temoto/move_robot_service");
  }
  else
  {
    ROS_ERROR("Failed to call temoto/move_robot_service");
  }
  return;
} // end callPlanAndMoveNamedTarget



/** Function that makes the service call to /temoto/move_robot service.
 * 
 */
void Teleoperator::computeCartesian(std::string frame_id)
{
  temoto::Goal move;				// create a service request
  move.request.action_type = 0x04;		// set action_type
  move.request.cartesian_wayposes = wayposes_;
  move.request.cartesian_frame = frame_id;
  if (move_robot_client.call(move))			// call for the service to move robot group
  {
    ROS_INFO("[start_teleop] Successfully called temoto/move_robot_service for Cartesian move. Fraction of computed path is %f", move.response.cartesian_fraction);
    if (move.response.cartesian_fraction < 1 && !wayposes_.empty()) {
      ROS_INFO("[start_teleop] Failed to compute the entire Cartesian path. Removing the last waypose and retrying...");
      wayposes_.pop_back();
      computeCartesian(frame_id);		// going recursive
    }
//     wayposes_in_baselink = wayposesInBaseLinkFrame(wayposes, /*TODO*/);
  }
  else
  {
    ROS_ERROR("[start_teleop] Failed to call temoto/move_robot_service for Cartesian move.");
  }
  
  return;
}

/** Returns a vector of wayposes that have been tranformed from "leap_motion" frame to "base_link".
 * 
 *  @param wayposes_leapmotion vector of wayposes defined in "leap_motion" frame.
 *  @param tf_listener a TransformListener that has been around long enough to know a transform from "leap_motion" to "base_link"
 *  @return a vector of equal in size to exisitng wayposes but all the poses are defined in "base_link". 
 */
std::vector<geometry_msgs::Pose> Teleoperator::wayposesInBaseLinkFrame(std::vector<geometry_msgs::Pose> wayposes_leapmotion/*, tf::TransformListener &tf_listener*/)
{
  std::vector<geometry_msgs::Pose> wayposes_baselink;
  std::reverse(wayposes_leapmotion.begin(),wayposes_leapmotion.end());	// reversed the order of elements in wayposes_leapmotion
  geometry_msgs::PoseStamped stamped_pose_msg_lm, stamped_pose_msg_bl;	// temp stamped poses for leap_motion and base_link
  stamped_pose_msg_lm.header.frame_id = "leap_motion";			// stamp leap_motion poses with appropriate data
  stamped_pose_msg_lm.header.stamp = ros::Time(0);			// ros::Time(0) is different from ros::Time::now() and is bettere in this case, idky
  while ( !wayposes_leapmotion.empty() )
  {
    stamped_pose_msg_lm.pose = wayposes_leapmotion.back();		// access the last element of reversed wayposes_
    wayposes_leapmotion.pop_back();					// delete the last element
    tf::Stamped<tf::Pose> tf_pose_lm;					// tf equivalent for PoseStamped
    tf::Stamped<tf::Pose> tf_pose_bl;
    tf::poseStampedMsgToTF(stamped_pose_msg_lm, tf_pose_lm);		// convert geometry_msgs/PoseStamped to TF Stamped<Pose>
    transform_listener_.transformPose("base_link", tf_pose_lm, tf_pose_bl);	// tranform pose to base_link frame
    tf::poseStampedTFToMsg(tf_pose_bl, stamped_pose_msg_bl);		// convert TF Stamped<Pose> to geometry_msgs/PoseStamped 
    wayposes_baselink.push_back(stamped_pose_msg_bl.pose);		// push to the end of return vector
  }
  return wayposes_baselink;
}

/** Callback function for /leapmotion_general subscriber.
 *  It updates target pose based on latest left palm pose (any scaling and/or relevant limitations are also being applied).
 *  Presence of the right hand is used to lock and unlock forward motion.
 *  KEY_TAP gesture detection is currenly unimplemented.
 *  @param leap_data temoto::Leapmsg published by leap_motion node
 */
void Teleoperator::processLeap(leap_motion_controller::LeapMotionOutput leap_data)
{
  // NOTE: I am now using voice commands to switch between control modes because key taps are not very well understood by LeapSDK.
  // Right hand KeyTapGesture to switch between view modes
//   if (leap_data.right_hand_key_tap) {
//     ROS_INFO("LEAP has detected KeyTapGesture on right hand!");
//     temoto::ChangeTf switch_tf;
//     switch_tf.request.leap_motion_natural = !using_natural_control;		// request a change of view mode
//     if ( tf_client.call( switch_tf ) ) using_natural_control = !using_natural_control;// if request successful, change the value of view mode in this node
//   } // end if lefthand_key_tap

  
  // If position data is to be limited AND right_hand is detected AND right hand wasn't there before, toggle position_fwd_only_.
  if (position_limited_ && leap_data.right_hand && !right_hand_before_)
  {
    (position_fwd_only_) ? position_fwd_only_ = false : position_fwd_only_ = true;	// toggles position_fwd_only value between true and false
    right_hand_before_ = true;							// sets right_hand_before_ true;
    ROS_INFO("RIGHT HAND DETECTED, position_fwd_only_ is now %d", position_fwd_only_);
  }
  else if (leap_data.right_hand == false) right_hand_before_ = false;		// if right hand is not detected, set right_hand_before_ false;
  

  // Leap Motion Controller uses different coordinate orientation from the ROS standard for SIA5:
  //				SIA5	LEAP
  //		forward 	x	z
  //		right		y	x
  //		down		z	y

  // *** Following calculations are done in "leap_motion" frame, i.e. incoming leap_data pose is relative to leap_motion frame.

  // Working variable for potential target pole calculated from the hand pose coming from Leap Motion Controller (i.e. stamped to leap_motion frame).
  geometry_msgs::PoseStamped scaled_pose;
  // Header is copied without a change.
  scaled_pose.header = leap_data.header;
  // Reads the position of left palm in meters, amplifies to translate default motion; scales to dynamically adjust range; shifts for better usability.
  scaled_pose.pose.position.x = scale_by_*AMP_HAND_MOTION_*leap_data.left_palm_pose.position.x;
  scaled_pose.pose.position.y = scale_by_*AMP_HAND_MOTION_*(leap_data.left_palm_pose.position.y - HEIGHT_OF_ZERO);
  scaled_pose.pose.position.z = scale_by_*AMP_HAND_MOTION_*leap_data.left_palm_pose.position.z;
  // Orientation of left palm is copied unaltered, i.e., is not scaled
  scaled_pose.pose.orientation = leap_data.left_palm_pose.orientation;
  // Apply any relevant limitations to direction and/or orientation
  if (position_limited_ && position_fwd_only_) 		// if position is limited and position_fwd_only_ is true
  {
    scaled_pose.pose.position.x = 0;
    scaled_pose.pose.position.y = 0;
  }
  else if (position_limited_ && !position_fwd_only_)	// if position is limited and position_fwd_only_ is false, i.e. consider only sideways position change
  {
    scaled_pose.pose.position.z = 0;
  }
  
  if (orientation_locked_)				// if palm orientation is to be ignored 
  {
    // Overwrite orientation with identity quaternion.
    scaled_pose.pose.orientation.w = 1;
    scaled_pose.pose.orientation.x = 0;
    scaled_pose.pose.orientation.y = 0;
    scaled_pose.pose.orientation.z = 0;
  }
  
    // == Additional explanation of the above coordinates and origins ==
    // While the motion planner of sia5 uses base_link as origin, it is not convenient for the operator as s/he likely wishes to see the motion relative to end effector.
    // leap_motion frame has been attached to the end effector and, thus, hand motion is visualized relative to the pose of end effector.
    // If hand is moved to the origin of hand motion system, the target position is at current position. This makes the UI more intuitive for the operator, imho.
    // Scaling down drags the marker and the corresponding target position towards the actual origin of hand motion systems (i.e. Leap Motion Controller).
    // Leap Motion Controller has forward and left-right origins in the middle of the sensor display, which is OK, but up-down origin is on the keyboard.
    // Subtracting HEIGHT_OF_ZERO sets the up-down origin at HEIGHT_OF_ZERO above the keyboard and allows negative values which represent downward motion relative to end effector.

  // Setting properly scaled and limited pose as the live_pose_
  live_pose_.pose = scaled_pose.pose;
  live_pose_.header.frame_id = leap_data.header.frame_id;
  live_pose_.header.stamp = ros::Time::now();

  // Print position info to terminal
  printf("Scale motion by %f; move SIA5 by (x=%f, y=%f, z=%f) mm; position_fwd_only_=%d\n",
	   AMP_HAND_MOTION_*scale_by_, live_pose_.pose.position.x*1000, live_pose_.pose.position.y*1000, live_pose_.pose.position.z*1000, position_fwd_only_);

  return;
} // end processLeap

/** Callback function for powermate subscriber.
 *  It either reacts to dial being pressed or it updates the scaling factor.
 *  @param powermate temoto::Dial message published by powermate_dial node.
 */
void Teleoperator::processPowermate(temoto::Dial powermate)
{
  if (powermate.push_ev_occured)		// if push event (i.e. press or depress) has occured_occured
  {
    if (powermate.pressed == 1)			// if the dial has been pressed
    {
      ROS_INFO("Powermate Dial has been pressed");
      callPlanAndMove(0x03);			// makes the service request to move the robot; requests plan&execute
    }
    else					// if the dial has been depressed
    {
      ROS_INFO("Powermate Dial has been depressed");
    }
    return;
  }
  else						// if push event did not occur, it had to be dialing
  {
    // Calculate step size depening on the current scale_by_ value. Negating direction means that clock-wise is zoom-in/scale-down and ccw is zoom-out/scale-up
    // The smaller the scale_by_, the smaller the step. log10 gives the order of magnitude
    double step = (-powermate.direction)*pow( 10,  floor( log10( scale_by_ ) ) - 1 );
    if (step < -0.09) step = -0.01;		// special case for when scale_by is 1; to ensure that step is never larger than 0.01
    scale_by_ = scale_by_ + step;			// increase/decrease scale_by
    if (scale_by_ > 1) scale_by_ = 1;		// to ensure that scale_by is never larger than 1
    // Print position info to terminal
    printf("Scale motion by %f; move SIA5 by (x=%f, y=%f, z=%f) mm; position_fwd_only_=%d\n",
	    AMP_HAND_MOTION_*scale_by_, live_pose_.pose.position.x*1000, live_pose_.pose.position.y*1000, live_pose_.pose.position.z*1000, position_fwd_only_);
    return;
  } // else
} // end processPowermate()

/** Callback function for /temoto/end_effector_pose.
 *  Sets the received pose of an end effector as current_pose_.
 *  @param end_eff_pose geometry_msgs::PoseStamped for end effector.
 */
void Teleoperator::processEndEffector(geometry_msgs::PoseStamped end_effector_pose)
{
  current_pose_ = end_effector_pose;		// sets the position of end effector as current pose
  return;
} // end processEndeffector()


/** This function fixes the quaternion of a pose input in 'inverted control mode'.
 *  @param operator_input_quaternion_msg is the original quaternion during 'inverted control mode'.
 *  @return geometry_msgs::Quaternion quaternion_msg_out is the proper quaternion leap_motion frame.
 */
geometry_msgs::Quaternion Teleoperator::oneEightyAroundOperatorUp(geometry_msgs::Quaternion operator_input_quaternion_msg)
{
  geometry_msgs::Quaternion quaternion_msg_out;
  // Adjust orientation for inverted control mode, i.e. translate leap_motion to leap_motion_on_robot
  tf::Quaternion invert_rotation(0, 1, 0, 0);				// 180° turn around Y axis, UP in leap_motion frame
  tf::Quaternion operator_input;					// incoming operator's palm orientation
  tf::quaternionMsgToTF(operator_input_quaternion_msg, operator_input);	// convert incoming quaternion msg to tf quaternion
  tf::Quaternion final_rotation = operator_input * invert_rotation;	// apply invert_rotation to incoming palm rotation
  final_rotation.normalize();						// normalize resulting quaternion
  tf::quaternionTFToMsg(final_rotation, quaternion_msg_out);		// convert tf quaternion to quaternion msg
  return quaternion_msg_out;
}

/** Callback function for /temoto/voice_commands.
 *  Executes published voice command.
 *  @param voice_command contains the specific command as an unsigned integer.
 */
void Teleoperator::executeVoiceCommand(temoto::Command voice_command)
{
  // TODO check for namespace
  if (voice_command.cmd == 0xff) {
    ROS_INFO("Voice command received! Aborting ...");
    // TODO
  } else if (voice_command.cmd == 0x00) {
    ROS_INFO("Voice command received! Stopping ...");
    // TODO
  } else if (voice_command.cmd == 0x01) {
    ROS_INFO("Voice command received! Planning ...");
    callPlanAndMove(0x01);
  } else if (voice_command.cmd == 0x02) {
    ROS_INFO("Voice command received! Executing last plan ...");
    callPlanAndMove(0x02);
  } else if (voice_command.cmd == 0x03) {
    ROS_INFO("Voice command received! Planning and moving ...");
    callPlanAndMove(0x03);
  } else if (voice_command.cmd == 0xf1) {
    ROS_INFO("Voice command received! Planning to home ...");
    callPlanAndMoveNamedTarget(0x01, "home_pose");
  } else if (voice_command.cmd == 0xf3) {
    ROS_INFO("Voice command received! Planning and moving to home ...");
    callPlanAndMoveNamedTarget(0x03, "home_pose");
  } else if (voice_command.cmd == 0x10) {
    ROS_INFO("Voice command received! Using natural control mode ...");
    temoto::ChangeTf switch_tf;
    switch_tf.request.leap_motion_natural = true;			// request a change of view mode
    if ( tf_client.call( switch_tf ) ) using_natural_control_ = true;	// if request successful, change the value of view mode in this node
  } else if (voice_command.cmd == 0x11) {
    ROS_INFO("Voice command received! Using inverted control mode ...");
    temoto::ChangeTf switch_tf;
    switch_tf.request.leap_motion_natural = false;			// request a change of view mode
    if ( tf_client.call( switch_tf ) ) using_natural_control_ = false;	// if request successful, change the value of view mode in this node
  } else if (voice_command.cmd == 0x20) {
    ROS_INFO("Voice command received! Acknowledging 'Free directions' ...");
    position_limited_ = false;
    position_fwd_only_ = false;						// if input position is not limited, then there cannot be "forward only" mode
  } else if (voice_command.cmd == 0x21) {
    ROS_INFO("Voice command received! Acknowledging 'Limit directions' ...");
    position_limited_ = true;
  } else if (voice_command.cmd == 0x22) {
    ROS_INFO("Voice command received! Considering hand rotation/orientation ...");
    orientation_locked_ = false;
  } else if (voice_command.cmd == 0x23) {
    ROS_INFO("Voice command received! Ignoring hand rotation/orientation ...");
    orientation_locked_ = true;
  } else if (voice_command.cmd == 0x34) {			// Restart (delete all and add new) Cartesian wayposes
    ROS_INFO("Voice command received! Started defining new Cartesian path ...");
    wayposes_.clear();						// Clear existing wayposes
    geometry_msgs::Pose new_waypose = live_pose_.pose;	// Set live_pose_ as a new waypose
    if (!using_natural_control_) {				// Fix orientation, for inverted view only
      new_waypose.orientation = oneEightyAroundOperatorUp(new_waypose.orientation);
    }
    wayposes_.push_back(new_waypose);				// Add a waypose
    computeCartesian(live_pose_.header.frame_id.c_str());	// Try to compute Cartesian path
  } else if (voice_command.cmd == 0x35) {			// Add a Cartesian waypose to the end existing Cartesian path
    ROS_INFO("Voice command received! Adding a pose to Cartesian path ...");
    geometry_msgs::Pose new_waypose = live_pose_.pose;	// Set live_pose_ as a new waypose
    if (!using_natural_control_) {				// Fix orientation, for inverted view only
      new_waypose.orientation = oneEightyAroundOperatorUp(new_waypose.orientation);
    }
    wayposes_.push_back(new_waypose);				// Add a waypose
    computeCartesian(live_pose_.header.frame_id.c_str());	// Try to compute Cartesian path
  } else if (voice_command.cmd == 0x36) {				// Remove the last Cartesian wayposet 
    ROS_INFO("Voice command received! Removing the last Cartesian waypose  ...");
    wayposes_.pop_back();					// Remove last waypose
    computeCartesian(live_pose_.header.frame_id.c_str());	// Try to compute Cartesian path
  } else if (voice_command.cmd == 0x37) {				// Remove the last Cartesian wayposet 
    ROS_INFO("Voice command received! Removing the last Cartesian waypose  ...");
    wayposes_.clear();						// Clear all wayposes
  }
  
  else {
    ROS_INFO("Voice command received! Unknown voice command.");
  }
  return;
}

/** Puts all the latest global variable values into temoto/status message.
 *  @return temoto::Status message.
 */
temoto::Status Teleoperator::getStatus( /*tf::TransformListener &transform_listener*/ )
{
  temoto::Status status;
  status.header.stamp = ros::Time::now();
  status.header.frame_id = "start_teleop";
  status.scale_by = scale_by_;					// Latest scale_by_ value
  status.live_hand_pose = live_pose_;				// Latest hand pose stamped
  status.cartesian_wayposes = wayposesInBaseLinkFrame(wayposes_/*, tee_eff_listener*/);	// Latest cartesian wayposes in base_link frame
  status.in_natural_control_mode = using_natural_control_;
  status.orientation_free = !orientation_locked_;
  status.position_unlimited = !position_limited_;
  status.end_effector_pose = current_pose_;			// latest known end effector pose
  status.position_forward_only = position_fwd_only_;			// needs renaming

  return status;
} // end getStatus()

/** Main method. */
int main(int argc, char **argv)
{

  ros::init(argc, argv, "start_teleop");								// ROS init
  ros::NodeHandle n;											// ROS handle
  
  // Instance of teleoperator
  Teleoperator temoto_teleop;
 
  temoto_teleop.move_robot_client = n.serviceClient<temoto::Goal>("temoto/move_robot_service");				// ROS client for /temoto/move_robot_service
  temoto_teleop.tf_client = n.serviceClient<temoto::ChangeTf>("temoto/change_tf");

  ros::Subscriber sub_powermate = n.subscribe<temoto::Dial>("griffin_powermate", 10, &Teleoperator::processPowermate, &temoto_teleop);	// ROS subscriber on /griffin_powermate
  ros::Subscriber sub_leap = n.subscribe("leapmotion_general", 10,  &Teleoperator::processLeap, &temoto_teleop);			// ROS subscriber on /leapmotion_general
  ros::Subscriber sub_end_effector = n.subscribe("temoto/end_effector_pose", 0, &Teleoperator::processEndEffector, &temoto_teleop);	// ROS subscriber on /temoto/end_effector_pose
  ros::Subscriber sub_voicecommands = n.subscribe("temoto/voice_commands", 1, &Teleoperator::executeVoiceCommand, &temoto_teleop);	// ROS subscriber on /temoto/voice_commands
  
  ros::Publisher pub_status = n.advertise<temoto::Status>("temoto/status", 3);				// ROS publisher on /temoto/status
  
  ROS_INFO("Starting teleoperation ...");
  while (ros::ok()) {
    
    pub_status.publish( temoto_teleop.getStatus( /*tf_listener*/ ) );		// publish status current
      
    ros::spinOnce();				// spins once to update subscribers or something like that
  } // end while
  
  return 0;
} // end main