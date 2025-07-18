/*
    InputTracklistComponent.h
    -------------------------
    UI component that displays a table of all discovered M1-Panner instances.
    
    Features:
    - Shows index, name, azimuth, elevation, diverge, and gain for each panner
    - Supports selection of panner instances
    - Real-time updates from PannerTrackingManager
    - Indicates tracking method (M1MemoryShare vs OSC)
    
    Table Columns:
    - Index: Panner number/ID
    - Name: Panner instance name

    - Azimuth: Current azimuth value
    - Elevation: Current elevation value
    - Diverge: Current diverge value
    - Out Gain (dB): Current gain value
*/

#pragma once

#include <JuceHeader.h>
#include "../../Managers/PannerTrackingManager.h"
#include <vector>
#include <functional>

namespace Mach1 {

/**
 * Table component that displays all M1-Panner instances
 */
class InputTracklistComponent : public juce::Component,
                               public juce::TableListBoxModel
{
public:
    explicit InputTracklistComponent();
    ~InputTracklistComponent() override;
    
    // Data updates
    void updatePannerData(const std::vector<PannerInfo>& panners);
    void setSelectedPanner(int index);
    int getSelectedPanner() const { return selectedPannerIndex; }
    
    // Selection callback
    std::function<void(int)> onSelectionChanged;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    
    // Column setup
    void setupColumns();
    juce::String getColumnText(int rowNumber, int columnId) const;
    
    // Data
    std::vector<PannerInfo> pannerData;
    int selectedPannerIndex = -1;
    
    // UI components
    std::unique_ptr<juce::TableListBox> table;
    std::unique_ptr<juce::Label> titleLabel;
    
    // Column IDs
    enum ColumnIds {
        IndexColumn = 1,
        NameColumn = 2,
        ChannelsColumn = 3,
        AzimuthColumn = 4,
        ElevationColumn = 5,
        DivergeColumn = 6,
        OutGainColumn = 7
    };
    
    // Styling
    juce::Colour backgroundColour{0xFF1A1A1A};
    juce::Colour headerColour{0xFF2A2A2A};
    juce::Colour textColour{0xFFE0E0E0};
    juce::Colour selectedRowColour{0xFF404040};
    juce::Colour memoryShareIndicatorColour{0xFF4CAF50};  // Green for M1MemoryShare
    juce::Colour oscIndicatorColour{0xFFFF9800};          // Orange for OSC
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputTracklistComponent)
};

} // namespace Mach1 