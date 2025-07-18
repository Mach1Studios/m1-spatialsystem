/*
    InputTracklistComponent.cpp
    ---------------------------
    Implementation of the panner table component
*/

#include "InputTracklistComponent.h"

namespace Mach1 {

//==============================================================================
InputTracklistComponent::InputTracklistComponent()
{
    // Create title label
    titleLabel = std::make_unique<juce::Label>("title", "Input Tracklist");
    titleLabel->setFont(juce::Font(16.0f, juce::Font::bold));
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
    
    header.addColumn("Index", IndexColumn, 50, 30, 100);
    header.addColumn("Name", NameColumn, 120, 80, 200);
    header.addColumn("Channels", ChannelsColumn, 60, 50, 80);
    header.addColumn("Azimuth", AzimuthColumn, 80, 60, 120);
    header.addColumn("Elevation", ElevationColumn, 80, 60, 120);
    header.addColumn("Diverge", DivergeColumn, 80, 60, 120);
    header.addColumn("Out Gain (dB)", OutGainColumn, 100, 80, 150);
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
    if (index >= 0 && index < pannerData.size())
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
        g.setColour(backgroundColour.brighter(0.1f));
        g.fillRect(0, 0, width, height);
    }
}

void InputTracklistComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= pannerData.size())
        return;
    
    const auto& panner = pannerData[rowNumber];
    
    // Set text colour
    g.setColour(textColour);
    g.setFont(juce::Font(12.0f));
    
    // Get cell text
    juce::String cellText = getColumnText(rowNumber, columnId);
    
    // Draw text
    g.drawText(cellText, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
    
    // Draw tracking method indicator for name column
    if (columnId == NameColumn)
    {
        juce::Colour indicatorColour = panner.isMemoryShareBased ? memoryShareIndicatorColour : oscIndicatorColour;
        g.setColour(indicatorColour);
        g.fillEllipse(width - 12, height / 2 - 3, 6, 6);
    }
}

void InputTracklistComponent::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (rowNumber >= 0 && rowNumber < pannerData.size())
    {
        selectedPannerIndex = rowNumber;
        table->selectRow(rowNumber);
        
        if (onSelectionChanged)
            onSelectionChanged(rowNumber);
    }
}

void InputTracklistComponent::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    // Could open panner details or settings
    cellClicked(rowNumber, columnId, event);
}

//==============================================================================
juce::String InputTracklistComponent::getColumnText(int rowNumber, int columnId) const
{
    if (rowNumber >= pannerData.size())
        return {};
    
    const auto& panner = pannerData[rowNumber];
    
    switch (columnId)
    {
        case IndexColumn:
            return juce::String(rowNumber + 1);
            
        case NameColumn:
            return panner.name;
            
        case ChannelsColumn:
            return juce::String(panner.channels);
            
        case AzimuthColumn:
            return juce::String(panner.azimuth, 1) + "°";
            
        case ElevationColumn:
            return juce::String(panner.elevation, 1) + "°";
            
        case DivergeColumn:
            return juce::String(panner.diverge, 2);
            
        case OutGainColumn:
            return juce::String(panner.gain, 1) + " dB";
            
        default:
            return {};
    }
}

} // namespace Mach1 