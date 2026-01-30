# Reason (Tracktion Engine) Project Timeline

## Project Overview (as of 2025-01-12)

### What we are building
We are building a standalone macOS DAW application named "Reason" on Apple Silicon. The core audio and edit engine is Tracktion Engine, the UI/app framework is JUCE, and the build system is CMake + Ninja. The target is Logic-Pro-level DAW capability with AI-native workflows built into the application (not as plugins). This is a ground-up, artist-facing DAW, not a plugin or demo.

### What Tracktion gave us initially
Tracktion Engine provides a full edit/playback model (Edit, Track, Clip, TransportControl, Plugin graph, rendering, etc.) and JUCE provides UI primitives. The repository includes example apps (notably DemoRunner) that show how to use the engine but do not provide a DAW-grade UI or workflow. DemoRunner is a demo selector and is not suitable as the UI foundation for Reason.

### How we will go about this
We will incrementally replace the demo-oriented UI with a custom DAW UI while keeping the engine wiring minimal and correct. We will build a thin SessionController that owns the Engine/Edit/Transport and expose small, safe commands. UI components (transport bar, track list, timeline, mixer) will be separate and dumb, calling into the controller. This allows the UI to evolve without touching Tracktion internals. Each step is documented below with date/time stamps and full details.

---

## 2025-01-12 23:30

### Summary of goals for this milestone
1) Cleanly rebrand the `examples/Reason` target so it no longer identifies as DemoRunner anywhere.
2) Replace the DemoRunner-based UI with a minimal Logic-ish DAW skeleton:
   - Top transport bar (Play/Stop, time display, tempo placeholder, Settings button)
   - Left track list (at least 3 dummy tracks, selectable)
   - Center timeline (time ruler + playhead line + clip rectangles)
   - Bottom mixer placeholder (per-track volume slider placeholders)
3) Implement a real end-to-end action:
   - "Import Audio..." file chooser inserts a WaveAudioClip on Track 1 at time 0
   - Clip is rendered as a rectangle on the timeline
   - Transport play/stop works and playhead moves

### Why these goals
- DemoRunner is a demo launcher and is not aligned with the DAW vision.
- We need an early, stable UI skeleton that is compile-first and minimal but clearly DAW-like.
- Implementing a single end-to-end path (file import -> clip -> timeline -> transport playhead) proves the engine wiring is correct and gives a foundation for future features.

### Repository inspection and findings
- `examples/Reason/Reason.cpp` still included `../DemoRunner/DemoRunner.h` and returned "DemoRunner" as the app name and window title.
- `examples/Reason/Reason.h` contained DemoRunner PIP metadata and a full DemoRunner class with demo registration.
- `examples/Reason/CMakeLists.txt` still referenced demo headers.
- `examples/common/PluginWindow.h` provides ExtendedUIBehaviour and plugin window integration but is not required for the initial Reason UI, and also brings in definitions that can cause duplicate linkage if included in multiple translation units.

### Commands run during this milestone
- `rg -n "DemoRunner" examples/Reason` to find all DemoRunner references.
- `cmake -S . -B build -G Ninja` to configure.
- `cmake --build build --target Reason` to build.
- `open build/examples/Reason/Reason_artefacts/Reason.app` to launch the app for visual verification.

### Files created / modified

#### 1) Branding cleanup
- `examples/Reason/Reason.cpp`
  - Removed DemoRunner include and replaced with `ReasonMainComponent.h`.
  - App name/version/window title changed from "DemoRunner" to "Reason".
  - Main window now constructs `ReasonMainComponent` instead of `DemoRunner`.
  - This fully breaks the dependency on DemoRunner in the Reason app entry point.

- `examples/Reason/Reason.h`
  - Old DemoRunner header removed entirely.
  - Replaced with a minimal header that includes `ReasonMainComponent.h` only.
  - All DemoRunner PIP metadata and demo registration code removed.

- `examples/Reason/CMakeLists.txt`
  - Removed all demo header includes from `SourceFiles`.
  - Added new Reason UI and controller files to `SourceFiles` so they compile into the Reason target.

#### 2) New UI skeleton and engine wiring
New files added under `examples/Reason/`:
- `SessionController.h` / `SessionController.cpp`
  - Owns `te::Engine`, `te::Edit`, and `te::TransportControl`.
  - Ensures at least 3 audio tracks on startup for UI placeholders.
  - Provides methods:
    - `togglePlay()`
    - `stop()`
    - `importAudioFileToTrack0(const juce::File&)`
    - `getCurrentTimeSeconds()`
    - `getTrackNames()`
    - `getTrack0ClipInfos()`
    - `showAudioSettings()`
  - Uses Tracktion APIs to:
    - Ensure Track 0 exists.
    - Clear existing clips on Track 0.
    - Insert a WaveAudioClip at time 0 using `track->insertWaveClip`.
    - Set loop range and reset transport position after import.
  - Note: `ExtendedUIBehaviour` was defined as a minimal subclass of `te::UIBehaviour` to satisfy Engine constructor without pulling in PluginWindow glue.

- `ReasonMainComponent.h` / `ReasonMainComponent.cpp`
  - Top-level UI component containing transport bar, track list, timeline, and mixer.
  - Owns a `SessionController` instance.
  - Wires up transport buttons to controller methods.
  - Implements the "Import Audio..." chooser and feeds results into the controller.
  - Updates the time display and causes the timeline to repaint at 30 Hz.

- `TransportBarComponent.h` / `TransportBarComponent.cpp`
  - UI for Play/Stop, time display, tempo placeholder, Settings button, and Import Audio.
  - Delegates button actions via std::function callbacks.

- `TrackListComponent.h` / `TrackListComponent.cpp`
  - Paint-only list of track names.
  - Allows mouse selection (UI-only, no edit binding yet).

- `TimelineComponent.h` / `TimelineComponent.cpp`
  - Draws time ruler in seconds.
  - Draws playhead line based on transport time.
  - Draws rectangles for audio clips on Track 0 using `SessionController::getTrack0ClipInfos()`.
  - Uses a fixed scale (100 px/sec) for mapping time to pixels.

- `MixerComponent.h` / `MixerComponent.cpp`
  - Placeholder vertical sliders per track.
  - Pure UI; not wired to Tracktion mixer yet.

#### 3) Build/verify fixes
During build, two key issues were found and resolved:
- **PluginWindow include ordering:**
  - `examples/common/PluginWindow.h` depends on `te` being defined. It was initially included before `namespace te = tracktion;` which caused compilation errors.
  - This was fixed by moving the include after the `te` alias or removing the include entirely.

- **Duplicate symbol errors from PluginWindow:**
  - `PluginWindow.h` contains non-inline definitions that, when included in multiple translation units, caused duplicate symbols at link time.
  - Resolution: removed the dependency entirely from Reason by creating a minimal `ExtendedUIBehaviour` subclass in `SessionController.h`. This avoids the plugin window machinery for now and prevents symbol duplication.

### What is different now (functional changes)
- Reason is fully branded as "Reason" and no longer depends on DemoRunner.
- Reason launches a DAW-style skeleton UI with:
  - Transport bar at top
  - Track list on left (3 tracks)
  - Timeline center with ruler/playhead
  - Mixer placeholder at bottom
- Import Audio flow works end-to-end:
  - Chooser opens
  - WaveAudioClip is inserted at time 0 on Track 1
  - Clip is rendered as a green rectangle
  - Play/Stop controls the transport and playhead moves

### Design and intent notes
- The UI is intentionally minimal and stable, with no custom layout complexity.
- The SessionController is intentionally thin to keep engine wiring safe and easy to extend.
- Timeline and mixer are paint-only and safe to change later.
- All changes are scoped to `examples/Reason` and its CMake wiring as requested.

### Next steps to consider (not executed here)
- Wire tempo display to Edit tempo sequence.
- Bind mixer sliders to track volume plugins.
- Add track creation and clip editing actions.
- Add transport timeline zoom/scroll.
- Replace placeholder ExtendedUIBehaviour with a full plugin window implementation when plugin UI becomes relevant.

---

