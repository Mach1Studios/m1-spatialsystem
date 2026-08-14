#include "Managers/ProjectPairingManager.h"
#include "Common/ProjectSessionPolicy.h"

#include <iostream>

namespace {
int failures = 0;

#define CHECK(condition)                                                           \
    do {                                                                           \
        if (!(condition)) {                                                        \
            ++failures;                                                            \
            std::cout << "FAILED: " #condition " (" << __FILE__ << ":"          \
                      << __LINE__ << ")" << std::endl;                             \
        }                                                                          \
    } while (false)
} // namespace

int runProjectPairingManagerTests()
{
    using Mach1::ProjectSessionPolicy::Action;
    using Mach1::ProjectSessionPolicy::chooseAction;

    // Native/control-only mode has no streaming hosts and must not interrupt
    // the user. Only an unnamed/ambiguous streaming project prompts.
    CHECK(chooseAction(0, false, false, false) == Action::None);
    CHECK(chooseAction(1, true, false, false) == Action::AutoStart);
    CHECK(chooseAction(1, false, false, false) == Action::Prompt);
    CHECK(chooseAction(1, true, true, false) == Action::Prompt);
    CHECK(chooseAction(2, true, false, false) == Action::Prompt);
    CHECK(chooseAction(1, false, false, true) == Action::None);

    const auto testRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("m1-project-pairing-" + juce::Uuid().toString());
    CHECK(testRoot.createDirectory());
    const auto registryFile = testRoot.getChildFile("bindings.json");

    const juce::String firstId = "11111111-1111-4111-8111-111111111111";
    const juce::String secondId = "22222222-2222-4222-8222-222222222222";

    {
        Mach1::ProjectPairingManager manager(registryFile);

        // Legacy/new instances each generate an ID. The first unnamed claim is
        // the temporary host anchor; later unnamed instances join it.
        const auto first = manager.registerHostClaim(1001, firstId, {});
        CHECK(first.bindingId == firstId);
        CHECK(!first.isNamed());
        const auto joined = manager.registerHostClaim(1001, secondId, {});
        CHECK(joined.bindingId == firstId);

        const auto named = manager.nameHostProject(1001, "Feature Film: Final / Mix");
        CHECK(named.bindingId == firstId);
        CHECK(named.displayName == "Feature Film: Final / Mix");
        CHECK(named.sessionId.startsWith("Feature_Film_Final_Mix_"));
        CHECK(!named.sessionId.containsAnyOf("\\/:*?\"<>|"));

        const auto other = manager.registerHostClaim(1002, secondId, "Trailer");
        CHECK(other.isNamed());
        manager.flushIfNeeded();
        CHECK(registryFile.existsAsFile());

        // Two different named saved projects in one process must not be
        // silently merged (REAPER can keep multiple project tabs in one PID).
        CHECK(manager.registerHostClaim(1003, firstId, "Feature Film: Final / Mix").isNamed());
        CHECK(!manager.registerHostClaim(1003, secondId, "Trailer").isValid());
        CHECK(manager.isHostAmbiguous(1003));

        const auto selected = manager.selectProject(1003, secondId);
        CHECK(selected.displayName == "Trailer");
        CHECK(!manager.isHostAmbiguous(1003));
    }

    {
        Mach1::ProjectPairingManager reloaded(registryFile);
        const auto bindings = reloaded.getKnownBindings();
        CHECK(bindings.size() == 2);

        // A restored named claim outranks a fresh generated ID after restart.
        CHECK(!reloaded.registerHostClaim(2001,
                                          "33333333-3333-4333-8333-333333333333",
                                          {}).isNamed());
        const auto restored = reloaded.registerHostClaim(2001, firstId,
                                                         "Feature Film: Final / Mix");
        CHECK(restored.bindingId == firstId);
        CHECK(restored.isNamed());
    }

    CHECK(testRoot.deleteRecursively());
    return failures;
}
