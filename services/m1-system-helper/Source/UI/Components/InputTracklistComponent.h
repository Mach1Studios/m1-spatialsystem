/*
    InputTracklistComponent.h
    -------------------------
    UI component that displays a table of all discovered M1-Panner instances.
    
    Features:
    - Shows index, name, channels, azimuth, elevation, diverge, gain, mode/status
    - Supports inline editing of azimuth, elevation, diverge, outGain
    - Real-time updates from PannerTrackingManager
    - Indicates tracking method (M1MemoryShare vs OSC)
    - Bi-directional parameter editing via .mem command region + OSC fallback
    
    Table Columns:
    - Index: Panner number/ID
    - Name: Panner instance name
    - Channels: Number of channels
    - Azimuth: Current azimuth value (editable)
    - Elevation: Current elevation value (editable)
    - Diverge: Current diverge value (editable)
    - Out Gain (dB): Current gain value (editable)
    - Mode/Status: Streaming/Native/Offline/Expired
*/

#pragma once

#include <JuceHeader.h>
#include "../../Managers/PannerTrackingManager.h"
#include <vector>
#include <functional>

namespace Mach1 {

// Forward declaration
class PannerTrackingManager;

/**
 * Table component that displays all M1-Panner instances with inline editing
 */
class InputTracklistComponent : public juce::Component,
                                public juce::TableListBoxModel,
                                public juce::Label::Listener
{
public:
    explicit InputTracklistComponent();
    ~InputTracklistComponent() override;
    
    // Data updates
    void updatePannerData(const std::vector<PannerInfo>& panners);
    void setSelectedPanner(int index);
    int getSelectedPanner() const { return selectedPannerIndex; }
    
    // Set tracking manager for bi-directional editing
    void setPannerTrackingManager(PannerTrackingManager* manager);
    
    // Selection callback
    std::function<void(int)> onSelectionChanged;
    
    // Parameter change callback (for OSC fallback)
    std::function<void(int pannerIndex, const juce::String& paramName, float value)> onParameterChanged;
    
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
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, 
                                              juce::Component* existingComponentToUpdate) override;
    
    // Label::Listener for inline editing
    void labelTextChanged(juce::Label* labelThatHasChanged) override;
    
    // Column setup
    void setupColumns();
    juce::String getColumnText(int rowNumber, int columnId) const;
    juce::String getModeStatusText(const PannerInfo& panner) const;
    bool isColumnEditable(int columnId) const;
    
    // Inline editing
    void startEditing(int rowNumber, int columnId);
    void commitEdit(int rowNumber, int columnId, float newValue);
    void sendParameterUpdate(int rowNumber, const juce::String& paramName, float value);
    
    // Data
    std::vector<PannerInfo> pannerData;
    int selectedPannerIndex = -1;
    PannerTrackingManager* pannerManager = nullptr;
    
    // Editing state
    int editingRow = -1;
    int editingColumn = -1;
    
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
        OutGainColumn = 7,
        ModeStatusColumn = 8
    };
    
    // Styling - matching reference design
    juce::Colour backgroundColour{0xFF0D0D0D};
    juce::Colour headerColour{0xFF141414};
    juce::Colour textColour{0xFFCCCCCC};
    juce::Colour selectedRowColour{0xFF2A2A2A};           // Dark gray for selection
    juce::Colour memoryShareIndicatorColour{0xFF939393}; // Gray for indicators
    juce::Colour oscIndicatorColour{0xFFFF9800};          // Orange for OSC
    juce::Colour editableColour{0xFFCCCCCC};              // Same as text - subtle
    juce::Colour streamingColour{0xFF939393};             // Gray for streaming
    juce::Colour nativeColour{0xFF939393};                // Gray for active native
    juce::Colour offlineColour{0xFF666666};               // Gray for offline
    juce::Colour expiredColour{0xFFFF4444};               // Red for expired
    juce::Colour staleColour{0xFFFFAA00};                 // Amber for stale
    juce::Colour rowAlternateColour{0xFF111111};          // Slight alternate row color
    juce::Colour borderColour{0xFF2A2A2A};                // Subtle borders
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputTracklistComponent)
};

/**
 * Editable label component for inline cell editing
 */
class EditableTableCell : public juce::Label
{
public:
    EditableTableCell(InputTracklistComponent& owner, int row, int column)
        : ownerTable(owner), rowIndex(row), columnId(column)
    {
        setEditable(false, true, false); // Single click to edit
        setJustificationType(juce::Justification::centredLeft);
        
        // Styling
        setColour(juce::Label::textColourId, juce::Colour(0xFFE0E0E0));
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::textColourId, juce::Colour(0xFFE0E0E0));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2A2A));
        setColour(juce::TextEditor::highlightColourId, juce::Colour(0xFF4CAF50));
    }
    
    int getRow() const { return rowIndex; }
    int getColumnId() const { return columnId; }
    
private:
    [[maybe_unused]] InputTracklistComponent& ownerTable;
    int rowIndex;
    int columnId;
};

} // namespace Mach1