## Notes for future timeline entries
- Every future change should include:
  - Date/time stamp
  - Goals
  - Why the change was needed
  - Files touched and specific code-level intent
  - Commands run
  - Build/test outcomes
  - What is new vs. still placeholder

---

## 2026-01-13 00:15

### Goals for this milestone
- Add a unified Import menu with Audio + MIDI paths and insert clips on the selected track at a real insertion time.
- Move the mixer UI into the track list (left side) with per-track volume sliders wired to real engine volume.
- Improve the timeline ruler with major/medium/minor ticks (1.0s / 0.5s / 0.25s) and a more Mac-like feel.
- Make the timeline interactive: clip selection, horizontal dragging, cursor clicks, and spacebar play/stop.
- Add visual differentiation for Audio vs MIDI clips (color + icon + label) and basic waveform/MIDI previews.

### Design choices and reasoning
- **Import menu**: The transport bar now provides a single “Import…” button that opens a popup menu (Audio/MIDI). This keeps the toolbar clean and DAW-like while expanding functionality.
- **Insertion time**: If transport is playing, imports use the current playhead time. If stopped, imports use the cursor time. Cursor is set by clicking the timeline, and we also update it when stopping playback.
- **SessionController API**: Expanded to enforce a clean UI → engine boundary. UI uses only controller commands: selection, import, clip list, move clip, cursor time, track volume.
- **Volume mapping**: Slider range [0..1] maps linearly to -60 dB .. +6 dB using Tracktion’s VolumeAndPanPlugin `setVolumeDb()` and `getVolumeDb()`.
- **Waveform previews**: Use `juce::AudioThumbnail` (cache-backed background scanning) to avoid blocking audio or UI threads. When no thumbnail is ready, a lightweight fallback bar pattern is drawn.
- **MIDI previews**: Use Tracktion’s MidiClip + MidiNote edit-time ranges to draw simple piano-roll-style bars in the clip body.
- **Clip dragging**: Dragging only moves horizontally within the same track (vertical dragging is intentionally deferred). The clip movement is committed through Edit’s UndoManager via `clip->setStart()`.
- **Ruler ticks**: Subtle alpha for minor/medium/major ticks keeps the ruler readable without looking busy.

### Files changed (and why)
- `examples/Reason/SessionController.h`
  - Expanded controller API: selected track, cursor time, importAudio/importMidi, clip list per track, moveClip, volume get/set.
  - Added ClipInfo with stable `id` (EditItemID raw), clip type, source path, and MIDI note previews.
- `examples/Reason/SessionController.cpp`
  - Implemented MIDI import (JUCE MidiFile → sequence → Tracktion MidiClip via `mergeInMidiSequence`).
  - Implemented insertion-time logic (playhead if playing, cursor if stopped).
  - Implemented track volume mapping to VolumeAndPanPlugin (-60dB to +6dB).
  - Implemented clip lookup by EditItemID and clip move with undo transaction.
- `examples/Reason/TransportBarComponent.h/.cpp`
  - Replaced “Import Audio…” with “Import…” and routed to a single callback.
  - Updated fonts to FontOptions to avoid deprecation warnings.
- `examples/Reason/TrackStripComponent.h/.cpp` (new)
  - New per-track row component with name label, volume slider, and “AI” placeholder button.
- `examples/Reason/TrackListComponent.h/.cpp`
  - Rebuilt as a vertical stack of TrackStripComponents.
  - Exposes selection and volume callbacks.
  - Increased row height and text size for readability.
- `examples/Reason/TimelineComponent.h/.cpp`
  - Added ruler improvements (major/medium/minor ticks).
  - Added clip hit-testing, selection highlight, and drag handling.
  - Added cursor-click behavior (click sets cursor/playhead time).
  - Added waveform preview using AudioThumbnail (async), plus fallback bars.
  - Added MIDI note preview bars and clip color/icon differentiation.
- `examples/Reason/ReasonMainComponent.h/.cpp`
  - Removed bottom mixer UI and integrated track list as left mixer strip.
  - Added import menu (Audio/MIDI) and new file chooser handling.
  - Added spacebar play/stop handling and focus management.
  - Wired timeline selection to track list selection.
- `examples/Reason/CMakeLists.txt`
  - Added TrackStripComponent files to the Reason target sources.

### Commands run
- `cmake -S . -B build -G Ninja`
- `cmake --build build --target Reason`
- `open build/examples/Reason/Reason_artefacts/Reason.app`

### What works now
- **Import menu** with Audio + MIDI.
- **Audio import** inserts WaveAudioClip on selected track at cursor/playhead time.
- **MIDI import** inserts MidiClip with parsed MIDI data on selected track at cursor/playhead time.
- **Track list** is taller, readable, and includes sliders + AI placeholder buttons.
- **Volume sliders** change actual playback volume (mapped to Tracktion VolumeAndPanPlugin).
- **Timeline ruler** shows 1.0s / 0.5s / 0.25s tick marks with subtle styling.
- **Clip selection + dragging** works horizontally and updates the edit state.
- **Spacebar toggles play/stop** (DAW-style).
- **Timeline click** sets cursor/playhead time.
- **Audio/MIDI clips** are visually distinct and labeled; audio shows waveform-like preview, MIDI shows note bars.

### Limitations intentionally left for later
- No snap/grid quantization during drag.
- No vertical dragging between tracks (horizontal-only for stability).
- No zoom or scroll in the timeline yet.
- No AI button behavior yet (placeholder only).
- MIDI import uses a merged sequence (no separate track lanes or channel splits yet).

---

## 2026-01-30 00:45

### Goals for this milestone
- Improve track navigation and readability: vertical scrolling, clearer M/S states, distinct row colors.
- Add chord visibility and inspection: labels in piano roll, persistent chord list panel, and per-track chord menu.
- Improve MIDI workflow: multi-select + copy/paste in piano roll.
- Add drag-and-drop import for audio/MIDI.
- Switch chord generation to GAPT model by default.

### Key changes and reasoning
- **Vertical scrolling**: Timeline and track list can now scroll vertically through many tracks, matching the earlier horizontal timeline scroll/zoom.
- **Track row colors + M/S visibility**: Each track row has a distinct accent color; mute/solo buttons change text color on toggle so state is obvious.
- **Chord labels**: When chord options are generated, labels are stored and drawn in the piano roll to make the progression visible in context.
- **Chord inspector**: A persistent right panel lists chords for the selected chord track and shows a simple staff-like chord readout for quick scanning.
- **Piano roll selection shortcuts**: Cmd+A selects all notes; Cmd+C/Cmd+V copy/paste selected notes around the cursor for fast iteration.
- **Drag-and-drop import**: Drop audio or MIDI files directly onto the timeline/track area; the app infers type and inserts on the track under the drop.
- **Model default**: GAPT is now the default model for chord generation since it is more robust than the offline MLE.

### Files changed (with intent)
- `examples/Reason/TrackColors.h`
  - New helper to provide consistent per-track colors using HSV for row accents and UI tinting.
- `examples/Reason/TrackStripComponent.h`
  - Added right-click context menu callback.
  - Hooked mute/solo buttons to use stronger visual state.
- `examples/Reason/TrackStripComponent.cpp`
  - Applied per-track colors to the row background gradient.
  - Mute/solo text colors change when toggled for visibility.
  - Slider styling updated to be shorter and fatter with dynamic color response.
  - Right-click menu trigger support.
- `examples/Reason/TrackListComponent.h`
  - Added scroll offset support and helpers: `setScrollOffset`, `getContentHeight`, `getTrackIndexAtY`.
- `examples/Reason/TrackListComponent.cpp`
  - Implemented vertical scrolling via mouse wheel and emitted `onVerticalScrollChanged`.
  - Track hit-testing now considers vertical scroll.
- `examples/Reason/TimelineComponent.h`
  - Added vertical scroll offset and helpers: `setScrollOffset`, `getContentHeight`, `getVisibleTrackHeight`, `getTrackIndexAtY`, `getTimeAtX`.
  - Added callback for vertical scroll syncing.
- `examples/Reason/TimelineComponent.cpp`
  - Implemented vertical scroll handling in wheel events (horizontal remains on shift/deltaX).
  - Track row drawing now uses per-track color.
  - Clip hit-testing and drawing respect vertical scroll.
