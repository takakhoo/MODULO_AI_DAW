#include "PluginWindow.h"

namespace
{
[[ maybe_unused ]] bool isDPIAware (te::Plugin&)
{
    return true;
}

class AudioProcessorEditorContentComp : public te::Plugin::EditorComponent
{
public:
    explicit AudioProcessorEditorContentComp (te::ExternalPlugin& plug)
        : plugin (plug)
    {
        JUCE_AUTORELEASEPOOL
        {
            if (auto pi = plugin.getAudioPluginInstance())
            {
                editor.reset (pi->createEditorIfNeeded());

                if (editor == nullptr)
                    editor = std::make_unique<juce::GenericAudioProcessorEditor> (*pi);

                addAndMakeVisible (*editor);
            }
        }

        resizeToFitEditor (true);
    }

    bool allowWindowResizing() override { return false; }

    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override
    {
        if (editor == nullptr || allowWindowResizing())
            return {};

        return editor->getConstrainer();
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds (getLocalBounds());
    }

    void childBoundsChanged (juce::Component* c) override
    {
        if (c == editor.get())
        {
            plugin.edit.pluginChanged (plugin);
            resizeToFitEditor();
        }
    }

    void resizeToFitEditor (bool force = false)
    {
        if (force || ! allowWindowResizing())
            setSize (juce::jmax (8, editor != nullptr ? editor->getWidth() : 0),
                     juce::jmax (8, editor != nullptr ? editor->getHeight() : 0));
    }

private:
    te::ExternalPlugin& plugin;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessorEditorContentComp)
};

class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (te::Plugin& plug)
        : juce::DocumentWindow (plug.getName(), juce::Colours::black,
                                juce::DocumentWindow::closeButton, shouldAddPluginWindowToDesktop),
          plugin (plug),
          windowState (*plug.windowState)
    {
        getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
        setResizeLimits (100, 50, 4000, 4000);

        recreateEditor();
        setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());

       #if JUCE_LINUX
        setAlwaysOnTop (true);
        addToDesktop();
       #endif

        updateStoredBounds = true;
    }

    ~PluginWindow() override
    {
        updateStoredBounds = false;
        plugin.edit.flushPluginStateIfNeeded (plugin);
        setEditor (nullptr);
    }

    static std::unique_ptr<juce::Component> create (te::Plugin& plug)
    {
        if (auto externalPlugin = dynamic_cast<te::ExternalPlugin*> (&plug))
            if (externalPlugin->getAudioPluginInstance() == nullptr)
                return {};

        std::unique_ptr<PluginWindow> window;

        {
            struct Blocker : public juce::Component { void inputAttemptWhenModal() override {} };
            Blocker blocker;
            blocker.enterModalState (false);

           #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
            if (! isDPIAware (plug))
            {
                juce::ScopedDPIAwarenessDisabler disableDPIAwareness;
                window = std::make_unique<PluginWindow> (plug);
            }
            else
           #endif
            {
                window = std::make_unique<PluginWindow> (plug);
            }
        }

        if (window == nullptr || window->getEditor() == nullptr)
            return {};

        window->show();
        return window;
    }

    void show()
    {
        setVisible (true);
        toFront (false);
        setBoundsConstrained (getBounds());
    }

    void recreateEditor()
    {
        setEditor (nullptr);
        setEditor (plugin.createEditor());
    }

    void recreateEditorAsync()
    {
        setEditor (nullptr);

        juce::Timer::callAfterDelay (50, [this, sp = juce::Component::SafePointer<juce::Component> (this)]
        {
            if (sp != nullptr)
                recreateEditor();
        });
    }

    te::Plugin::EditorComponent* getEditor() const { return editor.get(); }

private:
    void setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor)
    {
        JUCE_AUTORELEASEPOOL
        {
            setConstrainer (nullptr);
            editor.reset();

            if (newEditor != nullptr)
            {
                editor = std::move (newEditor);
                setContentNonOwned (editor.get(), true);
            }

            setResizable (editor == nullptr || editor->allowWindowResizing(), false);

            if (editor != nullptr && editor->allowWindowResizing())
                setConstrainer (editor->getBoundsConstrainer());
        }
    }

    void moved() override
    {
        if (updateStoredBounds)
        {
            plugin.windowState->lastWindowBounds = getBounds();
            plugin.edit.pluginChanged (plugin);
        }
    }

    void userTriedToCloseWindow() override { plugin.windowState->closeWindowExplicitly(); }
    void closeButtonPressed() override { userTriedToCloseWindow(); }
    float getDesktopScaleFactor() const override { return 1.0f; }

    std::unique_ptr<te::Plugin::EditorComponent> editor;
    te::Plugin& plugin;
    te::PluginWindowState& windowState;
    bool updateStoredBounds = false;

   #if JUCE_LINUX
    static constexpr bool shouldAddPluginWindowToDesktop = false;
   #else
    static constexpr bool shouldAddPluginWindowToDesktop = true;
   #endif
};

std::unique_ptr<juce::Component> createPluginWindowInternal (te::Plugin& plugin)
{
    return PluginWindow::create (plugin);
}

} // namespace

std::unique_ptr<juce::Component> createReasonPluginWindow (te::PluginWindowState& state)
{
    if (auto* windowState = dynamic_cast<te::Plugin::WindowState*> (&state))
        return createPluginWindowInternal (windowState->plugin);

    return {};
}

void recreateReasonPluginWindowContentAsync (te::Plugin& plugin)
{
    if (plugin.windowState == nullptr)
        return;

    if (auto* window = dynamic_cast<PluginWindow*> (plugin.windowState->pluginWindow.get()))
        window->recreateEditorAsync();
}
