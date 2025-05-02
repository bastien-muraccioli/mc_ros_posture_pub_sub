#include "RosPosturePubSub.h"

#include <mc_control/GlobalPluginMacros.h>

namespace mc_plugin
{

RosPosturePubSub::~RosPosturePubSub() = default;

void RosPosturePubSub::init(mc_control::MCGlobalController & controller, const mc_rtc::Configuration & config)
{
  mc_rtc::log::info("RosPosturePubSub::init called with configuration:\n{}", config.dump(true, true));
}

void RosPosturePubSub::reset(mc_control::MCGlobalController & controller)
{
  mc_rtc::log::info("RosPosturePubSub::reset called");
}

void RosPosturePubSub::before(mc_control::MCGlobalController &)
{
  mc_rtc::log::info("RosPosturePubSub::before");
}

void RosPosturePubSub::after(mc_control::MCGlobalController & controller)
{
  mc_rtc::log::info("RosPosturePubSub::after");
}

mc_control::GlobalPlugin::GlobalPluginConfiguration RosPosturePubSub::configuration()
{
  mc_control::GlobalPlugin::GlobalPluginConfiguration out;
  out.should_run_before = true;
  out.should_run_after = true;
  out.should_always_run = true;
  return out;
}

} // namespace mc_plugin

EXPORT_MC_RTC_PLUGIN("RosPosturePubSub", mc_plugin::RosPosturePubSub)