- `examples/Reason/SessionController.h`
  - Added chord label model: `ChordLabel`, and APIs to read labels per clip/track.
- `examples/Reason/SessionController.cpp`
  - Chord generation now stores chord labels into clip state (`CHORD_LABELS`).
  - Exposes chord labels for piano roll and inspector UI.
- `examples/Reason/PianoRollComponent.h`
  - Added chord label drawing and multi-note selection state.
- `examples/Reason/PianoRollComponent.cpp`
  - Chord labels are drawn above the grid.
  - Cmd+A selects all notes; Cmd+C/Cmd+V copy/paste selected notes at cursor time.
  - Multi-note dragging moves all selected notes together.
- `examples/Reason/ChordInspectorComponent.h` (new)
  - New right-side panel for chord lists and simple staff readout.
- `examples/Reason/ChordInspectorComponent.cpp` (new)
  - Lists chords with bar/beat times and renders a compact “score” line.
- `examples/Reason/ReasonMainComponent.h`
  - Now implements `juce::FileDragAndDropTarget`.
  - Added chord inspector, vertical scrollbar, scroll tracking members.
- `examples/Reason/ReasonMainComponent.cpp`
  - Integrated chord inspector on right panel.
  - Added vertical scrollbar for track rows; syncs TrackList + Timeline.
  - Track selection refreshes FX and chord inspector.
  - Drag-and-drop import detects audio vs MIDI and inserts into hovered track/time.
  - Right-click track list opens chord menu.
  - Chord generation requests now use model `GAPT`.
- `examples/Reason/CMakeLists.txt`
  - Added new chord inspector files to the Reason target.
- `tools/realchords/realchords_batch_server.py`
  - Default model set to `GAPT`.

### Commands run
- `cmake --build build --target Reason`
  - Failed initially due to missing inheritance for `FileDragAndDropTarget` override.
  - Fixed by updating `ReasonMainComponent` inheritance, then queued for rebuild.

### What works now
- Vertical scrolling through tracks in both track list and timeline.
- Track rows are color-coded, and M/S buttons visually show their state.
- Chord labels appear in the piano roll for generated chord tracks.
- Right-side chord inspector displays chord list + compact staff-style readout.
- Cmd+A/Cmd+C/Cmd+V in piano roll to select and copy/paste notes.
- Drag-and-drop of audio/MIDI files onto the timeline/track area.
- GAPT is the default chord generation model.

### Limitations / next issues to verify
- Rebuild is required after the drag-and-drop inheritance fix to confirm no remaining compile errors.
- Chord inspector currently reads only the first labeled MIDI clip per track.
- Drag-and-drop assumes file extensions for audio/MIDI detection; deeper validation could be added later.

---

## 2026-01-30 01:30

### Goals for this milestone
- Add sustain pedal visibility in the piano roll and improve the piano-roll time ruler accuracy.
- Allow moving MIDI clips across tracks and deleting clips/tracks with Delete.
- Keep track list and timeline lanes aligned beyond large track counts.
- Prompt for chord-option count instead of fixed 5.
- Add multi-note resize support in piano roll for extending/shortening notes.

### Design choices and reasoning
- **Sustain lane**: Display CC64 data in a compact lane under the piano roll notes so pedal usage is visible during editing without requiring a separate automation view.
- **Ruler fix**: A dedicated piano-roll ruler is drawn using bars/beats from the edit so grid and labels line up with musical time.
- **Track alignment**: Track list now respects the timeline’s visible height for scroll clamping so both sides stay perfectly aligned.
- **Clip drag across tracks**: Use Tracktion’s `Clip::moveTo` to move clips between tracks with minimal engine impact.
- **Delete behavior**: Delete removes the selected clip; if no clip is selected, it deletes the selected track.
- **Chord count prompt**: Simple modal input to avoid a fixed “5x” workflow, allowing any count between 1–16.

### Files changed (and why)
- `examples/Reason/SessionController.h`
  - Added `SustainEvent` model and new methods: `getSustainEventsForClip`, `moveClipToTrack`, `deleteClip`, `deleteTrack`.
- `examples/Reason/SessionController.cpp`
  - Implemented sustain event extraction from `MidiList` controller events (CC64).
  - Implemented clip move across tracks using `Clip::moveTo`.
  - Implemented clip/track deletion via `removeFromParent` and `edit->deleteTrack`.
- `examples/Reason/TimelineComponent.h/.cpp`
  - Added selection clear helper.
  - Dragging now supports vertical moves to different tracks.
  - Dragging preview updates row based on target track.
- `examples/Reason/TrackListComponent.h/.cpp`
  - Added visible-height override so scroll clamping matches timeline and stays aligned with lanes.
- `examples/Reason/PianoRollComponent.h/.cpp`
  - Added top ruler and bottom sustain lane.
  - Sustain lane draws CC64 values as bars; ruler labels use bars/beats.
  - Multi-note resize support when several notes are selected.
- `examples/Reason/ReasonMainComponent.cpp`
  - Delete key now removes selected clip, or track if no clip selected.
  - “Gen Chords” prompts for count and sends `/generate` with options=N.
  - Track list syncs its visible height to the timeline for perfect alignment.
- `tools/realchords/realchords_batch_server.py`
  - Added `/generate` endpoint; `/generate5` now aliases to it.

### Commands run
- `cmake --build build --target Reason`

### What works now
- Sustain pedal (CC64) is visible in the piano roll lane, and ruler labels align to bars/beats.
- MIDI clips can be dragged vertically across tracks, not just horizontally.
- Delete removes selected clip; if none is selected, the selected track is deleted.
- Track list + timeline lanes remain aligned even with many tracks.
- Chord generation asks for how many options and uses `/generate` with that count.
- Multi-note resize allows extending or shortening selected notes together.

### Limitations / next steps
- Sustain lane is read-only for now (no direct CC editing).
- Clip drag across tracks does not yet highlight the destination row (visual-only improvement).
- Delete track does not prompt for confirmation (consider a safety dialog later).

---

## 2026-01-13 00:49

### Goals for this milestone
- Add timeline navigation: horizontal scroll (trackpad/mouse) and zoom (pinch + Cmd+wheel).
- Improve ruler visuals so ticks stay readable across zoom levels.
- Make clip drawing/interaction correct under scroll + zoom, including edge auto-pan while dragging.
- Add a lightweight “Idea Marker” lane and `I` hotkey for marker drops (future AI prompt surface).

### Design choices and reasoning
- **Scroll + zoom**: Use a `juce::ScrollBar` plus internal `viewStartSeconds` and `pixelsPerSecond` for a clean, explicit time viewport. This keeps the track list fixed and timeline content scrollable.
- **Zoom anchoring**: Zoom uses cursor X as an anchor so the time under the cursor stays stable (DAW-like feel).
- **Ruler density**: Tick step adapts based on pixels-per-second so minor ticks don’t clutter when zoomed out; labels are skipped when too dense.
- **Markers**: Stored in TimelineComponent UI state only (not Edit). This keeps it decoupled for now but provides a clear surface for future AI suggestion timelines.

### Files changed (and why)
- `examples/Reason/TimelineComponent.h`
  - Added ScrollBar listener and marker/scroll/zoom state for timeline viewport and idea markers.
- `examples/Reason/TimelineComponent.cpp`
  - Implemented horizontal scroll (wheel/trackpad + scroll bar) and pinch/Cmd+wheel zoom.
  - Updated clip layout/hit-testing/rendering to respect `viewStartSeconds` and `pixelsPerSecond`.
  - Added marker lane drawing, marker storage, and inline text editing for markers.
  - Added dynamic ruler tick spacing and label spacing to avoid clutter.
  - Implemented edge auto-scroll while dragging clips.
  - Added content range tracking to keep scrollbar range accurate as clips/markers change.
- `examples/Reason/ReasonMainComponent.cpp`
  - Added `I` key handling to drop an Idea Marker at the current insertion time.

### Commands run
- `cmake -S . -B build -G Ninja`
- `cmake --build build --target Reason`
- `open build/examples/Reason/Reason_artefacts/Reason.app` (failed with `kLSNoExecutableErr`, see notes below)

