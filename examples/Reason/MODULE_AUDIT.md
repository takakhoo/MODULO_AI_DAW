# MODULE_AUDIT.md

## Scope and method
Audit of DAW-relevant capabilities already present in this repo (Tracktion Engine + JUCE), with file/class pointers and minimal-integration plans for Reason. Sources include `modules/tracktion_engine`, `modules/tracktion_core`, `modules/tracktion_graph`, and JUCE examples/demos inside this repo. Searches were driven by `rg` for transport, edit save/load, plugins, automation, tempo, recording, rendering, and clip-slot/launcher systems.

---

## Inventory of DAW-relevant features (with integration notes)

### 1) Plugin scanning + known plugin list UI
- **Where in code**
  - `modules/tracktion_engine/plugins/tracktion_PluginManager.h` / `.cpp` — `tracktion::engine::PluginManager` holds `AudioPluginFormatManager pluginFormatManager` and `KnownPluginList knownPluginList`, persists list via `PropertyStorage`.
  - `modules/juce/modules/juce_audio_processors/scanning/juce_PluginListComponent.h` / `.cpp` — UI for scanning and managing plugin lists.
- **Example wiring**
  - `examples/DemoRunner/DemoRunner.h` — opens a dialog with `PluginListComponent`.
  - `modules/juce/examples/Plugins/HostPluginDemo.h` — known plugin list persistence patterns.
- **Integration difficulty**: EASY
- **Reason integration plan**
  - Add a “Plugins…” button in `TransportBarComponent`.
  - In `ReasonMainComponent`, open a `DialogWindow` with `juce::PluginListComponent` wired to `engine.getPluginManager()`.
  - Use `engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal")` and `engine.getPropertyStorage().getPropertiesFile()` as in DemoRunner.
  - Keep PluginList UI isolated; no DemoRunner classes.

---

### 2) External plugin hosting (AU/VST3)
- **Where in code**
  - `modules/tracktion_engine/plugins/external/tracktion_ExternalPlugin.h` / `.cpp` — wrapper for external plugins.
  - `modules/tracktion_engine/plugins/tracktion_PluginList.h` / `.cpp` — track/master plugin chains and insert/remove.
  - `modules/tracktion_engine/model/tracks/tracktion_AudioTrack.h` — `Track::pluginList` and `getVolumePlugin()`.
- **Example wiring**
  - `examples/Reason/demos/PluginDemo.h` — inserts plugins and uses `EditComponent` UI.
  - `examples/DemoRunner/demos/PluginDemo.h` — similar patterns.
- **Integration difficulty**: MED
- **Reason integration plan**
  - Add per-track “Insert Plugin…” menu.
  - Use `engine.getPluginManager().knownPluginList` to choose a `PluginDescription`.
  - Call `track->getPluginList().insertPlugin (...)` for the selected description.
  - (Later) add plugin UI via a dedicated plugin window in a single TU to avoid duplicate symbols.

---

### 3) Project/edit save + load
- **Where in code**
  - `modules/tracktion_engine/model/edit/tracktion_EditFileOperations.h` / `.cpp` — `EditFileOperations`, `createEmptyEdit`, `loadEditFromFile`.
  - `modules/tracktion_engine/utilities/tracktion_Engine.cpp` — `ProjectManager` lifecycle.
- **Example wiring**
  - `examples/Reason/demos/PluginDemo.h` — `createEmptyEdit`, `loadEditFromFile`, and `EditFileOperations::save`.
- **Integration difficulty**: EASY → MED (because the current Reason Edit is held by value)
- **Reason integration plan**
  - Store `std::unique_ptr<te::Edit>` in `SessionController`.
  - Add `createNewEdit`, `openEdit`, `saveEdit`, `saveEditAs`.
  - Add a “Project” menu in the transport bar with New/Open/Save/Save As.
  - Refresh track list + timeline after load/new.

---

### 4) Undo/redo model
- **Where in code**
  - `modules/tracktion_engine/model/edit/tracktion_Edit.h` — `getUndoManager()`.
  - `juce::UndoManager` in JUCE.
- **Example wiring**
  - `examples/Reason/SessionController.cpp` already uses `edit.getUndoManager()` to wrap clip moves.
- **Integration difficulty**: EASY
- **Reason integration plan**
  - Add `SessionController::undo()` / `redo()` that forward to `edit->getUndoManager()`.
  - Add Cmd+Z / Cmd+Shift+Z shortcuts in `ReasonMainComponent`.
  - Optional: add menu items in Project menu.

---

### 5) Transport control + loop/cycle, punch in/out
- **Where in code**
  - `modules/tracktion_engine/playback/tracktion_TransportControl.h` / `.cpp` — play/stop, loop range, record state, punch handling.
- **Example wiring**
  - `examples/Reason/demos/PlaybackDemo.h` — uses `transport.setLoopRange` and `transport.looping`.
- **Integration difficulty**: EASY
- **Reason integration plan**
  - Add a “Loop” toggle to transport bar.
  - Set `transport.looping = true/false` and update `transport.setLoopRange`.
  - Optional: use cursor + selection to set loop in/out.

---

### 6) Tempo sequence + bars/beats formatting
- **Where in code**
  - `modules/tracktion_engine/model/edit/tracktion_TempoSequence.h` / `.cpp` — `TempoSequence` and `toBarsAndBeats`.
  - `modules/tracktion_engine/model/edit/tracktion_TimecodeDisplayFormat.cpp` — bars/beats formatting rules.
