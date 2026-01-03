/*
    InputPanelContainer.cpp
    -----------------------
    Implementation of the view-switching container for input panel
*/

#include "InputPanelContainer.h"

namespace Mach1 {

//==============================================================================
InputPanelContainer::InputPanelContainer()
{
    // Create list view
    tracklistView = std::make_unique<InputTracklistComponent>();
    tracklistView->onSelectionChanged = [this](int index) {
        if (onSelectionChanged)
            onSelectionChanged(index);
        
        // Sync selection to mixer view
        if (mixerView)
            mixerView->setSelectedPanner(index);
    };
    tracklistView->onParameterChanged = [this](int pannerIndex, const juce::String& paramName, float value) {
        if (onParameterChanged)
            onParameterChanged(pannerIndex, paramName, value);
    };
    addChildComponent(tracklistView.get());
    
    // Create mixer view
    mixerView = std::make_unique<InputMixerComponent>();
    mixerView->onChannelSelected = [this](int index) {
        if (onSelectionChanged)
            onSelectionChanged(index);
        
        // Sync selection to list view
        if (tracklistView)
            tracklistView->setSelectedPanner(index);
    };
    mixerView->onGainChanged = [this](int channelIndex, float gain) {
        if (onGainChanged)
            onGainChanged(channelIndex, gain);
    };
    addChildComponent(mixerView.get());
    
    // Create toolbar buttons
    listViewButton = std::make_unique<juce::TextButton>("List");
    listViewButton->setClickingTogglesState(false);
    listViewButton->onClick = [this]() {
        setViewMode(InputPanelViewMode::List);
    };
    addAndMakeVisible(listViewButton.get());
    
    mixerViewButton = std::make_unique<juce::TextButton>("Mixer");
    mixerViewButton->setClickingTogglesState(false);
    mixerViewButton->onClick = [this]() {
        setViewMode(InputPanelViewMode::Mixer);
    };
    addAndMakeVisible(mixerViewButton.get());
    
    // Set default view
    setViewMode(InputPanelViewMode::List);
}

InputPanelContainer::~InputPanelContainer()
{
}

//==============================================================================
void InputPanelContainer::updatePannerData(const std::vector<PannerInfo>& panners)
{
    if (tracklistView)
        tracklistView->updatePannerData(panners);
    
    if (mixerView)
        mixerView->updatePannerData(panners);
}

void InputPanelContainer::setSelectedPanner(int index)
{
    if (tracklistView)
        tracklistView->setSelectedPanner(index);
    
    if (mixerView)
        mixerView->setSelectedPanner(index);
}

int InputPanelContainer::getSelectedPanner() const
{
    if (tracklistView)
        return tracklistView->getSelectedPanner();
    return -1;
}

void InputPanelContainer::updateLevelMeters(const std::vector<float>& levels)
{
    if (mixerView)
        mixerView->updateLevelMeters(levels);
}

void InputPanelContainer::setPannerTrackingManager(PannerTrackingManager* manager)
{
    if (tracklistView)
        tracklistView->setPannerTrackingManager(manager);
}

//==============================================================================
void InputPanelContainer::setViewMode(InputPanelViewMode mode)
{
    currentViewMode = mode;
    
    // Show/hide views
    tracklistView->setVisible(mode == InputPanelViewMode::List);
    mixerView->setVisible(mode == InputPanelViewMode::Mixer);
    
    // Update button states
    updateButtonStates();
    
    // Force repaint and resize
    resized();
    repaint();
}

void InputPanelContainer::updateButtonStates()
{
    bool isListView = (currentViewMode == InputPanelViewMode::List);
    
    // Style buttons based on active state
    listViewButton->setColour(juce::TextButton::buttonColourId, 
                              isListView ? buttonActiveColour : buttonColour);
    listViewButton->setColour(juce::TextButton::textColourOnId, 
                              isListView ? juce::Colour(0xFF1A1A1A) : textColour);
    listViewButton->setColour(juce::TextButton::textColourOffId, 
                              isListView ? juce::Colour(0xFF1A1A1A) : textColour);
    
    mixerViewButton->setColour(juce::TextButton::buttonColourId, 
                               !isListView ? buttonActiveColour : buttonColour);
    mixerViewButton->setColour(juce::TextButton::textColourOnId, 
                               !isListView ? juce::Colour(0xFF1A1A1A) : textColour);
    mixerViewButton->setColour(juce::TextButton::textColourOffId, 
                               !isListView ? juce::Colour(0xFF1A1A1A) : textColour);
    
    listViewButton->repaint();
    mixerViewButton->repaint();
}

//==============================================================================
void InputPanelContainer::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(backgroundColour);
    
    // Draw toolbar background
    auto toolbarBounds = getLocalBounds().removeFromTop(TOOLBAR_HEIGHT);
    g.setColour(toolbarColour);
    g.fillRect(toolbarBounds);
    
    // Draw separator line
    g.setColour(separatorColour);
    g.drawHorizontalLine(TOOLBAR_HEIGHT - 1, 0, static_cast<float>(getWidth()));
}

void InputPanelContainer::resized()
{
    auto bounds = getLocalBounds();
    
    // Toolbar area
    auto toolbarBounds = bounds.removeFromTop(TOOLBAR_HEIGHT);
    
    // Title on the left
    auto titleArea = toolbarBounds.removeFromLeft(100);
    
    // Buttons on the right
    auto buttonArea = toolbarBounds.removeFromRight(150);
    int buttonWidth = 60;
    int buttonHeight = 20;
    int buttonY = (TOOLBAR_HEIGHT - buttonHeight) / 2;
    
    mixerViewButton->setBounds(buttonArea.getRight() - buttonWidth - 5, buttonY, buttonWidth, buttonHeight);
    listViewButton->setBounds(buttonArea.getRight() - buttonWidth * 2 - 10, buttonY, buttonWidth, buttonHeight);
    
    // Content area (remaining bounds below toolbar)
    tracklistView->setBounds(bounds);
    mixerView->setBounds(bounds);
}

} // namespace Mach1