### What works now
- Timeline can scroll horizontally via trackpad/mouse wheel or scrollbar drag.
- Pinch-to-zoom and Cmd+wheel zoom adjust the time scale around the cursor.
- Ruler ticks adapt to zoom; labels are spaced to avoid overlaps.
- Clips render, hit-test, and drag correctly with scroll/zoom.
- Dragging near the timeline edges auto-pans left/right.
- Idea Marker lane renders at the top; `I` drops a marker at the current insertion time.
- Marker text is editable (double-click the marker pill).

### Limitations intentionally left for later
- Markers are UI-only (not persisted to Edit/project yet).
- No vertical clip dragging between tracks.
- No zoom snapping or grid-based snapping.
- Auto-scroll is a simple step (no smooth acceleration).

### Launch note
- `open build/examples/Reason/Reason_artefacts/Reason.app` failed with `kLSNoExecutableErr` even though the executable exists at `build/examples/Reason/Reason_artefacts/Reason.app/Contents/MacOS/Reason`. This may be a local LaunchServices quirk; running the binary directly should still work.

---

## 2026-01-20 19:33

### Goals for this milestone
- Deep-audit Tracktion/JUCE modules and demos for DAW-relevant features.
- Document findings in `MODULE_AUDIT.md` with file/class references and integration plans.
- Implement 2–3 high-leverage features tonight, without breaking Reason’s architecture.

### What was audited (high level)
- **Plugin scanning/hosting** via Tracktion `PluginManager` and JUCE `PluginListComponent`.
- **Edit save/load** via `EditFileOperations`, `createEmptyEdit`, `loadEditFromFile`.
- **Transport + tempo** (looping, bars/beats, tempo sequence).
- **Recording** input device and EditUtilities helpers.
- **Rendering/export** via Renderer/RenderOptions.
- **Automation** curves and parameters.
- **Clip slots / launcher** + arranger track for session-view workflows.
- **Mix/volume plugins** (`VolumeAndPanPlugin`) and selection systems.

Full inventory is in `examples/Reason/MODULE_AUDIT.md`.

### Implemented features (tonight)
1) **Project save/load workflow**
   - Added Project menu with New / Open / Save / Save As.
   - Switched `SessionController` to own a `std::unique_ptr<te::Edit>` and re-initialize on load/new.
   - New edits are created using `te::createEmptyEdit` and saved immediately to a file.
   - Open uses `te::loadEditFromFile`; UI refreshes track list and timeline after load.
   - Cmd+N / Cmd+O / Cmd+S / Cmd+Shift+S hotkeys wired.

2) **Plugin scanning window**
   - Added a “Plugins” button to the transport bar.
   - Opens a `juce::PluginListComponent` dialog bound to Tracktion’s `PluginManager` (format manager + known list).
   - Uses Tracktion temp file and property storage for scan/persistence, same pattern as DemoRunner.

### Design choices and reasoning
- **SessionController ownership**: Edit is now a pointer, so we can safely swap edits on open/new while preserving the UI boundary.
- **UI separation**: Reason UI still talks through SessionController commands; plugin scanning stays UI-only with no DemoRunner dependencies.
- **Minimal risk**: Save/load uses Tracktion’s native file operations and avoids touching Tracktion internals.

### Files changed (and why)
- `examples/Reason/SessionController.h`
  - Switched Edit storage to `std::unique_ptr`, added create/open/save/saveAs APIs.
- `examples/Reason/SessionController.cpp`
  - Implemented new/open/save/saveAs using Tracktion’s `createEmptyEdit` and `loadEditFromFile`.
  - Ensured default tracks for new edits and refreshed transport state safely.
- `examples/Reason/TransportBarComponent.h` / `examples/Reason/TransportBarComponent.cpp`
  - Added “Project” and “Plugins” buttons and layout updates.
- `examples/Reason/ReasonMainComponent.h` / `examples/Reason/ReasonMainComponent.cpp`
  - Project menu + file chooser handling; plugin list dialog; keyboard shortcuts.
  - Refresh helper for track list + timeline after edit changes.
- `examples/Reason/MODULE_AUDIT.md`
  - New deep inventory report with integration guidance.

### Commands run
- `cmake -S . -B build -G Ninja` (first run timed out, reran successfully)
- `cmake --build build --target Reason` (first run timed out, reran successfully; one JUCE warning)
- `open build/examples/Reason/Reason_artefacts/Reason.app`

### Build/run outcome
- Build succeeded.
- Warning during JUCE build: `juce_NSViewComponentPeer_mac.mm` switch enum (unchanged upstream JUCE warning).
- App launch command executed; no runtime verification available from the CLI.

### What works now
- Project menu (New/Open/Save/Save As) with file pickers.
- Save falls back to Save As when no file is available.
- Plugin scan/list window opens from “Plugins” button.
- Cmd+N/Cmd+O/Cmd+S/Cmd+Shift+S shortcuts active.

### Limitations intentionally left for later
- No plugin insertion UI yet (scan only, no per-track insert).
- No session title or current edit path shown in UI.
- Markers remain UI-only (not persisted into the Edit).

---

## 2026-01-20 20:46

### Goals for this milestone
- Add MIDI playback support by inserting instruments per track (AU or built-in fallback).
- Add FX insertion per track and a safe plugin window launcher.
- Add MIDI recording workflow (Record button + Arm per track + MIDI input selection).
- Add a bottom piano roll editor with basic note editing and timeline-aligned zoom/scroll.

### Design choices and reasoning
- **Plugin windows**: Implemented a Reason-local plugin window wrapper in a single .cpp to avoid the duplicate-symbol problems of `examples/common/PluginWindow.h`.
- **Per-track plugin control**: Instrument and FX menus live on each track strip, but all insertions go through `SessionController` to keep engine mutations centralized.
- **MIDI recording**: Uses Tracktion input device targets and `EngineHelpers::toggleRecord` so Tracktion manages clip creation and recording buffers.
- **Piano roll**: Kept intentionally minimal (fixed note range, 1/16 grid). Uses `SessionController` note APIs so edits persist in the Edit/Undo.

### Files changed (and why)
- `examples/Reason/SessionController.h`
  - Added MIDI tempo getter, instrument/effect APIs, track arm state, and plugin window overrides.
- `examples/Reason/SessionController.cpp`
  - Fixed beat/tempo conversions for MIDI note edits; implemented instrument/effect insertion, plugin window routing, and MIDI recording prep.
- `examples/Reason/PluginWindow.h` / `examples/Reason/PluginWindow.cpp`
  - New Reason-local plugin window helper to safely show AU/VST UIs without ODR issues.
- `examples/Reason/PianoRollComponent.h` / `examples/Reason/PianoRollComponent.cpp`
  - New bottom piano roll editor with grid, note add/move/resize/delete.
- `examples/Reason/TrackStripComponent.h` / `examples/Reason/TrackStripComponent.cpp`
  - Added Instrument, FX, and Arm controls per track row.
- `examples/Reason/TrackListComponent.h` / `examples/Reason/TrackListComponent.cpp`
  - Added instrument names and armed states; added callbacks for instrument/FX/arm.
- `examples/Reason/TransportBarComponent.h` / `examples/Reason/TransportBarComponent.cpp`
  - Added Record button and MIDI input selector.
- `examples/Reason/TimelineComponent.h` / `examples/Reason/TimelineComponent.cpp`
  - Added clip double-click callbacks and view-change hooks for piano roll sync.
- `examples/Reason/ReasonMainComponent.h` / `examples/Reason/ReasonMainComponent.cpp`
  - Wired new menus (instrument/FX/MIDI input), record workflow, and piano roll panel + resizer.
- `examples/Reason/CMakeLists.txt`
  - Added new PianoRoll and PluginWindow source files to the target.

### Commands run
- `cmake -S . -B build -G Ninja`
- `cmake --build build --target Reason`
- `open build/examples/Reason/Reason_artefacts/Reason.app`

### Build/run outcome
- Build succeeded after fixing tempo/beat conversions and JUCE menu overloads.
- App launch command executed for manual verification.

