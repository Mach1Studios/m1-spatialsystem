// Link stubs for OSCHandler dependencies the unit tests never exercise.
//
// The tests construct OSCHandler with nullptr for the external mixer and the
// panner tracking manager (both are null-guarded at every call site), but the
// compiled OSCHandler.cpp still references these member functions, so the
// linker needs definitions. Stubbing them avoids pulling the Mach1 SDK,
// shared-memory, and audio engine stacks into the test binary.

#include "Core/ExternalMixerProcessor.h"
#include "Managers/PannerTrackingManager.h"

namespace Mach1 {

void ExternalMixerProcessor::setMasterYPR(float, float, float) {}

void ExternalMixerProcessor::setOutputFormat(int) {}

juce::Result PannerTrackingManager::registerOSCPanner(const M1RegisteredPlugin&)
{
    return juce::Result::ok();
}

} // namespace Mach1
