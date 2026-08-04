// Link stubs for OSCHandler dependencies the unit tests never exercise.
//
// The tests construct OSCHandler with nullptr for the external mixer (it is
// null-guarded at every call site), but the compiled OSCHandler.cpp still
// references these member functions, so the linker needs definitions.
// Stubbing them avoids pulling the audio engine stack into the test binary.
// PannerTrackingManager is NOT stubbed: the integration tests exercise the
// real implementation (see SystemIntegrationTests.cpp).

#include "Core/ExternalMixerProcessor.h"

namespace Mach1 {

void ExternalMixerProcessor::setMasterYPR(float, float, float) {}

void ExternalMixerProcessor::setOutputFormat(int) {}

} // namespace Mach1