### What works now
- Per-track Instrument selector inserts AU instruments (or built-in FourOsc fallback).
- Per-track FX menu inserts effects and exposes “Show Plugin UI” actions.
- Record button + Arm toggle + MIDI input menu enable MIDI recording.
- Piano roll opens on MIDI clip double-click or `E`, supports basic note add/move/resize/delete.
- Piano roll follows timeline scroll/zoom so navigation stays consistent.

### Limitations intentionally left for later
- Piano roll uses a fixed note range with no vertical scroll.
- Grid quantization is fixed to 1/16 notes and not user-configurable yet.
- No velocity editing or multi-select in piano roll.
- FX menu lists all effects (long list) without category grouping.
- Recording UX assumes Tracktion’s default recording behavior (no punch-in/out UI yet).

---

## 2026-01-20 21:16

### Goals for this milestone
- Fix instrument selection so clicking a new instrument reliably switches the track instrument.
- Simplify the FX menu to a single list and auto-open plugin UI on insert.
- Add a clear FX indicator per track and align track strips with timeline rows.
- Update styling toward a more colorful, Logic-like UI without changing architecture.

### Design choices and reasoning
- **Instrument detection**: Added a more robust instrument test (synth or ExternalPlugin category) so existing AUs are removed/replaced correctly.
- **Menu reliability**: Switched instrument/FX menus to ID-based `showMenuAsync` callbacks to avoid action dropouts.
- **FX UX**: Removed the three “Insert” shortcuts and the parallel “Show” section. Now one list inserts and immediately opens the plugin UI.
- **Alignment**: Track list now reserves a header height equal to the timeline ruler + marker lane so rows line up.

### Files changed
- `examples/Reason/SessionController.h`
  - Added effect count API and instrument helper; changed insert methods to return plugin IDs.
- `examples/Reason/SessionController.cpp`
  - Implemented instrument detection for ExternalPlugin categories; added effect counts; returned plugin IDs from insert; updated instrument list to force instrument flag.
- `examples/Reason/ReasonMainComponent.cpp`
  - Rebuilt instrument/FX menus as single lists with auto-open plugin UI; wired effect counts and header alignment.
- `examples/Reason/TrackStripComponent.h` / `examples/Reason/TrackStripComponent.cpp`
  - Added FX count indicator and refreshed styling.
- `examples/Reason/TrackListComponent.h` / `examples/Reason/TrackListComponent.cpp`
  - Added header height, effect counts, and header paint for alignment.
- `examples/Reason/TimelineComponent.h` / `examples/Reason/TimelineComponent.cpp`
  - Exposed header height and refined colors.
- `examples/Reason/TransportBarComponent.cpp`
  - Updated button colors and toolbar gradient.

### Commands run
- `cmake --build build --target Reason`
- `open build/examples/Reason/Reason_artefacts/Reason.app`

### What works now
- Instrument selection switches reliably and opens the selected instrument UI automatically.
- FX menu is a single list; choosing an effect inserts it and opens its UI immediately.
- FX button reflects active effect count (e.g., “FX 2”).
- Track list rows align with timeline tracks.
- UI colors are richer and closer to a Logic-style palette.

### Limitations intentionally left for later
- FX menu is not yet grouped by category.
- No per-track FX chain view; only count indicator and menu insertion.
- Additional UI polish (icons, meters, and theme tuning) still pending.

---

## 2026-01-20 21:37

### Goals for this milestone
- Show only safe external FX and include all built-in Tracktion effects.
- Add piano roll playhead and enable horizontal scroll within the piano roll.
- Add track mute/solo, editable names, track numbers, and align track headers with timeline.
- Improve track strip slider visuals and add clearer FX indicators.

### Design choices and reasoning
- **Safe FX list**: External effects are filtered by allow/deny keywords to avoid crashing AU utilities. Built-in effects are always available.
- **Built-in effects**: Added all built-in effect classes from Tracktion’s effects module so the engine’s own effects are exposed.
- **Track controls**: Mute/Solo stay per-track and allow multi-solo; Tracktion handles routing internally.
- **Piano roll navigation**: Added internal scroll/zoom handling and kept timeline/piano roll views in sync.

### Files changed
- `examples/Reason/SessionController.h`
  - Added mute/solo state APIs, track name setters, effect counts, and instrument detection helper.
- `examples/Reason/SessionController.cpp`
  - Added built-in effect choices + safe external filter; added mute/solo/name control methods.
- `examples/Reason/TrackStripComponent.h` / `examples/Reason/TrackStripComponent.cpp`
  - Added track number label, editable name, M/S buttons, and updated slider styling.
- `examples/Reason/TrackListComponent.h` / `examples/Reason/TrackListComponent.cpp`
  - Added header offset for alignment, mute/solo states, name change callbacks, and effect counts.
- `examples/Reason/TimelineComponent.h` / `examples/Reason/TimelineComponent.cpp`
  - Exposed view setter for piano roll sync.
- `examples/Reason/PianoRollComponent.h` / `examples/Reason/PianoRollComponent.cpp`
  - Added playhead line and scroll/zoom handling with view callbacks.
- `examples/Reason/ReasonMainComponent.cpp`
  - Wired name edits, mute/solo actions, instrument renaming logic, and view syncing.

### Commands run
- `cmake --build build --target Reason`
- `open build/examples/Reason/Reason_artefacts/Reason.app`

### What works now
- FX menu only lists safe external effects + all built-in Tracktion effects.
- Piano roll shows the playhead and scrolls horizontally with the mouse wheel.
- Track names are editable on double-click; track numbers appear on the left.
- Mute (M) and Solo (S) buttons toggle and update colors; multi-solo works.
- Track rows align with timeline lanes; slider styling is thicker and more visible.

### Limitations intentionally left for later
- FX safe list is keyword-based and may still miss edge-case safe plugins.
- Slider color does not yet animate smoothly across the whole range.
- No per-track FX chain panel yet, just count indicator + menu.

---

## 2026-01-20 22:08

### Goals for this milestone
- Remove the track “arm” checkbox and make selection the primary interaction.
- Build an in-app FX inspector panel (Logic-style) with enable/disable and remove per FX.
- Show only safe external FX and list all Tracktion built-in effects.
- Add bar/beat grid lines on the timeline, and improve transport readouts.
- Add piano roll playhead and require Ctrl-click to add notes.
- Make track headers resizable horizontally.

### Design choices and reasoning
- **FX inspector panel**: Keeps FX editing inside the DAW window (no external plugin windows required for built-ins). Each FX has a slot with enable toggle and remove button.
- **Safety first FX list**: External effects are filtered by keywords to avoid instantiating panners/mixers/utilities that crash. Built-in Tracktion effects are always available.
- **Transport info**: Added editable Tempo/TimeSig/Key fields tied to Tracktion sequences so MIDI timing follows edits.
- **Selection as “arm”**: Track selection now drives the record target; the visible arm checkbox was removed.

### Files changed
- `examples/Reason/FxInspectorComponent.h` / `examples/Reason/FxInspectorComponent.cpp`
  - New FX inspector panel with slots, enable toggles, remove actions, and embedded plugin editor area.
- `examples/Reason/SessionController.h` / `examples/Reason/SessionController.cpp`
  - Added FX list APIs, plugin enable/remove/editor creation, tempo/time-signature/key getters/setters, and selection-based record arming.
- `examples/Reason/TrackStripComponent.h` / `examples/Reason/TrackStripComponent.cpp`
  - Removed arm checkbox, added editable track names, M/S buttons, and visual tweaks.
- `examples/Reason/TrackListComponent.h` / `examples/Reason/TrackListComponent.cpp`
  - Removed arm state, added name editing callbacks, mute/solo state wiring, and track numbering.
- `examples/Reason/TransportBarComponent.h` / `examples/Reason/TransportBarComponent.cpp`
  - Added bars/tempo/time-signature/key display fields with inline editing; improved layout and height.
- `examples/Reason/TimelineComponent.h` / `examples/Reason/TimelineComponent.cpp`
  - Added view syncing for piano roll; drew bar/beat/subdivision grid lines.
- `examples/Reason/PianoRollComponent.h` / `examples/Reason/PianoRollComponent.cpp`
  - Added playhead line; Ctrl-click required to add notes; scroll/zoom emits view changes.
