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

// ROS includes
#include "ros/ros.h"
#include "geometry_msgs/Pose.h"
#include "geometry_msgs/PoseStamped.h"
#include "tf/tf.h"
#include "tf/transform_listener.h"

// temoto includes
#include "temoto_include.h"
#include "leap_motion_controller/LeapMotionOutput.h"

// Other includes
#include "math.h"
#include <cstdlib>
#include <iostream>
#include <algorithm>    // std::reverse

#ifndef START_TELEOP_H
#define START_TELEOP_H

class Teleoperator
{
public:
  Teleoperator()
  {
    scale_by_ = 1;
    AMP_HAND_MOTION_ = 10;
    using_natural_control_ = true;
    orientation_locked_ = false;
    position_limited_ = true;
    position_fwd_only_ = false;
    right_hand_before_ = false;
  };
  
  // Callout functions
  
  void callPlanAndMove(uint8_t action_type);
  
  void callPlanAndMoveNamedTarget(uint8_t action_type, std::string named_target);
  
  void computeCartesian(std::string frame_id);

  // Helper functions
  
  std::vector<geometry_msgs::Pose> wayposesInBaseLinkFrame(std::vector<geometry_msgs::Pose> wayposes_leapmotion/*, tf::TransformListener &tf_listener*/);
  
  geometry_msgs::Quaternion oneEightyAroundOperatorUp(geometry_msgs::Quaternion operator_input_quaternion_msg);
  
  temoto::Status getStatus( /*tf::TransformListener &transform_listener*/ );
  
  //Callback functions
  
  void processLeap(leap_motion_controller::LeapMotionOutput leap_data);		// TODO rename to more general case, e.g. processHandPose
  
  void processPowermate(temoto::Dial powermate);				// TODO rename to more general case, e.g. processScaleFactor
  
  void processEndEffector(geometry_msgs::PoseStamped end_effector_pose);	// TODO rename to updateEndEffector or similar
  
  void executeVoiceCommand(temoto::Command voice_command);			// TODO rename to processVoiceCommand
  
  ros::ServiceClient move_robot_client;		///< This service client for temoto/move_robot_service is global, so that callback funtions could see it.
  ros::ServiceClient tf_client;			///< Service client for requesting changes of control mode, i.e., change of orientation for leap_motion frame. 

private:

  tf::TransformListener transform_listener_;					// Local TransformListener

  double scale_by_;			///< Scaling factor, it is normalized to 1.
  int8_t AMP_HAND_MOTION_; 		///< Amplification of hand motion.

  geometry_msgs::PoseStamped current_pose_;///< Latest pose value received for the end effector.
  geometry_msgs::PoseStamped live_pose_;///< Properly scaled target pose in reference to leap motion frame.
  std::vector<geometry_msgs::Pose> wayposes_;		///< Vector of desired wayposes for a Cartesian move of the end-effector. Specified in the frame of the end-effector.
  std::vector<geometry_msgs::Pose> wayposes_fixed_to_baselink_;///< Vector of desired wayposes for a Cartesian move of the end-effector. Specified in the frame of the base_link.

  // State variables:
  
  // 'natural' robot and human are oriented the same way; 'inverted' means the human operator is facing the robot so that left and right are inverted.
  bool using_natural_control_;	///< Mode of intepration for hand motion: 'true' - natural, i.e., human and robot arms are the same; 'false' - inverted.

  bool orientation_locked_;	///< 'True' if hand orientation info is to be ignored.
  bool position_limited_;		///< 'True' if hand position is restricted to a specific direction/plane.
  bool position_fwd_only_;		///< 'True' when hand position is restricted to back and forward motion. Is only relevant when position_limited is 'true'.
  bool right_hand_before_;		///< Presense of right hand during the previous iteration of Leap Motion's callback processLeap(..).
};

#endif