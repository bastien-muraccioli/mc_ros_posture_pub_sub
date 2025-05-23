#include "RosPosturePubSub.h"

#include <mc_control/GlobalPluginMacros.h>
#include <Eigen/src/Core/Matrix.h>
#include <cstddef>
#include <vector>

namespace mc_plugin
{

RosPosturePubSub::~RosPosturePubSub()  = default;

void RosPosturePubSub::init(mc_control::MCGlobalController & controller, const mc_rtc::Configuration & config)
{
  auto & ctl = static_cast<mc_control::MCGlobalController &>(controller);

  auto & robot = ctl.robot(ctl.robots()[0].name());
  auto & realRobot = ctl.realRobot(ctl.robots()[0].name());
  auto & rjo = robot.refJointOrder();

  jointNumber_ = rjo.size();

  if(!ctl.controller().datastore().has("ros_spin"))
  {
    ctl.controller().datastore().make<bool>("ros_spin", false);
  }
  
  robotPositions_.resize(jointNumber_);
  robotVelocities_.resize(jointNumber_);
  postureReceivedVector_.setZero(jointNumber_);
  auto plugin_config = config("ros_posture_pub_sub");
  robotJointsName_ = rjo;

  postureCommandTopic_ = plugin_config("command_topic", (std::string) "/joint_trajectory_controller/joint_trajectory");
  postureFeedbackTopic_ = plugin_config("state_topic", (std::string) "/joint_trajectory_controller/state");
  pub_freq_ =  plugin_config("freq", 100);

  dt_ = ctl.timestep();
  pub_dt_ = 1.0 / pub_freq_;
  counter_ = 0;

  if(dt_ >= pub_dt_)
  {
    mc_rtc::log::error("dt_ ({}) is greater than pub_dt_ ({})", dt_, pub_dt_);
    throw std::runtime_error("[RosPosturePubSub] dt_ is greater than pub_dt_");
  }
  
  // Get node handle
  node_ = mc_rtc::ROSBridge::get_node_handle();
  if(!ctl.controller().datastore().get<bool>("ros_spin"))
  {
    spinThread_ = std::thread(std::bind(&RosPosturePubSub::rosSpinner, this));
    ctl.controller().datastore().assign("ros_spin", true);
  }
  mc_rtc::log::info("[RosPosturePubSub][ROS] Subscribing to {}", postureCommandTopic_);
  posture_sub_.subscribe(node_, postureCommandTopic_);
  posture_sub_.maxTime(pub_dt_);

  mc_rtc::log::info("[RosPosturePubSub][ROS] Publishing to {}", postureFeedbackTopic_);
  posture_pub_.init(node_, postureFeedbackTopic_, 100);

  for(size_t i = 0; i < jointNumber_; i++)
  {
    robotPositions_[i] = robot.q()[robot.jointIndexByName(rjo[i])][0];
    robotVelocities_[i] = robot.alpha()[robot.jointIndexByName(rjo[i])][0];
  }

  msg_.joint_names = robotJointsName_;
  msg_.feedback.positions = robotPositions_;
  msg_.feedback.velocities = robotVelocities_;
  posture_pub_.publish(msg_);

  ctl.controller().datastore().make<std::map<std::string, std::vector<double>>>("ros_posture_pub_sub");

  const auto posture_data = posture_sub_.data(); // thread-safe: only access once
  if(posture_data.isValid())
  {
    const auto & posture = posture_data.value();

    if(posture.points.empty())
    {
      mc_rtc::log::warning("Received posture message with no points");
    }
    else if(posture.points[0].positions.size() < jointNumber_)
    {
      mc_rtc::log::error("Trajectory point has {} positions but expected {}", posture.points[0].positions.size(), jointNumber_);
    }
    else if(robotJointsName_.size() < jointNumber_)
    {
      mc_rtc::log::error("robotJointsName_ size ({}) is less than jointNumber_ ({})", robotJointsName_.size(), jointNumber_);
    }
    else if(postureReceivedVector_.size() < jointNumber_)
    {
      mc_rtc::log::error("postureReceivedVector_ size ({}) is less than jointNumber_ ({})", postureReceivedVector_.size(), jointNumber_);
    }
    else
    {
      for(size_t i = 0; i < jointNumber_; i++)
      {
        postureReceived_[robotJointsName_[i]] = {posture.points[0].positions[i]};
        postureReceivedVector_[i] = posture.points[0].positions[i];
      }
      ctl.controller().datastore().assign("ros_posture_pub_sub", postureReceived_);
    }
  }


  ctl.controller().logger().addLogEntry("RosPosturePubSub", [this]() { return postureReceivedVector_; });
  ctl.controller().gui()->addElement({"Plugins", "RosPosturePubSub"},
                                     mc_rtc::gui::ArrayLabel("Posture", robotJointsName_, [this]() { return postureReceivedVector_; }));

  mc_rtc::log::info("RosPosturePubSub::init called with configuration:\n{}", config.dump(true, true));
}

void RosPosturePubSub::reset(mc_control::MCGlobalController & controller)
{
  mc_rtc::log::info("RosPosturePubSub::reset called");
}

void RosPosturePubSub::before(mc_control::MCGlobalController & controller)
{
  auto & ctl = static_cast<mc_control::MCGlobalController &>(controller);

  auto & robot = ctl.robot(ctl.robots()[0].name());
  auto & realRobot = ctl.realRobot(ctl.robots()[0].name());

  counter_+= dt_;

  if(counter_>= 1/pub_freq_)
  {
    for(size_t i = 0; i < jointNumber_; i++)
    {
      robotPositions_[i] = robot.q()[robot.jointIndexByName(robotJointsName_[i])][0];
      robotVelocities_[i] = robot.alpha()[robot.jointIndexByName(robotJointsName_[i])][0];
    }
    msg_.joint_names = robotJointsName_;
    msg_.feedback.positions = robotPositions_;
    msg_.feedback.velocities = robotVelocities_;
    posture_pub_.publish(msg_);
    counter_ = 0;

    const auto posture_data = posture_sub_.data(); // thread-safe: only access once
    if(posture_data.isValid())
    {
      const auto & posture = posture_data.value();
      
      if(posture.points.empty())
      {
        mc_rtc::log::warning("Received posture message with no points");
      }
      else if(posture.points[0].positions.size() < jointNumber_)
      {
        mc_rtc::log::error("Trajectory point has {} positions but expected {}", posture.points[0].positions.size(), jointNumber_);
      }
      else if(robotJointsName_.size() < jointNumber_)
      {
        mc_rtc::log::error("robotJointsName_ size ({}) is less than jointNumber_ ({})", robotJointsName_.size(), jointNumber_);
      }
      else if(postureReceivedVector_.size() < jointNumber_)
      {
        mc_rtc::log::error("postureReceivedVector_ size ({}) is less than jointNumber_ ({})", postureReceivedVector_.size(), jointNumber_);
      }
      else
      {
        for(size_t i = 0; i < jointNumber_; i++)
        {
          postureReceived_[robotJointsName_[i]] = {posture.points[0].positions[i]};
          postureReceivedVector_[i] = posture.points[0].positions[i];
        }
        ctl.controller().datastore().assign("ros_posture_pub_sub", postureReceived_);
      }
    }
  }
}

void RosPosturePubSub::after(mc_control::MCGlobalController & controller)
{
  mc_rtc::log::info("RosPosturePubSub::after");
}

mc_control::GlobalPlugin::GlobalPluginConfiguration RosPosturePubSub::configuration()
{
  mc_control::GlobalPlugin::GlobalPluginConfiguration out;
  out.should_run_before = true;
  out.should_run_after = false;
  out.should_always_run = true;
  return out;
}

void RosPosturePubSub::rosSpinner()
{
  mc_rtc::log::info("[RosPosturePubSub][ROS Spinner] thread started");
  rclcpp::Rate rate(pub_freq_);
  while(rclcpp::ok())
  {
    rclcpp::spin_some(node_);
    rate.sleep();
  }
}

} // namespace mc_plugin

EXPORT_MC_RTC_PLUGIN("RosPosturePubSub", mc_plugin::RosPosturePubSub)