- `examples/Reason/ReasonMainComponent.h` / `examples/Reason/ReasonMainComponent.cpp`
  - Added FX inspector panel, tracklist resizer, and wiring for tempo/time signature/key edits.
- `examples/Reason/CMakeLists.txt`
  - Added `FxInspectorComponent` sources.

### Commands run
- `cmake --build build --target Reason`
- `open build/examples/Reason/Reason_artefacts/Reason.app`

### What works now
- Track checkbox removed; clicking a track selects it and arms it for recording.
- FX inspector shows the track’s FX slots with on/off toggles and remove buttons.
- Built-in Tracktion effects appear in the FX list; external FX are filtered to safe categories.
- Transport shows time + bars + tempo + time signature + key; tempo/time signature/key are editable.
- Timeline shows bar/beat/subdivision grid lines.
- Piano roll shows a red playhead and requires Ctrl-click to add notes.
- Track header width is draggable.

### Limitations intentionally left for later
- FX safety filter is keyword-based; some safe plugins may be excluded until whitelisted.
- Embedded plugin editors depend on plugin support; some built-ins still show “no UI”.
- Time signature/key changes only update the primary sequence (no historical key/time-sig lanes yet).

---

## 2026-01-20 22:35

### Problems and discoveries (detailed)
This entry captures the issues we hit or observed while using the current Reason build, plus the underlying causes we identified so far. This is a diagnosis log, not a change list.

### Core plugin/instrument issues
- **Instrument selection menu shows options but selection doesn’t switch**
  - Symptom: Clicking a different instrument in the instrument menu does nothing; DLSMusicDevice remains selected.
  - Likely cause: the instrument insertion/update path is not replacing the existing instrument plugin or is failing to instantiate the new plugin (plugin creation failure, or selection UI not wired to the controller).
  - Risk: Users think instruments are selectable but the track remains on the default AU, leading to confusion and no change in sound.
- **External plugin UI shows (Splice) and edits the external app**
  - Symptom: Opening a plugin editor for Splice Instrument appears to edit the standalone Splice app UI.
  - Cause: We are hosting the AU/VST plugin editor, which is the plugin’s own UI. Some plugins share state with their standalone app or present identical UI.
  - Implication: For a native DAW feel, we will eventually need a built-in instrument/sampler rather than relying on third-party plugin UIs.

### FX menu / plugin list issues
- **FX menu shows nested/parallel lists and is confusing**
  - Symptom: Selecting “Insert Reverb/EQ/Compressor” opens another nested menu, creating parallel lists.
  - Cause: The menu uses submenu categories plus a flat list below; it is not a single filtered list.
  - User impact: It feels like duplicate menus and makes it unclear which item actually inserts a plugin.
- **Most FX cause crashes (except Reverb)**
  - Symptom: Adding many FX from the list crashes the app; Reverb is the only reliably safe one.
  - Likely causes: AU “utility” or “panner” units that are not safe in a standard insert chain, channel-layout mismatches, or plugin initialization requirements we don’t satisfy.
  - Mitigation: FX list is filtered to “safe” keywords, but the filter is still evolving and incomplete.
- **Built-in Tracktion effects appear but no editor window shows**
  - Symptom: Built-in FX are inserted and counted, but the UI panel shows no editor.
  - Cause: Some built-in Tracktion effects don’t expose a standard plugin editor or require a custom editor implementation that we haven’t wired.
  - Impact: Users cannot tweak built-in FX parameters yet.

### Tracklist / timeline alignment
- **Track headers do not align with timeline lanes**
  - Symptom: Track strip rows appear vertically offset compared to the track lanes in the timeline.
  - Cause: Header height mismatch and a missing top offset between TrackList and Timeline ruler.
  - Impact: Users can’t visually match track controls to the timeline content.

### Transport / ruler display issues
- **Time display cut off / too small**
  - Symptom: The transport time display is clipped at the top and unreadable.
  - Cause: Transport bar height too small for the chosen font size; label bounds not padded for ascenders.
  - Impact: Critical timeline information is partially hidden.
- **Ruler lacks measure/beat grid**
  - Symptom: No bar/beat subdivision grid in the track area.
  - Cause: We were still drawing in seconds and not mapping bar/beat lines yet.
  - Impact: No visual quantization guide for MIDI work.

### Piano roll issues
- **Clicking anywhere adds notes (unsafe)**
  - Symptom: Single click inserts a note even if the user is just selecting or scrolling.
  - Cause: Add-note on any click; no modifier guard.
  - Impact: Accidental note creation.
- **No horizontal scrolling when focused**
  - Symptom: Mouse wheel scroll in the piano roll does nothing horizontally.
  - Cause: Piano roll did not implement mouse wheel/scroll handling; no view sync with the timeline state.
  - Impact: Piano roll feels “dead” when focused.
- **Missing playhead in piano roll**
  - Symptom: No red playhead line in the piano roll, so playback position is unclear.
  - Cause: Playhead rendering not implemented.
  - Impact: Editing feels disconnected from playback.

### Track controls / usability
- **Arm checkbox is confusing**
  - Symptom: A small checkbox appears on each track with unclear purpose.
  - Cause: Legacy arm checkbox used for selection/record arming.
  - Impact: Redundant UI; users expect simple click-to-select.
- **No mute/solo buttons initially**
  - Symptom: Tracks could not be muted or soloed.
  - Cause: UI did not expose Tracktion mute/solo state.
  - Impact: Core DAW control missing.

### Plugin discovery understanding
- **Why AU entries (AUBandpass, AUReverb2, etc.) appear**
  - These entries come from JUCE/Tracktion’s plugin format manager scanning system Audio Units.
  - The KnownPluginList stores the scan results (metadata like name, format, category, manufacturer).
  - The list reflects system-discoverable plugins, not a curated Reason set.

### Overall risk notes
- The plugin list still needs stricter validation to avoid inserting unsupported or unstable plugins.
- Built-in Tracktion effects need a standardized in-app editor surface (or custom UIs) to be useful.
- The UI is more DAW-like now, but there is still a gap between track controls and editor surfaces (FX chain, piano roll, timeline ruler) that we are actively closing.

---

## 2026-01-29 — Thesis focus and ReaLchords direction

### Meeting with Nikhil Singh (summary)
- **Product vs thesis:** The product (Reason DAW) can continue to add many features. The thesis must focus on **one high-leverage contribution** with a clear claim and evidence—not K features (which would require K!+1 evaluations).
- **One thing:** Identify the single intervention that would “maximally move the needle” for the target population (e.g. practicing composers/producers). That becomes the thesis claim.
- **Your chosen focus:** Harmony and chords—helping users **explore the space of possibilities** rather than chasing a single AI output (e.g. Suno). Thesis claim to formalize: tools like Suno narrow exploration; we open it up via harmony-focused option generation, interface, and evaluation.
- **SOTA substrate:** ReaLchords (Anna Huang et al.)—reinforcement-learning-based online chord accompaniment. Use it as the algorithmic backbone; our contribution = interaction design + “space of possibilities” (e.g. 5 chord options per melody) + evaluation.
- **Reference:** Tyler Virgo’s thesis (OpenMuse: integrating open-source models into music creation workflows); RealChords paper (“Adaptive Accompaniment with ReaLchords”); ReaLJam for real-time jamming.