- **Example wiring**
  - `examples/Reason/demos/AbletonLinkDemo.h` — `edit.tempoSequence.toBarsAndBeats`.
- **Integration difficulty**: MED
- **Reason integration plan**
  - Add an editable tempo label/box in the transport bar.
  - Update timeline ruler to use bars/beats formatting based on `tempoSequence`.
  - Fall back to seconds if tempo track is missing.

---

### 7) Recording (audio + MIDI)
- **Where in code**
  - `modules/tracktion_engine/playback/devices/tracktion_InputDevice.h` — input devices, recording preparation, punch in/out.
  - `modules/tracktion_engine/model/edit/tracktion_EditUtilities.h` — `prepareAndPunchRecord`, `punchOutRecording`, `assignTrackAsInput`.
  - `modules/tracktion_engine/model/tracks/tracktion_AudioTrack.h` — track input devices.
- **Example wiring**
  - `examples/Reason/demos/RecordingDemo.h` and `examples/Reason/demos/MidiRecordingDemo.h`.
- **Integration difficulty**: HARD (needs device selection + UI + threading)
- **Reason integration plan**
  - Add Record button and Arm buttons on tracks.
  - Use `EditUtilities` to assign input devices and call `transport.record()` or punch in/out.
  - Provide device selection UI and fail gracefully if no device is configured.

---

### 8) Render/export/bounce
- **Where in code**
  - `modules/tracktion_engine/model/export/tracktion_Renderer.h` / `.cpp` — `Renderer::renderToFile`.
  - `modules/tracktion_engine/model/export/tracktion_RenderOptions.h` / `.cpp`.
  - `modules/tracktion_engine/model/export/tracktion_ExportJob.h` / `.cpp`.
- **Example wiring**
  - Tests under `modules/tracktion_engine/utilities/tracktion_TestUtilities.h` show render helpers.
- **Integration difficulty**: MED
- **Reason integration plan**
  - Add “Export Mixdown…” menu item.
  - Build `RenderOptions` for stereo WAV, render in a background thread (UIBehaviour for progress).
  - Save to ProjectItem or file path; update UI on completion.

---

### 9) Automation lanes / parameter recording
- **Where in code**
  - `modules/tracktion_engine/model/automation/tracktion_AutomationCurve.h`
  - `modules/tracktion_engine/model/automation/tracktion_AutomatableParameter.h`
  - `modules/tracktion_engine/model/automation/tracktion_AutomationCurveList.h`
- **Example wiring**
  - Unit tests under `modules/tracktion_engine/model/automation`.
- **Integration difficulty**: HARD
- **Reason integration plan**
  - Add an Automation lane UI per track/plugin.
  - Bind `AutomatableParameter` to UI with record enable.
  - Use `AutomationCurveList` to draw and edit envelopes.

---

### 10) Clip slots / launcher + arranger track
- **Where in code**
  - `modules/tracktion_engine/model/tracks/tracktion_ClipSlot.h` / `.cpp`
  - `modules/tracktion_engine/model/clips/tracktion_LauncherClipPlaybackHandle.h` / `.cpp`
  - `modules/tracktion_engine/model/tracks/tracktion_ArrangerTrack.h` / `.cpp`
- **Example wiring**
  - `examples/Reason/demos/ClipLauncherDemo.h`
- **Integration difficulty**: MED → HARD
- **Reason integration plan**
  - Add a session-view grid with ClipSlots per track.
  - Use `ClipSlotList::ensureNumberOfSlots` and `LauncherClipPlaybackHandle`.
  - Sync to transport and allow quantized launch.

---

### 11) Master + track volume/pan plugins
- **Where in code**
  - `modules/tracktion_engine/plugins/internal/tracktion_VolumeAndPan.h` / `.cpp`
  - `modules/tracktion_engine/model/tracks/tracktion_AudioTrack.h` (`getVolumePlugin()`).
- **Example wiring**
  - `examples/Reason/SessionController.cpp` already maps sliders to `VolumeAndPanPlugin::setVolumeDb`.
- **Integration difficulty**: EASY
- **Reason integration plan**
  - Continue exposing per-track volume and add pan knob.
  - Add master strip mapped to `edit.getMasterVolumePlugin()`.

---

### 12) Selection management
- **Where in code**
  - `modules/tracktion_engine/selection/tracktion_SelectionManager.h`
- **Example wiring**
  - `examples/Reason/demos/PluginDemo.h` uses `SelectionManager`.
- **Integration difficulty**: MED
- **Reason integration plan**
  - Use SelectionManager for unified selection of clips, tracks, plugins.
  - Update timeline + track list selection to drive it.

---

## Best immediate wins (top 5)
1) **Project save/load** — unlocks real sessions and persistence with minimal API surface.
2) **Plugin scanning window** — enables discovery of installed AU/VST3 without refactoring.
3) **Master strip + pan controls** — small UI change, big perceived DAW completeness.
4) **Loop/cycle region** — simple toggle adds core DAW workflow.
5) **Tempo readout + bars/beats ruler** — strong musical UX improvement with existing tempo APIs.

### Why these are wins
- They reuse existing engine utilities with minimal risk.
- They are visible to users and make Reason feel “real”.
- They keep the `SessionController` boundary clean and extendable for AI features later.

---

## Immediate implementation shortlist (for tonight)
- **Project save/load** (EditFileOperations) — small UI menu + SessionController methods.
- **Plugin scanning window** (PluginListComponent) — simple dialog, no DemoRunner dependencies.
- **Optional**: Master strip + pan (extend track strips with a master section).

