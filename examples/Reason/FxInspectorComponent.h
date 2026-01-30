#pragma once

#include <JuceHeader.h>

#include "SessionController.h"

class FxInspectorComponent : public juce::Component
{
public:
    struct Slot
    {
        uint64_t id = 0;
        juce::String name;
        bool enabled = true;
    };

    FxInspectorComponent();
    ~FxInspectorComponent() override;

    void setTrackName (const juce::String& name);
    void setSlots (const std::vector<Slot>& newSlots);
    void setSelectedSlot (uint64_t slotId);
    uint64_t getSelectedSlot() const noexcept { return selectedSlotId; }

    void setEditor (std::unique_ptr<te::Plugin::EditorComponent> editorToUse);
    void clearEditor();

    std::function<void (uint64_t)> onSlotSelected;
    std::function<void (uint64_t, bool)> onSlotEnabledChanged;
    std::function<void (uint64_t)> onSlotRemoved;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class SlotComponent;

    void rebuildSlots();

    juce::Label headerLabel;
    juce::Viewport listViewport;
    juce::Component listContainer;
    juce::Label emptyLabel;

    std::unique_ptr<te::Plugin::EditorComponent> editor;
    juce::Label editorPlaceholder;

    std::vector<Slot> slots;
    std::vector<std::unique_ptr<SlotComponent>> slotComponents;
    uint64_t selectedSlotId = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxInspectorComponent)
};