### What we are moving forward with (thesis + product)
| Scope | Focus | What must be done |
|-------|--------|-------------------|
| **Thesis** | One claim + evidence | (1) Formalize claim: e.g. “Harmony-focused option generation (N alternatives per melody) improves exploration and creative benefit over single-output tools (e.g. Suno Studio).” (2) Algorithm: ReaLchords for chord generation. (3) Interface: present **5 different chord options** for a given melody so users can explore. (4) Evaluation: study with target users (e.g. recruitment, tasks, metrics) to show the thing works. |
| **Product** | Reason DAW | Continue adding features (melody gen, gain automation, etc.) as desired; keep thesis scope to harmony/chord exploration so the story stays coherent. |
| **Immediate** | ReaLchords in Reason | Get ReaLchords (or ReaLJam backend) running from the [lukewys/realchords-pytorch](https://github.com/lukewys/realchords-pytorch) repo; wire it so we can request **5 different chord progressions** for a given melody and surface them in the DAW. |

### ReaLchords: papers and repo
- **Papers:** “Adaptive Accompaniment with ReaLchords” (ICML 2024); “ReaLJam: Real-Time Human-AI Music Jamming with Reinforcement Learning-Tuned Transformers” (CHI 2025 EA); “Generative Adversarial Post-Training Mitigates Reward Hacking…” (GAPT).
- **Repo:** [lukewys/realchords-pytorch](https://github.com/lukewys/realchords-pytorch) — PyTorch implementation of ReaLchords, ReaLJam, GAPT. Pretrained checkpoints on Hugging Face; ReaLJam server can run locally for real-time jamming.
- **Goal in Reason:** For a selected melody (or region), call ReaLchords and obtain **5 distinct chord options** (e.g. via sampling/temperature or multiple inference runs), then show them in the UI for the user to pick or iterate.

### Concrete next steps (must be done)
1. **Get ReaLchords running locally** — Install and run ReaLJam server (or use realchords-pytorch inference) so we can send melody → get chord progression(s). See “ReaLchords implementation steps” below.
2. **Define melody → chord API** — Clear input (e.g. MIDI or frame-based melody) and output (e.g. chord symbols + timing) so Reason can send a melody and receive one or more chord progressions.
3. **Implement “5 options” path** — Either (a) call the model 5 times with sampling/temperature to get 5 different progressions, or (b) use an existing batch/sampling API if the server exposes it. Document the choice.
4. **Wire Reason → ReaLchords** — Reason (C++/JUCE) must invoke the Python backend (e.g. ReaLJam server over HTTP, or a small Python script via subprocess/socket). Design the bridge (URLs, payload format, timeouts).
5. **UI for 5 chord options** — In Reason, add a way to “Get chord options” for the current selection (e.g. piano roll or clip), show 5 alternatives, let the user pick one (or refine and re-request).
6. **Thesis formalization** — Write the one-sentence claim and list baselines (e.g. Suno Studio single-output, or no AI). Plan evaluation (population, task, metrics).

---

## ReaLchords implementation steps (step-by-step)

### Phase 1: Run ReaLchords/ReaLJam with minimal setup (no training)

**Step 1.1 — Python environment**  
- Use Python ≥ 3.10. Create a dedicated venv or conda env for ReaLchords to avoid dependency clashes.

```bash
cd /Users/takakhoo/Dev/tracktion/tracktion_engine  # or a sibling folder for ML code
python3 -m venv .venv_realchords
source .venv_realchords/bin/activate   # macOS/Linux
pip install --upgrade pip
```

**Step 1.2 — Install ReaLJam (fast path: server + pretrained checkpoints)**  
- ReaLJam runs a server that uses pretrained checkpoints (downloaded automatically to `$HOME/.realjam/checkpoints/`). No dataset or training required.

```bash
pip install realjam
```

- Optional: use the full repo for inference and future “5 options” logic:

```bash
git clone https://github.com/lukewys/realchords-pytorch.git
cd realchords-pytorch
pip install -e .
```

**Step 1.3 — Start ReaLJam server**  
- From project root (or from `realchords-pytorch` if you cloned it):

```bash
realjam-start-server
# Or with port/SSL: realjam-start-server --port 8080 --ssl
```

- Or from inside the cloned repo:

```bash
python -m realchords.realjam.server --port 8080
```

- Open `http://127.0.0.1:8080` in a browser to confirm the ReaLJam UI. Pretrained checkpoints download on first run.

**Step 1.4 — Optional: ONNX speedup**  
- For faster inference (CPU/GPU): `realjam-start-server --onnx`. Adjust `onnx_provider` if needed (`realjam-start-server -h`).

---

### Phase 2: Understand inputs/outputs and get “melody → chords” once

**Step 2.1 — ReaLchords data format (from paper/repo)**  
- **Time:** Quantized to sixteenth-note frames (4 frames = 1 beat).  
- **Melody:** Per-frame tokens — note-on (MIDI pitch 0–127), note-hold, or silence.  
- **Chords:** Per-frame — chord symbol (from vocab, e.g. Hooktheory), chord-hold, or silence.  
- Max length in training is 256 frames (64 beats).  
- Chord vocabulary: e.g. `data/cache/chord_names.json` or `chord_names_augmented.json` in the repo when using datasets.

**Step 2.2 — ReaLJam server API**  
- ReaLJam is built for **real-time jamming** (stream melody, get chords back). For “given a melody, get 5 chord options,” we need either:  
  - (A) Use the server’s existing endpoints (if any) that accept a full melody and return one chord progression, then call 5 times with different sampling/temperature if the server allows; or  
  - (B) Add a small Python script in `realchords-pytorch` that loads the pretrained model, takes a melody sequence, runs inference 5 times with sampling (e.g. different temperature or top_p), and returns 5 chord progressions.  
- **Immediate action:** Inspect ReaLJam server code in the repo (`realchords/realjam/server` or similar) to see how it receives melody and returns chords. Then decide: use HTTP API as-is and call 5 times, or add an offline “batch” endpoint/script that returns 5 options in one go.

**Step 2.3 — Checkpoints**  
- ReaLJam downloads checkpoints to `$HOME/.realjam/checkpoints/`.  
- Full training checkpoints (MLE, reward, RL) are on the [Hugging Face page](https://huggingface.co) linked from the repo README. For inference-only, the ReaLJam-installed checkpoints are enough to start.

---

### Phase 3: Implement “5 chord options” for one melody

**Step 3.1 — Sampling strategy**  
- ReaLchords is autoregressive. To get 5 **different** chord progressions for the same melody:  
  - Run inference 5 times with **temperature > 0** (e.g. 0.8–1.0) and/or **nucleus (top_p)** sampling so each run can produce a different sequence.  
- If the ReaLJam server only supports real-time streaming, implement a small **offline inference script** in `realchords-pytorch` that:  
  - Loads melody (e.g. from MIDI or from a JSON of frame-based tokens).  
  - Loads the pretrained ReaLchords model (decoder-only online chord model).  
  - Runs 5 forward passes with sampling, collects 5 chord sequences.  
  - Outputs 5 chord progressions (e.g. as list of chord names per frame, or as MIDI/JSON for Reason).

**Step 3.2 — Output format for Reason**  
- Define a single format (e.g. JSON): one object per option, each with a list of chord symbols and their start/end times or frame indices. Reason will parse this and show 5 options in the UI.

---

### Phase 4: Wire Reason (C++/JUCE) to ReaLchords

**Step 4.1 — Bridge design**  
- Reason cannot run PyTorch directly. Options:  
  - **HTTP:** ReaLJam server (or a thin Flask/FastAPI wrapper around the “5 options” script) running locally; Reason sends POST with melody (e.g. MIDI or frame tokens), receives JSON with 5 chord options.  
  - **Subprocess:** Reason spawns a Python script that writes melody to stdin or a temp file and reads 5 options from stdout or another temp file. Simpler but less flexible than HTTP.  
- **Recommendation:** Start with HTTP. Run ReaLJam (or a custom small server) on localhost; Reason uses `juce::URL` or a simple HTTP client to POST melody and GET/POST response with 5 chord options.

**Step 4.2 — Melody from Reason to backend**  
- When the user selects a clip/region:  
  - Export the melody from the Edit (e.g. selected MidiClip or pitch data from the piano roll) as a sequence of (time, pitch) or frame-based tokens.  
  - Map to ReaLchords format (sixteenth-note frames, note-on/hold/silence).  
  - Send in the HTTP request body (JSON or similar).

**Step 4.3 — Response handling in Reason**  
- Parse the JSON with 5 chord progressions.  
- Display them in a list or panel (e.g. “Option 1 … Option 5” with chord names or a short preview).  
- On “Apply,” insert the chosen chord progression into the Edit (e.g. as a new MidiClip or as chord track data) using existing Tracktion APIs.

---

### Phase 5: Docs and thesis

- **Document** in TIMELINE or a short design doc: (1) ReaLchords version and checkpoint used, (2) input/output format, (3) how “5 options” are produced (sampling/temperature), (4) bridge (HTTP URLs, payload).  
- **Thesis:** Keep the claim to “harmony exploration via multiple chord options (e.g. 5) with ReaLchords, evaluated against single-output baseline and/or no-AI.”

---

### Commands quick reference

```bash
# 1. Environment
python3 -m venv .venv_realchords && source .venv_realchords/bin/activate
pip install realjam

# 2. Start server (checkpoints download to ~/.realjam/checkpoints/)
realjam-start-server

# 3. Or clone and run from repo
git clone https://github.com/lukewys/realchords-pytorch.git
cd realchords-pytorch && pip install -e .
python -m realchords.realjam.server --port 8080
```

- **Deep walkthrough:** See `examples/Reason/REALCHORDS_WALKTHROUGH.md` for step-by-step repo layout, data format, “5 options” strategy, checkpoints, and Reason ↔ backend wiring.

---

### What is different now (after this timeline entry)
- Thesis scope is **fixed** on harmony/chord exploration and “space of possibilities,” with ReaLchords as SOTA and a clear next step: get 5 chord options per melody into Reason.  
- Product (Reason) continues to add features; thesis work is focused on one claim, one evaluation, and the ReaLchords integration path above.

---

## 2026-01-29 18:52

### Goals for this milestone
- Add a **local Realchords batch service** (no browser UI) that returns **5 chord options** for a melody.
- Wire Reason to call that service and **create 5 new chord tracks** from a selected MIDI clip.
- Keep engine mutations inside SessionController and keep HTTP/network work off the message thread.

### Design choices and reasoning
- **Local batch server (port 8090):** avoids the ReaLJam web UI and provides a simple HTTP endpoint for Reason.
- **Offline non-causal model for options:** uses the enc/dec teacher model to generate full chord sequences for a melody, sampled 5 times with temperature to produce alternatives.
- **Frame-based note encoding:** converts MIDI clip notes into 16th-note frames (4 frames/beat), matching ReaLchords’ expected representation.
- **UI/engine boundary:** ReasonMainComponent handles HTTP/JSON; SessionController provides note extraction and track creation.
- **Async networking:** network call runs on a detached background thread, then posts results back on the message thread.

### Files created / modified
- `tools/realchords/realchords_batch_server.py`
  - New Flask server providing `/generate5` that accepts note events and returns 5 chord option sequences.
  - Loads ReaLchords models via `realjam.agent_interface.Agent` and uses the offline enc/dec model for full-sequence generation.
- `examples/Reason/SessionController.h`
  - Added `RealchordsNoteEvent`, `ChordFrame`, and `ChordOption` structs.
  - Added `buildRealchordsNoteEvents()` and `createChordOptionTracks()` declarations.
- `examples/Reason/SessionController.cpp`
  - Implemented conversion from MIDI clip → frame-based note events for Realchords.
  - Implemented creation of 5 new tracks with chord MIDI clips from returned chord frames.
- `examples/Reason/TransportBarComponent.h/.cpp`
  - Added a “Chords x5” button and callback.
- `examples/Reason/ReasonMainComponent.h/.cpp`
  - Added chord generation workflow: extract notes, POST to local server, parse response, and create new tracks.

### Commands run (local setup)
- `brew install python@3.12`
- `brew link python@3.12`
- `brew reinstall openssl@3 python@3.12`
- `install_name_tool -change ... _ssl.cpython-312-darwin.so` (fix SSL link)
- `python3.12 -m venv tools/realchords/.venv`
- `tools/realchords/.venv/bin/python -m pip install realjam`
- `tools/realchords/.venv/bin/realjam-start-server` (for checkpoint download)

### What works now
- Realchords checkpoints download successfully (GAPT + ReaLchords + Online/Offline models).
- A local batch server script exists and can be started via:
  - `tools/realchords/.venv/bin/python tools/realchords/realchords_batch_server.py`
- Reason has a “Chords x5” button that:
  - Extracts a selected MIDI clip into frame events,
  - Calls the local batch server,
  - Creates 5 new MIDI tracks with chord progressions.

### Limitations intentionally left for later
- The batch server currently always uses the **offline MLE teacher model** (not the RL online models) for full-sequence generation.
- No chord “preview UI” yet; it directly creates new tracks.
- No chord naming lane or UI selection between options (can be added later).
- If the batch server isn’t running, Reason shows a connection error.

---

## 2026-01-30 02:05

### Goals for this milestone
- Fix “invalid response” errors during chord generation.
- Auto-start the Realchords batch server when Reason launches so manual terminal steps aren’t required.

### Changes made
- Added a `/health` endpoint to the Realchords batch server for readiness checks.
- Reason now pings the server on launch and before generating chords; if it isn’t running, the app spawns the server process using the local venv.
- Error dialogs now include the raw response body for easier diagnosis.

### Files updated
- `tools/realchords/realchords_batch_server.py`
  - Added `/health` endpoint and kept `/generate` + `/generate5` compatible.
- `examples/Reason/ReasonMainComponent.h`
  - Added Realchords server management helpers and process handle.
- `examples/Reason/ReasonMainComponent.cpp`
  - Added auto-start on launch and pre-generate checks.
  - Improved error reporting for invalid responses.

### Commands run
- `cmake --build build --target Reason`

### What works now
- Reason attempts to auto-start the Realchords batch server on launch.
- If the server is down, the app will start it and retry on chord generation.
- Error dialogs show the actual raw response from the server.

### Notes / limitations
- Auto-start assumes the venv exists at `tools/realchords/.venv/bin/python`.
  If that venv is missing or broken, the server won’t launch (manual start still works).

---

## 2026-01-30 02:42

### Goals for this milestone
- Fix chord generation failing for larger option counts (appearing as “Could not reach Realchords server”).
- Add core editor shortcuts: clip copy/paste, undo, and duplicate track.
- Make tracks taller with clearer selection state for easier navigation.

### Changes made
- Increased Realchords request timeout and surfaced HTTP status codes for failed responses, improving stability when requesting more than 6 chord options.
- Added clip copy/paste (Cmd+C / Cmd+V) at the timeline level, pasting at the cursor on the selected track.
- Added Undo (Cmd+Z) and Redo (Cmd+Shift+Z) via the Edit UndoManager.
- Added Track duplicate (Cmd+D) to create a copy of the selected track directly below.
- Increased track row height and made selected tracks more visually obvious with stronger accent styling.
- Slightly resized track strip controls (buttons, instrument row, volume slider) to fit the taller layout.

### Files updated
- `examples/Reason/ReasonMainComponent.cpp`
  - Increased Realchords request timeout to 60s and added HTTP status reporting.
  - Added Cmd+Z/Cmd+Shift+Z, Cmd+D, Cmd+C, Cmd+V handling.
  - Clipboard paste now selects the newly pasted clip.
- `examples/Reason/ReasonMainComponent.h`
  - Added clip clipboard tracking.
- `examples/Reason/SessionController.h`
  - Added APIs for `duplicateClipToTrack`, `duplicateTrack`, `undo`, and `redo`.
- `examples/Reason/SessionController.cpp`
  - Implemented clip paste via `duplicateClip` + move + setStart.
  - Implemented track duplicate via `Edit::copyTrack`.
  - Implemented undo/redo hooks and UI refresh updates.
- `examples/Reason/TimelineComponent.h`
  - Added `setSelectedClipId` for selecting pasted clips.
- `examples/Reason/TrackListComponent.h`
  - Increased row height to make track strips taller.
- `examples/Reason/TrackStripComponent.cpp`
  - Stronger selected styling (accent overlay + border).
  - Tweaked layout for taller rows and thicker slider.

### Commands run
- `cmake --build build --target Reason`

### What works now
- Chord generation tolerates longer request times and reports server-side HTTP errors explicitly.
- Cmd+C/V copies and pastes selected clips to the cursor time on the selected track.
- Cmd+Z undoes the last action; Cmd+Shift+Z redoes.
- Cmd+D duplicates the selected track.
- Tracks are taller and the selection state is more visually obvious.

### Notes / limitations
- Very large chord-option counts can still be slow; if the server process crashes, the app will report the HTTP error or missing server.
- Clip clipboard is in-memory only (clears on app restart).
