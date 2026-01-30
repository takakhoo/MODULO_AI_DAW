#include "FxInspectorComponent.h"

namespace
{
constexpr int kSlotHeight = 28;
}

class FxInspectorComponent::SlotComponent : public juce::Component
{
public:
    SlotComponent()
    {
        addAndMakeVisible (enableButton);
        addAndMakeVisible (nameButton);
        addAndMakeVisible (removeButton);

        enableButton.setClickingTogglesState (true);
        enableButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2F3A));
        enableButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        enableButton.onClick = [this]
        {
            if (onEnabledChanged)
                onEnabledChanged (slotId, enableButton.getToggleState());
        };

        nameButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF1F232A));
        nameButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        nameButton.onClick = [this]
        {
            if (onSelected)
                onSelected (slotId);
        };

        removeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3B2A2A));
        removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        removeButton.onClick = [this]
        {
            if (onRemoved)
                onRemoved (slotId);
        };
    }

    void setSlot (uint64_t id, const juce::String& name, bool enabled, bool selected)
    {
        slotId = id;
        nameButton.setButtonText (name);
        enableButton.setToggleState (enabled, juce::dontSendNotification);
        const auto enabledColour = enabled ? juce::Colour (0xFF246B63) : juce::Colour (0xFF2A2F3A);
        enableButton.setColour (juce::TextButton::buttonColourId, enabledColour);

        auto nameColour = selected ? juce::Colour (0xFF2D465F) : juce::Colour (0xFF1F232A);
        nameButton.setColour (juce::TextButton::buttonColourId, nameColour);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 3);
        enableButton.setBounds (r.removeFromLeft (26));
        removeButton.setBounds (r.removeFromRight (24));
        nameButton.setBounds (r);
    }

    std::function<void (uint64_t)> onSelected;
    std::function<void (uint64_t, bool)> onEnabledChanged;
    std::function<void (uint64_t)> onRemoved;

private:
    uint64_t slotId = 0;
    juce::TextButton enableButton { "" };
    juce::TextButton nameButton;
    juce::TextButton removeButton { "X" };
};

FxInspectorComponent::FxInspectorComponent()
{
    addAndMakeVisible (headerLabel);
    addAndMakeVisible (listViewport);
    listViewport.setViewedComponent (&listContainer, false);
    listViewport.setScrollBarsShown (true, false);

    addAndMakeVisible (emptyLabel);
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    emptyLabel.setText ("No FX on this track", juce::dontSendNotification);

    addAndMakeVisible (editorPlaceholder);
    editorPlaceholder.setJustificationType (juce::Justification::centred);
    editorPlaceholder.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    editorPlaceholder.setText ("Select an effect to edit", juce::dontSendNotification);

    headerLabel.setText ("FX", juce::dontSendNotification);
    headerLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    headerLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
}

FxInspectorComponent::~FxInspectorComponent() = default;

void FxInspectorComponent::setTrackName (const juce::String& name)
{
    headerLabel.setText (name.isNotEmpty() ? ("FX: " + name) : "FX", juce::dontSendNotification);
}

void FxInspectorComponent::setSlots (const std::vector<Slot>& newSlots)
{
    slots = newSlots;
    rebuildSlots();
}

void FxInspectorComponent::setSelectedSlot (uint64_t slotId)
{
    selectedSlotId = slotId;
    rebuildSlots();
}

void FxInspectorComponent::setEditor (std::unique_ptr<te::Plugin::EditorComponent> editorToUse)
{
    editor.reset();
    editorPlaceholder.setVisible (true);

    if (editorToUse != nullptr)
    {
        editor = std::move (editorToUse);
        addAndMakeVisible (editor.get());
        editorPlaceholder.setVisible (false);
    }

    resized();
}

void FxInspectorComponent::clearEditor()
{
    editor.reset();
    editorPlaceholder.setVisible (true);
    resized();
}

void FxInspectorComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF1C1F25));

    g.setColour (juce::Colour (0xFF2B2F36));
    g.fillRect (getLocalBounds().removeFromTop (28));
    g.setColour (juce::Colour (0xFF353A44));
    g.drawLine (0.0f, 28.0f, (float) getWidth(), 28.0f);
}

void FxInspectorComponent::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (28);
    headerLabel.setBounds (header.reduced (8, 2));

    auto listArea = r.removeFromTop (juce::jmin (getHeight() / 2, 220));
    listViewport.setBounds (listArea);
    emptyLabel.setBounds (listArea.reduced (8));

    auto editorArea = r.reduced (6);
    editorPlaceholder.setBounds (editorArea);
    if (editor != nullptr)
        editor->setBounds (editorArea);

    rebuildSlots();
}

void FxInspectorComponent::rebuildSlots()
{
    slotComponents.clear();
    listContainer.removeAllChildren();

    const int width = juce::jmax (1, listViewport.getWidth());
    int y = 0;
    for (const auto& slot : slots)
    {
        auto comp = std::make_unique<SlotComponent>();
        auto* compPtr = comp.get();
        slotComponents.push_back (std::move (comp));

        compPtr->setSlot (slot.id, slot.name, slot.enabled, slot.id == selectedSlotId);
        compPtr->onSelected = [this] (uint64_t id)
        {
            selectedSlotId = id;
            if (onSlotSelected)
                onSlotSelected (id);
            rebuildSlots();
        };
        compPtr->onEnabledChanged = [this] (uint64_t id, bool enabled)
        {
            if (onSlotEnabledChanged)
                onSlotEnabledChanged (id, enabled);
        };
        compPtr->onRemoved = [this] (uint64_t id)
        {
            if (onSlotRemoved)
                onSlotRemoved (id);
        };

        compPtr->setBounds (0, y, width, kSlotHeight);
        listContainer.addAndMakeVisible (compPtr);
        y += kSlotHeight + 4;
    }

    listContainer.setSize (width, juce::jmax (y, listViewport.getHeight()));
    emptyLabel.setVisible (slots.empty());
}
