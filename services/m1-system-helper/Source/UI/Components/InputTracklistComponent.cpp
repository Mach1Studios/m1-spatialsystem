/*
    InputTracklistComponent.cpp
    ---------------------------
    Implementation of the panner table component with inline editing
*/

#include "InputTracklistComponent.h"

namespace Mach1 {

//==============================================================================
InputTracklistComponent::InputTracklistComponent()
{
    // Create title label
    titleLabel = std::make_unique<juce::Label>("title", "Input Tracklist");
    titleLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    titleLabel->setColour(juce::Label::textColourId, textColour);
    titleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel.get());
    
    // Create table
    table = std::make_unique<juce::TableListBox>("PannerTable", this);
    table->setColour(juce::TableListBox::backgroundColourId, backgroundColour);
    table->setColour(juce::TableListBox::textColourId, textColour);
    table->setColour(juce::TableHeaderComponent::backgroundColourId, headerColour);
    table->setColour(juce::TableHeaderComponent::textColourId, textColour);
    table->getHeader().setStretchToFitActive(true);
    table->setMultipleSelectionEnabled(false);
    addAndMakeVisible(table.get());
    
    // Set up columns
    setupColumns();
}

InputTracklistComponent::~InputTracklistComponent()
{
}

//==============================================================================
void InputTracklistComponent::setupColumns()
{
    auto& header = table->getHeader();
    
    header.addColumn("#", IndexColumn, 35, 25, 50);
    header.addColumn("Name", NameColumn, 100, 70, 180);
    header.addColumn("Ch", ChannelsColumn, 35, 30, 50);
    header.addColumn("Azimuth", AzimuthColumn, 70, 55, 100);
    header.addColumn("Elevation", ElevationColumn, 70, 55, 100);
    header.addColumn("Diverge", DivergeColumn, 60, 50, 90);
    header.addColumn("Gain (dB)", OutGainColumn, 70, 55, 100);
    header.addColumn("Status", ModeStatusColumn, 80, 60, 120);
}

//==============================================================================
void InputTracklistComponent::updatePannerData(const std::vector<PannerInfo>& panners)
{
    pannerData = panners;
    table->updateContent();
    table->repaint();
}

void InputTracklistComponent::setSelectedPanner(int index)
{
    if (index >= 0 && index < static_cast<int>(pannerData.size()))
    {
        selectedPannerIndex = index;
        table->selectRow(index);
    }
    else
    {
        selectedPannerIndex = -1;
        table->deselectAllRows();
    }
}

void InputTracklistComponent::setPannerTrackingManager(PannerTrackingManager* manager)
{
    pannerManager = manager;
}

//==============================================================================
void InputTracklistComponent::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);
}

void InputTracklistComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Title at top
    titleLabel->setBounds(bounds.removeFromTop(25));
    
    // Table takes remaining space
    table->setBounds(bounds.reduced(2));
}

//==============================================================================
// TableListBoxModel implementation
//==============================================================================

int InputTracklistComponent::getNumRows()
{
    return static_cast<int>(pannerData.size());
}

void InputTracklistComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.setColour(selectedRowColour);
        g.fillRect(0, 0, width, height);
    }
    else if (rowNumber % 2 == 0)
    {
        g.setColour(backgroundColour.brighter(0.05f));
        g.fillRect(0, 0, width, height);
    }
}

void InputTracklistComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= static_cast<int>(pannerData.size()))
        return;
    
    const auto& panner = pannerData[static_cast<size_t>(rowNumber)];
    
    // Set text colour - editable columns get different color
    if (isColumnEditable(columnId))
    {
        g.setColour(editableColour);
    }
    else
    {
        g.setColour(textColour);
    }
    
    g.setFont(juce::Font(11.0f));
    
    // Special handling for Mode/Status column
    if (columnId == ModeStatusColumn)
    {
        juce::String statusText = getModeStatusText(panner);
        
        // Set color based on status
        if (statusText.containsIgnoreCase("streaming"))
            g.setColour(streamingColour);
        else if (statusText.containsIgnoreCase("native") || statusText.containsIgnoreCase("active"))
            g.setColour(nativeColour);
        else if (statusText.containsIgnoreCase("stale"))
            g.setColour(staleColour);
        else if (statusText.containsIgnoreCase("offline"))
            g.setColour(offlineColour);
        else if (statusText.containsIgnoreCase("expired"))
            g.setColour(expiredColour);
        
        g.drawText(statusText, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
    }
    else
    {
        // Get cell text
        juce::String cellText = getColumnText(rowNumber, columnId);
        
        // Draw text
        g.drawText(cellText, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
    }
    
    // Draw tracking method indicator for name column
    if (columnId == NameColumn)
    {
        juce::Colour indicatorColour = panner.isMemoryShareBased ? memoryShareIndicatorColour : oscIndicatorColour;
        g.setColour(indicatorColour);
        g.fillEllipse(static_cast<float>(width - 12), static_cast<float>(height / 2 - 3), 6.0f, 6.0f);
    }
}

void InputTracklistComponent::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (rowNumber >= 0 && rowNumber < static_cast<int>(pannerData.size()))
    {
        selectedPannerIndex = rowNumber;
        table->selectRow(rowNumber);
        
        if (onSelectionChanged)
            onSelectionChanged(rowNumber);
    }
}

void InputTracklistComponent::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    // Start editing if column is editable
    if (isColumnEditable(columnId) && rowNumber >= 0 && rowNumber < static_cast<int>(pannerData.size()))
    {
        startEditing(rowNumber, columnId);
    }
    else
    {
        cellClicked(rowNumber, columnId, event);
    }
}

juce::Component* InputTracklistComponent::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, 
                                                                   juce::Component* existingComponentToUpdate)
{
    // For editable columns, return an editable label
    if (isColumnEditable(columnId))
    {
        auto* editableCell = static_cast<EditableTableCell*>(existingComponentToUpdate);
        
        if (editableCell == nullptr)
        {
            editableCell = new EditableTableCell(*this, rowNumber, columnId);
            editableCell->addListener(this);
        }
        
        // Update the cell content
        juce::String cellText = getColumnText(rowNumber, columnId);
        editableCell->setText(cellText, juce::dontSendNotification);
        
        return editableCell;
    }
    
    // For non-editable columns, return nullptr to use default painting
    if (existingComponentToUpdate != nullptr)
    {
        delete existingComponentToUpdate;
    }
    
    return nullptr;
}

//==============================================================================
void InputTracklistComponent::labelTextChanged(juce::Label* labelThatHasChanged)
{
    auto* editableCell = dynamic_cast<EditableTableCell*>(labelThatHasChanged);
    if (editableCell == nullptr)
        return;
    
    int rowIndex = editableCell->getRow();
    int columnId = editableCell->getColumnId();
    
    if (rowIndex < 0 || rowIndex >= static_cast<int>(pannerData.size()))
        return;
    
    // Parse the new value
    juce::String newText = labelThatHasChanged->getText();
    float newValue = newText.getFloatValue();
    
    // Commit the edit
    commitEdit(rowIndex, columnId, newValue);
}

//==============================================================================
juce::String InputTracklistComponent::getColumnText(int rowNumber, int columnId) const
{
    if (rowNumber >= static_cast<int>(pannerData.size()))
        return {};
    
    const auto& panner = pannerData[static_cast<size_t>(rowNumber)];
    
    switch (columnId)
    {
        case IndexColumn:
            return juce::String(rowNumber + 1);
            
        case NameColumn:
            return juce::String(panner.name).isEmpty() 
                ? "Panner " + juce::String(rowNumber + 1)
                : juce::String(panner.name);
            
        case ChannelsColumn:
            return juce::String(panner.channels);
            
        case AzimuthColumn:
            return juce::String(panner.azimuth, 1);
            
        case ElevationColumn:
            return juce::String(panner.elevation, 1);
            
        case DivergeColumn:
            return juce::String(panner.diverge, 1);
            
        case OutGainColumn:
            return juce::String(panner.gain, 1);
            
        case ModeStatusColumn:
            return getModeStatusText(panner);
            
        default:
            return {};
    }
}

juce::String InputTracklistComponent::getModeStatusText(const PannerInfo& panner) const
{
    // Use the connection status enum if available
    switch (panner.connectionStatus)
    {
        case PannerConnectionStatus::Stale:
            return "Stale";
            
        case PannerConnectionStatus::Disconnected:
            return "Disconnected";
            
        case PannerConnectionStatus::Active:
        default:
            break;
    }
    
    // Determine status based on panner state for Active connection
    if (!panner.isActive)
    {
        return "Expired";
    }
    
    if (panner.isMemoryShareBased)
    {
        // Memory-share based panners are considered "streaming" if actively updating
        auto timeSinceUpdate = juce::Time::currentTimeMillis() - panner.lastUpdateTime;
        if (timeSinceUpdate > 5000) // 5 seconds without update
        {
            return "Offline";
        }
        
        if (panner.isPlaying)
        {
            return "Streaming";
        }
        
        return "Native";
    }
    else
    {
        // OSC-based panners
        auto timeSinceUpdate = juce::Time::currentTimeMillis() - panner.lastUpdateTime;
        if (timeSinceUpdate > 10000) // 10 seconds without update
        {
            return "Offline";
        }
        
        return "OSC Active";
    }
}

bool InputTracklistComponent::isColumnEditable(int columnId) const
{
    return columnId == AzimuthColumn || 
           columnId == ElevationColumn || 
           columnId == DivergeColumn || 
           columnId == OutGainColumn;
}

//==============================================================================
void InputTracklistComponent::startEditing(int rowNumber, int columnId)
{
    editingRow = rowNumber;
    editingColumn = columnId;
    
    // The table will handle the editing via refreshComponentForCell
    table->repaintRow(rowNumber);
}

void InputTracklistComponent::commitEdit(int rowNumber, int columnId, float newValue)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(pannerData.size()))
        return;
    
    // Determine parameter name
    juce::String paramName;
    float clampedValue = newValue;
    
    switch (columnId)
    {
        case AzimuthColumn:
            paramName = "azimuth";
            clampedValue = juce::jlimit(-180.0f, 180.0f, newValue);
            pannerData[static_cast<size_t>(rowNumber)].azimuth = clampedValue;
            break;
            
        case ElevationColumn:
            paramName = "elevation";
            clampedValue = juce::jlimit(-90.0f, 90.0f, newValue);
            pannerData[static_cast<size_t>(rowNumber)].elevation = clampedValue;
            break;
            
        case DivergeColumn:
            paramName = "diverge";
            clampedValue = juce::jlimit(0.0f, 100.0f, newValue);
            pannerData[static_cast<size_t>(rowNumber)].diverge = clampedValue;
            break;
            
        case OutGainColumn:
            paramName = "gain";
            clampedValue = juce::jlimit(-60.0f, 12.0f, newValue);
            pannerData[static_cast<size_t>(rowNumber)].gain = clampedValue;
            break;
            
        default:
            return;
    }
    
    // Send parameter update
    sendParameterUpdate(rowNumber, paramName, clampedValue);
    
    // Refresh the table
    table->repaintRow(rowNumber);
    
    editingRow = -1;
    editingColumn = -1;
}

void InputTracklistComponent::sendParameterUpdate(int rowNumber, const juce::String& paramName, float value)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(pannerData.size()))
        return;
    
    const auto& panner = pannerData[static_cast<size_t>(rowNumber)];
    
    DBG("[InputTracklistComponent] Parameter update: " + paramName + " = " + juce::String(value) + 
        " for panner " + juce::String(panner.name));
    
    // Method 1: Write to .mem command region (bi-directional via MemoryShare)
    if (panner.isMemoryShareBased && pannerManager != nullptr)
    {
        // TODO: Implement writing to .mem command region
        // The panner plugin should read this and update its state
        // For now, use sendParameterUpdate if available
        
        bool success = pannerManager->sendParameterUpdate(panner, paramName.toStdString(), value);
        if (success)
        {
            DBG("[InputTracklistComponent] Sent parameter via MemoryShare command region");
        }
        else
        {
            DBG("[InputTracklistComponent] MemoryShare parameter update not yet implemented");
        }
    }
    
    // Method 2: Send via OSC as fallback
    // TODO: Implement OSC parameter update if the existing system supports it
    // For now, call the callback if set
    if (onParameterChanged)
    {
        onParameterChanged(rowNumber, paramName, value);
    }
    
    // TODO: If neither method works, show a tooltip or status indicating 
    // that the parameter will be read from the plugin on next sync
}

} // namespace Mach1
