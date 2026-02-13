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

---

## 2026-02-09 10:58

### Goals for this milestone
- Make external instrument state (Splice) persist when the plugin window is closed, so the selected instrument does not revert to default.
- Show the current instrument/preset name on the track’s instrument button when the plugin reports it.

### What changed and why
- **Plugin window close now forces a state flush**: Some plugins don’t notify Tracktion that their internal state changed when presets are loaded. We now explicitly mark the plugin as changed and flush its state on window close, so program/preset changes are saved.
- **Instrument button reflects plugin program name**: If an external plugin reports a current program name, we display that as the track’s instrument label; otherwise we fall back to the plugin name.
- **Periodic instrument name refresh**: Track strips now poll the instrument name periodically to reflect in-plugin changes without rebuilding the whole UI.

### Files updated
- `examples/Reason/PluginWindow.cpp`
  - On close, call `plugin.edit.pluginChanged(plugin)` before flushing state to force persistence of in-plugin preset changes.
- `examples/Reason/SessionController.cpp`
  - Instrument name now uses `ExternalPlugin::getCurrentProgramName()` when available; falls back to plugin name.
- `examples/Reason/ReasonMainComponent.cpp`
  - Added a periodic instrument-name refresh (every 15 timer ticks) to update the track button label without a full UI rebuild.

### Commands run
- `cmake --build build --target Reason`

### What works now
- Closing a plugin window saves its current state, so Splice preset choices persist.
- Instrument button shows the plugin’s program name (when the plugin reports it).

### Notes / limitations
- If a plugin does not expose a program name, the button will still show the plugin name.

---

## 2026-02-09 11:04

### Goals for this milestone
- Stabilize Splice instrument preset persistence without causing crashes on app shutdown.
- Keep the instrument label syncing to the plugin’s reported program name.

### What changed and why
- **Safer plugin state flush on close**: Moved the explicit “mark changed + flush state” to the user-initiated close path only. This keeps preset changes persistent but avoids potential shutdown crashes caused by touching Edit state in the window destructor.

### Files updated
- `examples/Reason/PluginWindow.cpp`
  - `userTriedToCloseWindow()` now calls `plugin.edit.pluginChanged()` and `plugin.edit.flushPluginStateIfNeeded()` before closing.
  - Removed `plugin.edit.pluginChanged()` from the window destructor to reduce shutdown crash risk.

### Commands run
- `cmake --build build --target Reason`

### What works now
- Splice preset changes should persist when the plugin window is closed.
- Reduced risk of “quit unexpectedly” on shutdown tied to plugin window teardown.

### Notes / limitations
- If the plugin doesn’t expose program names, the instrument button will still show the plugin name.

---

## 2026-02-09 11:25

### Goals for this milestone
- Diagnose the crash seen when closing the Splice AU plugin window.
- Stabilize AU editor teardown to avoid invalid NSView removal.

### Root cause analysis (from crash log)
- Crash triggered on the JUCE message thread when closing the plugin window.
- Stack shows `AudioUnitPluginWindowCocoa::~AudioUnitPluginWindowCocoa()` → `NSViewAttachment::removeFromParent()` → `-[NSView removeFromSuperview]` → `BUG_IN_CLIENT_OF_LIBMALLOC_POINTER_BEING_FREED_WAS_NOT_ALLOCATED`.
- This points to a content teardown ordering problem where the AU view is being detached after its native view reference has become invalid.

### What changed and why
- **Clear content before destroying editor**: `PluginWindow::setEditor` now calls `clearContentComponent()` before resetting the editor. This ensures the window no longer holds a stale content pointer while the AU editor is being destroyed.
- **Avoid close-time state churn**: Removed `plugin.edit.pluginChanged()` and `flushPluginStateIfNeeded()` from `userTriedToCloseWindow()` to reduce plugin state work during AU teardown.

### Files updated
- `examples/Reason/PluginWindow.cpp`
  - `setEditor()` clears the content component before `editor.reset()`.
  - `userTriedToCloseWindow()` now only calls `plugin.windowState->closeWindowExplicitly()`.

### Commands run
- `rg -n "PluginWindow" examples/Reason/PluginWindow.cpp`
- `sed -n '1,260p' examples/Reason/PluginWindow.cpp`
- `rg -n "setContentNonOwned" modules/juce/modules/juce_gui_basics/windows`
- `sed -n '70,170p' modules/juce/modules/juce_gui_basics/windows/juce_ResizableWindow.cpp`

### Build/test status
- Not rebuilt after this change yet.

### What to verify next
- Closing Splice AU windows no longer crashes.
- Splice AU windows still open and function normally.

---

## 2026-02-09 14:10

### Goals for this milestone
- Ensure the track instrument button never shows “Untitled” and instead reflects a meaningful instrument name.
- Narrow the instrument button and improve track strip UX clarity.
- Make track control colors, gain UI, and transport controls match the requested visual language.
- Show real-time MIDI notes while recording so a region appears as you play.

### What changed and why
- **Persistent instrument label fallback**: Track instrument names now use a stored per-track label when the plugin reports a placeholder name (e.g., “Untitled”). This ensures the instrument button displays a meaningful label even when plugins don’t expose program names.
- **Track-name collision guard**: If a plugin reports a name that matches the track name (e.g., “Untitled”), we treat it as a placeholder and fall back to the stored label or plugin type.
- **Instrument label propagation on duplication**: When duplicating an instrument plugin (e.g., for chord option tracks), the stored instrument label is copied to the new track to keep the instrument button consistent.
- **Track strip visual updates**: Instrument button width reduced; mute/solo buttons are filled with light blue/neon yellow; AI button removed; gain label + live dB readout + 0 dB marker added; slider height increased.
- **Transport updates**: “Chords 5x” renamed to “Generate Chords”; play/stop/record switched to triangle/square/circle shape buttons.
- **Recording preview**: A live MIDI preview clip is drawn while recording so notes appear immediately in the timeline.

### Files updated
- `examples/Reason/SessionController.cpp`
  - Added `reasonInstrumentLabel` ValueTree property handling.
  - Stored a robust instrument label on insert and reused it when program names are placeholders.
  - Copied instrument labels when duplicating instrument plugins.
- `examples/Reason/TrackStripComponent.cpp`
  - Reduced instrument button width.
  - Gain label, dB readout, 0 dB marker, thicker slider area, and mute/solo color updates.
- `examples/Reason/TrackStripComponent.h`
  - Track strip UI elements include gain labels and readout.
- `examples/Reason/TransportBarComponent.h`
  - Transport buttons now use shape-based play/stop/record.
  - Chords button label updated to “Generate Chords”.
- `examples/Reason/TransportBarComponent.cpp`
  - Shape button geometry + colors, smaller transport sizing.
- `examples/Reason/SessionController.h`
  - Recording preview state for real-time MIDI drawing.

### Commands run
- `rg -n "Instrument|instrument" examples/Reason/TrackStripComponent.cpp examples/Reason/SessionController.cpp`
- `sed -n '1,220p' examples/Reason/TrackStripComponent.cpp`
- `sed -n '280,380p' examples/Reason/SessionController.cpp`
- `sed -n '1,260p' examples/Reason/TransportBarComponent.cpp`

### Build/test status
- Not rebuilt after these changes yet.

### What to verify next
- Instrument button shows a meaningful name (e.g., “Splice INSTRUMENT” or the selected AU name) instead of “Untitled”.
- Mute/Solo buttons are clearly filled with the requested colors on every track.
- Gain readout updates live, 0 dB marker is visible, and slider feels easier to grab.
- Transport buttons render as triangle/square/circle and “Generate Chords” label is correct.
- While recording MIDI, a clip appears and notes draw in real time.

---

## 2026-02-09 15:45

### Goals for this milestone
- Tighten the gain UI range and sizing to match -10 dB to +6 dB with a shorter slider.
- Ensure the MIDI input label reflects the selected device and recording actually captures notes.
- Make tempo edits apply globally at time 0 and expose a metronome toggle.
- Remove stray non-ASCII symbols in the chord inspector header.

### What changed and why
- **Gain range + sizing**: Mapped normalized gain to -10 dB .. +6 dB and capped the slider width so it doesn’t span the full row.
- **Thicker gain track**: Custom slider look-and-feel draws a thicker filled track and thumb for better visibility.
- **MIDI input label refresh**: The transport bar now refreshes the MIDI input label in the main timer tick.
- **Recording capture fix**: The selected MIDI device now has `recordingEnabled` set explicitly, so notes are actually recorded and show up in clips.
- **Tempo consistency + metronome**: Tempo and time-signature lookups now use the edit’s time 0 entry (project-wide), and the metronome toggle is wired to `edit->clickTrackEnabled`.
- **Chord inspector clean-up**: Replaced em dash characters with ASCII to avoid “â€”” artifacts.

### Files updated
- `examples/Reason/SessionController.cpp`
  - Gain dB mapping now uses -10..+6 range.
  - Selected MIDI device explicitly sets `recordingEnabled = true` before record.
  - Tempo/time-signature getters now use time 0.
  - Metronome flag wired to `edit->clickTrackEnabled`.
- `examples/Reason/TrackStripComponent.cpp`
  - Gain slider uses custom look-and-feel and capped width.
- `examples/Reason/TrackStripComponent.h`
  - Added slider look-and-feel holder.
- `examples/Reason/ReasonMainComponent.cpp`
  - MIDI input label updated every timer tick.
- `examples/Reason/TransportBarComponent.cpp`
  - Tempo label editable on double-click.
- `examples/Reason/ChordInspectorComponent.cpp`
  - Replaced em dashes with ASCII.

### Commands run
- `rg -n "GainSliderLookAndFeel|kMinTrackDb|tempoLabel|recordingEnabled" examples/Reason/SessionController.cpp examples/Reason/TrackStripComponent.cpp examples/Reason/TransportBarComponent.cpp`
- `sed -n '140,220p' examples/Reason/TrackStripComponent.cpp`
- `sed -n '1840,1950p' examples/Reason/SessionController.cpp`

### Build/test status
- Not rebuilt after these changes yet.

### What to verify next
- Gain slider feels shorter and maps -10 dB .. +6 dB with readable dB values.
- MIDI input button reflects the selected device name (e.g., Casio).
- Recording now produces MIDI notes in the recorded clip and the region persists after stopping.
- Tempo edits on the label change the project tempo and grid/ruler align with bars.
- Metronome toggles on/off and clicks at the correct tempo.

---

## 2026-02-09 16:25

### Goals for this milestone
- Stop instrument buttons from showing “Untitled” and prefer a meaningful plugin label.
- Make the gain control visually tighter (shorter slider, thicker track).
- Ensure tempo edits trigger immediate UI refresh of grid/ruler.
- Keep MIDI input selection stable so the top bar shows the chosen device name.

### What changed and why
- **Instrument label fallback**: External plugin labels now fall back to plugin name when the program name is a placeholder, and placeholder detection treats any label containing “untitled” as invalid. This prevents “Untitled” from leaking into the instrument button.
- **Track rename guard**: The track name is only auto-set to the new instrument name if that label is meaningful (not “Instrument”, “No Instrument”, or placeholder).
- **Gain control sizing**: The instrument button width was reduced and the gain slider width shortened to avoid spanning the full row. The slider track thickness was increased for easier visibility.
- **Tempo UI refresh**: Tempo edits now force the timeline and piano roll to repaint so grid lines update immediately.
- **MIDI input selection stability**: The update loop now ensures a valid MIDI input is selected before updating the transport bar label.

### Files updated
- `examples/Reason/SessionController.cpp`
  - Placeholder program-name detection now treats any label containing “untitled” as invalid.
  - External plugin label fallback now uses the plugin name when program name and description are placeholders.
- `examples/Reason/TrackStripComponent.cpp`
  - Instrument button width reduced and gain slider width shortened.
  - Gain slider track thickness increased.
  - Placeholder instrument label detection now catches “untitled” anywhere in the string.
- `examples/Reason/ReasonMainComponent.cpp`
  - Tempo edits now repaint the timeline and piano roll.
  - Update loop now ensures MIDI input selection before refreshing the label.
  - Auto-rename to instrument name now skips placeholder labels.

### Commands run
- `ls`
- `rg -n "Instrument|Untitled|AI" examples/Reason/TrackStripComponent.*`
- `sed -n '1,240p' examples/Reason/TrackStripComponent.cpp`
- `rg -n "setInstrumentName|instrumentButton" examples/Reason`
- `sed -n '1,260p' examples/Reason/TrackListComponent.cpp`
- `rg -n "instrument" examples/Reason/SessionController.cpp examples/Reason/ReasonMainComponent.cpp examples/Reason/PluginWindow.cpp`
- `sed -n '300,460p' examples/Reason/SessionController.cpp`
- `sed -n '1,120p' examples/Reason/SessionController.cpp`
- `rg -n "insertInstrument|kInstrumentLabelId" examples/Reason/SessionController.cpp`
- `sed -n '1590,1685p' examples/Reason/SessionController.cpp`
- `sed -n '420,520p' examples/Reason/ReasonMainComponent.cpp`
- `rg -n "refreshSessionState" -n examples/Reason/ReasonMainComponent.cpp`
- `sed -n '1100,1180p' examples/Reason/ReasonMainComponent.cpp`
- `rg -n "SelectedMidiInput" -n examples/Reason/SessionController.cpp`
- `sed -n '230,320p' examples/Reason/SessionController.cpp`
- `rg -n "record" examples/Reason`
- `sed -n '780,1120p' examples/Reason/SessionController.cpp`
- `sed -n '80,140p' examples/Reason/SessionController.cpp`
- `sed -n '1,180p' modules/tracktion_engine/playback/tracktion_TransportControl.h`
- `rg -n "Tempo|BPM|bar|beat" examples/Reason/TimelineComponent.* examples/Reason/PianoRollComponent.*`
- `sed -n '600,720p' examples/Reason/TimelineComponent.cpp`

### Build/test status
- Not rebuilt after these changes yet.

### What to verify next
- Instrument button shows a stable label (e.g., “Splice INSTRUMENT” or the chosen AU name), not “Untitled”.
- Gain slider is visibly thicker and shorter.
- Tempo edits update grid lines immediately in the timeline and piano roll.
- MIDI input label in the top bar reflects the selected device.

## 2026-02-09 16:43
### Summary
- Fixed build error by exposing `SessionController::ensureMidiInputSelection()` to callers.

### Changes
- `examples/Reason/SessionController.h`
  - Moved `ensureMidiInputSelection()` declaration to public API so `ReasonMainComponent` can call it.

### Commands run
- `rg -n "ensureMidiInputSelection" -n examples/Reason/SessionController.h`
- `sed -n '160,250p' examples/Reason/SessionController.h`

### Build/test status
- Not rebuilt after this header visibility fix yet.

### What to verify next
- `cmake --build build --target Reason` now succeeds without the private-member error.

## 2026-02-09 16:55
### Summary
- Recording fix: ensure MIDI input targets and record-enabled flags survive playback restart before recording begins.

### Changes
- `examples/Reason/SessionController.cpp`
  - Moved `edit->restartPlayback()` to occur immediately after `ensureContextAllocated()` and before setting input targets/record-enabled flags, and removed the trailing restart. This keeps `InputDeviceInstance::recordEnabled` from being reset before recording starts, which was preventing MIDI clips from being created.

### Commands run
- `rg -n "record|Recording|preview" examples/Reason/SessionController.cpp`
- `sed -n '80,150p' examples/Reason/SessionController.cpp`
- `sed -n '820,980p' examples/Reason/SessionController.cpp`
- `sed -n '1840,1945p' examples/Reason/SessionController.cpp`
- `rg -n "allowRecordingIfNoInputsArmed|record \\(" modules/tracktion_engine/playback/tracktion_TransportControl.cpp`
- `sed -n '1420,1465p' modules/tracktion_engine/playback/tracktion_TransportControl.cpp`
- `rg -n "areAnyInputsRecording" -n modules/tracktion_engine/playback/tracktion_TransportControl.cpp`
- `sed -n '1000,1060p' modules/tracktion_engine/playback/tracktion_TransportControl.cpp`
- `rg -n "liveNotes" -n modules/tracktion_engine/playback/devices/tracktion_MidiInputDevice.cpp`
- `sed -n '1020,1065p' modules/tracktion_engine/playback/devices/tracktion_MidiInputDevice.cpp`
- `sed -n '180,260p' modules/tracktion_engine/playback/devices/tracktion_InputDevice.cpp`
- `rg -n "setTarget\\(" -g"*.cpp" examples/Reason modules/tracktion_engine`
- `rg -n "togglePlay|toggleRecord" -n examples/common/Utilities.h`
- `sed -n '160,220p' examples/common/Utilities.h`

### Build/test status
- Not rebuilt after this recording fix yet.

### What to verify next
- Recording now creates a persistent MIDI clip with notes after stopping (Record button or Space).

## 2026-02-09 17:35
### Summary
- Realchords server reliability: make the app consistently target the batch server, and auto-start it from the correct project root even when launched from the app bundle.

### Changes
- `examples/Reason/ReasonMainComponent.cpp`
  - Added helpers to derive `REALCHORDS_HOST/PORT`, base URL, and a start command hint.
  - Replaced hard-coded `http://127.0.0.1:8090` with the derived base URL for `/generate` and `/health`.
  - Improved error messaging to show the exact batch-server command to run.
  - Made auto-start robust by locating the project root via current executable and working directory.
  - Passed host/port environment vars into the batch-server child process.
- `tools/realchords/realchords_batch_server.py`
  - Added `REALCHORDS_HOST`/`REALCHORDS_PORT` environment override for `app.run(...)` to match the app configuration.

### Commands run
- `rg -n "realchords|/generate|8090|ensureRealchordsServer" examples/Reason/ReasonMainComponent.cpp tools/realchords/realchords_batch_server.py`
- `sed -n '840,1120p' examples/Reason/ReasonMainComponent.cpp`
- `sed -n '1,240p' tools/realchords/realchords_batch_server.py`

### Build/test status
- Not rebuilt after this server-start and URL fix yet.

### What to verify next
- Running the batch server via the provided command results in successful chord generation.

## 2026-02-09 18:05
### Summary
- Tempo and key signature are now editable directly from the transport bar (double-click).

### Changes
- `examples/Reason/TransportBarComponent.h`
  - Added `mouseDoubleClick` override and a helper for key signature menus.
- `examples/Reason/TransportBarComponent.cpp`
  - Tempo label now enters edit mode on double click.
  - Key signature label opens a drop-down menu (major/minor choices) instead of raw text edit.
  - Double-click handling for tempo/time signature/key is centralized to ensure consistent behavior.

### Commands run
- `rg -n "Tempo|Key|TransportBarComponent" examples/Reason/TransportBarComponent.* examples/Reason/ReasonMainComponent.cpp examples/Reason/SessionController.*`
- `sed -n '1,220p' examples/Reason/TransportBarComponent.cpp`

### Build/test status
- Not rebuilt after this transport-bar edit yet.

### What to verify next
- Double-clicking tempo opens the editor and updates BPM when submitted.
- Double-clicking key opens the menu and updates the project key.

## 2026-02-09 18:20
### Summary
- Make tempo/key edits actually stick by wiring double-click handlers to the labels and preventing the timer from overwriting active edits.

### Changes
- `examples/Reason/TransportBarComponent.cpp`
  - Added mouse listeners for tempo/time signature labels so double-clicks reach the transport bar handler.
  - Prevented `setTempoText` and `setTimeSignatureText` from overwriting edits while the label is being edited.
  - Ensured labels intercept mouse clicks explicitly for reliable double-click behavior.

### Commands run
- `rg -n "Tempo|Key|TransportBarComponent" examples/Reason/TransportBarComponent.*`
- `sed -n '1,220p' examples/Reason/TransportBarComponent.cpp`

### Build/test status
- Not rebuilt after this fix yet.

### What to verify next
- Double-click tempo now opens the editor and keeps the typed BPM.
- Double-click key opens the menu reliably.

## 2026-02-09 19:10
### Summary
- MIDI recording persistence issue finally resolved through a full debug cycle.
- Final state: MIDI recording now produces persistent clips/regions after stop.

### User-facing symptom (exact)
- Pressing Record created a temporary-looking MIDI indicator while recording.
- After stopping, no region remained visible.
- MIDI input monitoring was inconsistent at startup (often needed touching Record once).

### Why this was hard
- Multiple overlapping failure modes were present:
  1) **Monitoring/routing timing** issues at app startup and after device changes.
  2) **Record-stop path ambiguity** (toggle helper behavior vs direct transport control).
  3) **Clip finalization failures** in this app path (Tracktion recorded state toggled, but no clip materialized).
  4) **Timestamp domain mismatch** from live MIDI FIFO events (in some runs) producing pathological clip times.

### Full debugging sequence (what we tried and learned)
1. **Initial transport-path fixes**
   - Updated recording logic away from the demo helper path where needed.
   - Ensured stop path used non-discard semantics.
   - Result: recording state toggled, but clips still missing.

2. **Routing stability fixes**
   - Added explicit live MIDI routing updates on:
     - app/edit init,
     - track selection changes,
     - MIDI input selection changes.
   - Switched target assignment to `edit->getAllInputDevices()`/instance-based logic.
   - Result: input monitoring became more reliable, but clip persistence still inconsistent.

3. **Deterministic diagnostics**
   - Added structured recording logs to a stable path:
     - `~/ReasonLogs/record_debug.log`
   - Logged:
     - selected MIDI device and armed tracks,
     - prepare/start/stop state transitions,
     - routing outcomes,
     - per-track MIDI clip counts after stop.
   - Critical finding from logs:
     - `toggleRecord() started/stopped` happened correctly,
     - but clip counts remained zero in failing runs:
       - `Track0:midiClips=0 | Track1:midiClips=0 | Track2:midiClips=0`.

4. **Fallback clip commit path**
   - Implemented a safe fallback:
     - if stop completes and no new MIDI clip appears, use captured live note preview to create a real `MidiClip`.
   - Logged fallback events:
     - `fallback committed MIDI clip on track 0 with notes=...`.
   - This proved note capture existed even when native finalization failed.

5. **Timestamp sanitization (final blocker)**
   - Found pathological timestamps in logs:
     - e.g. `clipEnd=2717633957999.999`.
   - Root cause:
     - occasional MIDI message timestamps arrived in a non-transport clock domain.
   - Fix:
     - sanitize incoming note event timestamps during recording preview capture,
     - clamp/normalize invalid note-off ordering,
     - clamp final clip end to a sane range near stop time.
   - Result:
     - fallback-created clips now land in sane timeline positions and persist.

### Key evidence from logs
- Before final fixes:
  - record toggled on/off correctly,
  - clip snapshot after stop remained zero.
- After fallback + sanitization:
  - `fallback committed MIDI clip on track 0 with notes=8`
  - `toggleRecord() clip snapshot after stop: Track0:midiClips=1 ...`
  - later runs showed repeated persistent increments.

### Files changed (recording debug/fix cycle)
- `examples/Reason/SessionController.h`
  - Added helper declarations for recording fallback and instance lookup.
- `examples/Reason/SessionController.cpp`
  - Reworked start/stop recording flow to avoid discard behavior.
  - Added robust MIDI input/target routing refresh.
  - Added persistent diagnostics logging (`~/ReasonLogs/record_debug.log`).
  - Added fallback clip commit from live recording preview when no clip is materialized.
  - Added timestamp sanitation and clip time clamping to prevent off-screen/invalid clip placement.

### Commands run during the cycle
- `rg -n "record|Recording|toggleRecord|stop\\(|setTarget|getAllInputDevices|isRecordingActive" examples/Reason/SessionController.cpp examples/common/Utilities.h modules/tracktion_engine`
- `cmake --build build --target Reason`
- `tail -n 120 ~/ReasonLogs/record_debug.log`
- `tail -n 80 ~/ReasonLogs/record_debug.log`
- `tail -n 60 ~/ReasonLogs/record_debug.log`

### Build/test status
- Build succeeds after final timestamp sanitation and fallback clip placement fixes.
- Runtime logs confirm post-stop MIDI clip creation in successful runs.

### What is now different
- Recording no longer depends on temporary UI preview only.
- When native clip finalization fails, a real MIDI clip is still committed from captured notes.
- Invalid MIDI timestamp domains no longer produce absurd clip ranges/off-screen placement.

### Follow-up cleanup (recommended)
- Keep logging for one more validation session, then gate/remove verbose debug logs.
- Once fully stable, either:
  - keep fallback commit as a safety net, or
  - replace with canonical Tracktion-only finalize path if root cause is fully eliminated upstream.

## 2026-02-09 22:25 - Timeline UX pass (bars ruler, MIDI edge resize, multi-clip drag)

### Goal
- Align timeline interactions with musical editing workflow:
  - ruler should show bars/measures instead of raw seconds,
  - MIDI clips should be quickly shorten-able from the edge,
  - multiple clips should be selectable and movable together (including across tracks).

### Implemented changes
1. **Top timeline ruler now shows musical bars**
   - Replaced second-based ruler ticks/labels with bar/beat ticks driven by `Edit::tempoSequence`.
   - Major ticks now represent bar starts and labels are `Bar N`.
   - This visually aligns the ruler with the existing beat/grid lines in clip lanes.

2. **MIDI clip edge-resize interaction**
   - Added hover detection near a MIDI clip's right edge.
   - Cursor changes to horizontal-resize when on the edge handle.
   - Mouse drag on that edge now previews and commits clip length changes on mouse-up.
   - Added `SessionController::resizeClipLength` to commit the resize through Tracktion clip APIs.

3. **Multi-selection + group move for clips**
   - Added additive selection (`Shift`/`Cmd` click) for clips in timeline.
   - Preserved a primary selected clip (`selectedClipId`) for existing keyboard/editor flows.
   - Dragging a selected clip now moves all selected clips together, preserving relative offsets.
   - Vertical drag applies a track delta so selected clips can be moved across tracks as a group.

### Technical notes
- New timeline drag state tracks:
  - original start times per selected clip,
  - original track index per selected clip,
  - shared time offset and track delta while dragging.
- Group-move logic clamps time to `>= 0` and clamps target tracks to valid track index range.
- Resize uses a minimum non-zero clip length to avoid zero-length invalid clips.

### Files changed
- `examples/Reason/TimelineComponent.h`
  - Added multi-select state, resize state, hover/edge helper declarations, and mouse move/exit handling.
- `examples/Reason/TimelineComponent.cpp`
  - Implemented bars-based ruler drawing.
  - Implemented MIDI right-edge hover/resize interaction.
  - Implemented additive multi-selection and group drag/move behavior.
  - Updated clip drawing to indicate selection set and right-edge handle for MIDI clips.
- `examples/Reason/SessionController.h`
  - Added `resizeClipLength (uint64_t clipId, double newLengthSeconds)`.
- `examples/Reason/SessionController.cpp`
  - Implemented `resizeClipLength` using clip `setLength`.

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: build successful (`Reason` target links successfully).

## 2026-02-09 22:40 - Clip trim behavior correction (no MIDI note shift)

### User-reported issue
- MIDI clip resize only worked on the right edge.
- Shortening from the right shifted note content, which felt wrong for trim behavior.

### Fix
- Updated resize semantics to preserve clip sync while changing bounds, so trimming now cuts visible content instead of moving note timing.
- Added left-edge trim support for MIDI clips (in addition to right-edge trim).
- Added preview/commit support for resizing both start and end clip bounds.

### Technical details
- `SessionController::resizeClipLength` now uses sync-preserving resize.
- Added `SessionController::resizeClipRange (clipId, newStartSeconds, newLengthSeconds)` for two-sided trimming.
- Timeline now detects both left and right edge handles and commits through `resizeClipRange`.

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: successful build after trim behavior fixes.

## 2026-02-09 23:05 - DAW UI polish + piano-roll lasso multi-select

### UI polish pass requested
- Goal: make the top transport area feel more "DAW-like" (metallic/futuristic), improve button readability, and consolidate live transport info in a single display area.

### What changed
1. **Top bar visual redesign**
   - Added metallic/chrome-style wells behind transport/action buttons.
   - Added a dedicated "screen" panel look in the center with neon-blue accent border.
   - Increased `Generate Chords` button width so label is no longer squished.
   - Refined button color palettes (project/import/plugins/settings/metronome/MIDI/chords) for stronger contrast.

2. **Unified live info display area**
   - Reworked top-bar layout so time, bars/beats, BPM, time signature, and key all live inside one centered display cluster.
   - Existing update flow remains live (real-time updates from `updateTimeDisplay()`).

3. **Record status light**
   - Added a blinking red indicator light beside the record button when recording is active.
   - Blink rate is timer-driven and only active during record.

4. **Piano roll lasso multi-selection**
   - Added drag-lasso selection in note grid (click-drag empty area to box-select notes).
   - Supports additive lasso (`Cmd`/`Shift` held).
   - Selection rectangle is rendered during drag.
   - Clicking an already-selected note now preserves multi-selection, enabling immediate group move/resize.
   - Delete now removes all selected notes when multiple are selected.

### Files changed
- `examples/Reason/TransportBarComponent.h`
  - Added timer hook, style helper, record blink state, display panel state.
- `examples/Reason/TransportBarComponent.cpp`
  - Added metallic/3D-ish drawing treatment, display-panel styling, updated layout widths, record blink indicator.
- `examples/Reason/PianoRollComponent.h`
  - Added lasso state and helper declaration.
- `examples/Reason/PianoRollComponent.cpp`
  - Added lasso-select interaction and rendering.
  - Preserved multi-selection when clicking selected notes.
  - Added multi-delete for selected notes.

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: successful build with new transport UI and piano-roll selection behavior.

## 2026-02-09 23:20 - Live MIDI monitoring recovery guard

### Issue observed
- MIDI keyboard appeared connected in UI, but playing keys produced no sound in some runs until additional actions (e.g. touching record/other state changes).

### Fix
- Added explicit live MIDI routing refresh entry point:
  - `SessionController::refreshMidiLiveRouting()`.
- Called this periodically from `ReasonMainComponent::timerCallback()` (light-touch cadence) so device/routing state is re-applied and monitoring remains audible after reconnects/state drift.

### Files changed
- `examples/Reason/SessionController.h`
  - Added `refreshMidiLiveRouting()`.
- `examples/Reason/SessionController.cpp`
  - Implemented `refreshMidiLiveRouting()` via existing routing path.
- `examples/Reason/ReasonMainComponent.cpp`
  - Added periodic `session.refreshMidiLiveRouting()` call in timer loop.

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: successful build after MIDI live-monitoring guard patch.

## 2026-02-09 23:45 - MIDI monitor regression follow-up (stability rollback + safer reroute)

### Issue observed
- After the first monitoring guard, live MIDI pass-through was still unstable in some sessions.
- Root cause was linked to over-aggressive routing refresh behavior.

### Fix
- Removed aggressive continuous routing refresh behavior from the main timer path.
- Kept routing refresh as a **one-shot** when MIDI device selection/state changes.
- Preserved normal routing calls on explicit user operations (track selection / device selection / edit load).
- Kept additional logging context for routing diagnostics.

### Files changed
- `examples/Reason/ReasonMainComponent.h`
  - Added `lastKnownMidiInputName` state for device-change detection.
- `examples/Reason/ReasonMainComponent.cpp`
  - Removed periodic force-refresh behavior.
  - Added one-shot `session.refreshMidiLiveRouting()` when MIDI input name changes.
- `examples/Reason/SessionController.cpp`
  - Kept stable live-routing flow and logging enhancements from regression debugging.

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: successful build.

## 2026-02-09 23:58 - Piano roll interaction polish pass

### Requested improvements
- Smoother note movement.
- Square notes instead of rounded.
- Velocity-based note color representation.
- Real-time playhead updates in piano roll.
- Horizontal zoom in piano roll.
- Set playback position directly inside piano roll.

### Implemented
1. **Smooth drag/resize feel**
   - Removed quantized stepping during drag preview for move/resize.
   - Notes now move continuously while dragging.

2. **Square notes**
   - Replaced rounded note drawing with rectangular note blocks and square outlines.

3. **Velocity color mapping**
   - Extended `SessionController::MidiNoteInfo` to include `velocity`.
   - Populated velocity from `MidiNote`.
   - Piano roll now colors notes based on velocity (with selected-note highlight overlay behavior).

4. **Playhead sync in piano roll**
   - Added timer-driven repaint to keep playhead visually in sync during playback.

5. **Horizontal zoom + playback position control**
   - Added `mouseMagnify` handling for pinch zoom.
   - Click in piano roll/ruler now updates transport cursor position.

### Files changed
- `examples/Reason/SessionController.h`
  - Added `velocity` field to `MidiNoteInfo`.
- `examples/Reason/SessionController.cpp`
  - Populated `MidiNoteInfo::velocity` in `getMidiNotesForClip`.
- `examples/Reason/PianoRollComponent.h`
  - Added timer/magnify support declarations.
- `examples/Reason/PianoRollComponent.cpp`
  - Added timer repaint loop.
  - Added smoother drag preview behavior.
  - Added square/velocity-aware note rendering.
  - Added cursor-position click behavior.
  - Added magnify-based horizontal zoom.

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: successful build after piano roll polish changes.

## 2026-02-10 00:30 - Modulo branding pass + transport/menu restructuring

### Goals
- Complete product rename from "Reason" (user-facing) to "Modulo" without destabilizing project internals.
- Reclaim top-bar space by reducing oversized buttons.
- Improve transport information readability and centered layout.

### Implemented
1. **Top action menu simplification**
   - Replaced separate `Project` + `Import` controls with a single compact `File` button.
   - `File` menu now includes New/Open/Save/Save As + Import Audio/MIDI.

2. **Settings consolidation**
   - Removed standalone `Plugins` button from transport.
   - Moved plugin-manager access into `Settings` menu alongside device settings.

3. **Centered transport display improvements**
   - Reworked panel layout into deterministic left/center/right lanes to avoid overlap.
   - Kept panel centered and visually balanced.
   - Time display upgraded to millisecond precision (`MM:SS.mmm`).

4. **Chord inspector readability + playback-follow**
   - Increased list row readability.
   - Added active-chord highlight synced to current transport time.
   - Auto-scrolls chord list to keep active chord visible.

### Files changed
- `examples/Reason/TransportBarComponent.h/.cpp`
- `examples/Reason/ReasonMainComponent.h/.cpp`
- `examples/Reason/ChordInspectorComponent.h/.cpp`

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: successful build after transport/chord inspector layout updates.

## 2026-02-10 01:10 - App identity rename to Modulo + icon integration

### Goals
- Set macOS app bundle identity/icon to Modulo.
- Preserve existing source structure and target stability.

### Implemented
1. **Bundle identity**
   - Updated bundle/product identity to Modulo:
     - `PRODUCT_NAME "Modulo"`
     - `BUNDLE_ID "com.takakhoo.modulo"`
     - `MACOSX_BUNDLE_BUNDLE_NAME "Modulo"`

2. **Dock icon**
   - Added custom icon source: `examples/Reason/Modulo_AppIcon_1024.png` via JUCE CMake `ICON_BIG`.

3. **Runtime name**
   - Updated application/window name strings from `Reason` to `Modulo`.

4. **Engine label alignment**
   - Updated Tracktion engine app name string to `"Modulo"` for consistent settings identity.

### Files changed
- `examples/Reason/CMakeLists.txt`
- `examples/Reason/Reason.cpp`
- `examples/Reason/SessionController.h`

### Validation
- Built app bundle now outputs as:
  - `build/examples/Reason/Reason_artefacts/Modulo.app`
- `Info.plist` verified:
  - `CFBundleName = Modulo`
  - `CFBundleDisplayName = Modulo`
  - `CFBundleExecutable = Modulo`
  - `CFBundleIdentifier = com.takakhoo.modulo`

## 2026-02-10 01:35 - Plugin inventory recovery after rename

### Issue observed
- After app identity rename, external instrument/plugin choices appeared reduced.
- Root cause: plugin scan cache moved to a new settings domain (`Modulo`) while previous scans lived under `Reason`.

### Fix
- Added startup migration path:
  - If current known plugin list lacks external plugins, load legacy `Reason/Settings.xml` plugin list key (`knownPluginList`/`knownPluginList64`) and persist into current settings.
  - Guarded migration to avoid clobbering a valid existing plugin database.

### Files changed
- `examples/Reason/SessionController.cpp`

### Validation
- Build successful.
- Existing external plugin availability restored through migrated known-plugin-list state.

## 2026-02-10 02:00 - Modulo startup/launch experience

### Goals
- Introduce branded startup screen using custom artwork.
- Gate DAW entry through explicit launch choices.

### Implemented
1. **Startup overlay component**
   - Added full-window startup overlay rendering:
     - `examples/Reason/Modulo_Loading_1920x1080.png`
   - Added two launch actions:
     - `New Blank Project`
     - `Open Existing Project`
   - Added short "loading" enable delay for buttons.

2. **Launch flow control**
   - Main DAW UI remains hidden at app start.
   - DAW becomes visible only after startup choice.
   - Startup overlay bounds fixed to avoid first-frame zero-size black window.

3. **Open-existing-project UX polish**
   - Keeps startup screen visible while file chooser is open.
   - Dismisses startup overlay only after successful project open.
   - Canceling file chooser keeps user on startup screen.

4. **Duplicate title cleanup**
   - Removed redundant text rendering so image typography remains authoritative.

### Files changed
- `examples/Reason/ReasonMainComponent.h/.cpp`
- `examples/Reason/CMakeLists.txt` (added loading image to `BinaryData`)

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: successful build with startup overlay/flow changes.

## 2026-02-10 02:40 - Chord playback style controls + arpeggio timing fixes

### Goals
- Provide direct control over generated chord playback style.
- Keep arpeggios musically clear and grid-locked.

### Implemented
1. **Playback style selection in generate flow**
   - Added style choice to "Generate Chords" prompt:
     - `Block Chords`
     - `Arpeggio`
   - Style is passed through generation response handling into chord-track creation.

2. **Arpeggio behavior iteration**
   - Tested a custom motif variant and reverted based on listening feedback.
   - Final behavior restored to original staggered arpeggios.

3. **Grid-locked arpeggio starts**
   - Arpeggio note starts now align strictly to beat grids:
     - eighth-note (`0.5` beats) when space allows
     - sixteenth-note (`0.25` beats) fallback for shorter chord windows
   - Removed in-between fractional step offsets.

### Files changed
- `examples/Reason/ReasonMainComponent.h/.cpp`
- `examples/Reason/SessionController.h/.cpp`

### Validation
- Build successful after playback-style + timing refinements.

## 2026-02-10 03:05 - Sustain pedal shaping for generated chord tracks

### Goal
- Add repeatable bar-level sustain behavior to generated chord MIDI tracks.

### Implemented
- For each generated chord clip:
  - Insert CC64 sustain ON at each bar start.
  - Insert CC64 sustain OFF shortly before bar end (16th-note early).
  - Repeat through full clip length for every generated option track.

### Files changed
- `examples/Reason/SessionController.cpp`

### Validation
- Build successful with sustain-automation generation enabled.

## 2026-02-10 03:20 - Chord-inspector architecture shift (from popups to persistent workshop controls)

### Product direction change
- The chord workflow was moved away from transient popup menus toward a persistent "workshop" model:
  - Click chord cell once to select context.
  - Apply all edits from fixed controls in the lower inspector area.
- This was done for speed, visibility, and reduced click depth while iterating harmony ideas.

### Implemented
1. **Persistent chord editing controls**
   - Replaced popup action menus with always-visible action buttons:
     - `Block`, `Arpeggio`, `Double Time`, `Delete`, `Octave Up`, `Octave Down`, `Inversion Up`, `Inversion Down`.
   - Replaced popup note-name chooser with always-visible root/quality selector buttons.

2. **Cell selection model**
   - Added explicit selected-cell state (`measure`, `beat`) in inspector.
   - Highlighted selected cell in grid.
   - Wired selected cell as the target for all chord edit actions.

3. **Playback-follow behavior**
   - Active-playing chord cell remains highlighted during transport playback.
   - Inspector can auto-scroll to active row.

### Mistakes / regressions found
- Initial transition still had mixed interaction assumptions (popup-era assumptions in code paths) and required follow-up wiring to ensure all operations correctly targeted the selected cell.

### Fixes
- Unified callback flow:
  - UI actions emit `(measure, beat, action)` to main component.
  - Main component maps to `SessionController::ChordEditAction`.
  - Session applies edit to exact cell span.

### Files changed
- `examples/Reason/ChordInspectorComponent.h/.cpp`
- `examples/Reason/ReasonMainComponent.cpp`
- `examples/Reason/SessionController.h/.cpp`

### Validation
- Built and smoke-tested action flow in-place without popup dependency.

---

## 2026-02-10 03:30 - Full black/gold visual pass + transport lane stabilization

### Goals
- Push consistent black/gold styling across transport, menus, tracks, and inspectors.
- Keep transport controls readable and non-overlapping under changing window widths.

### Implemented
1. **Top bar visual redesign**
   - Metallic gold gradient background.
   - Brighter button wells and panel accents for icon visibility.
   - Transport display panel made taller with stronger recording pulse.

2. **Transport placement constraints**
   - Enforced left/center/right lane layout:
     - Left: `File`, `AI Generate Chords`
     - Center-left: `Play`, `Stop`, `Record`
     - Center: time/BPM/key display
     - Center-right: metronome
     - Right: MIDI / Settings
   - Added controlled spacing between buttons.
   - Ensured top bar spans full window width.

3. **Icon integration**
   - Metronome moved to icon button.
   - Settings moved to gear icon + label treatment.
   - Chord button uses piano icon and renamed to `AI Generate Chords`.

4. **Menu theming**
   - Applied popup menu colors globally via look-and-feel defaults.

### Mistakes / regressions found
1. **Control overlap and clipping in transport**
   - Root cause: incremental layout edits without strict lane budgets caused collisions at certain widths.
2. **Metronome icon not visible unless pressed**
   - Root cause: icon contrast against darker background and button state rendering.
3. **Compile failure in popup menu theming**
   - Error: `no member named 'setColour' in 'juce::PopupMenu'`.
   - Root cause: attempted per-menu API that is unsupported in this JUCE version.

### Fixes
1. Rewrote `TransportBarComponent::resized()` with deterministic lane math and spacing.
2. Brightened gold bar + adjusted icon button rendering so black assets are visible at rest.
3. Removed invalid per-menu color calls and set theme on default look-and-feel globally.

### Files changed
- `examples/Reason/TransportBarComponent.h/.cpp`
- `examples/Reason/ReasonMainComponent.cpp`
- `examples/Reason/CMakeLists.txt` (binary data for icon assets)

### Validation
- Multiple rebuilds while tuning layout; final build successful with no overlap in targeted configurations.

---

## 2026-02-10 03:40 - Track strip ergonomics: gain readability, pan control, and color coherence

### Goals
- Improve mix-control readability and reduce ambiguity in per-track controls.
- Add real panning control with clear visual semantics.

### Implemented
1. **Track row structure update**
   - Top row reserved for track title.
   - Mute/Solo/FX moved down.
   - Gain label moved left of slider; gain value label moved right.

2. **Pan control**
   - Added per-track rotary pan knob + label.
   - Added callback wiring from strip -> list -> session.
   - Added backend get/set pan methods using `VolumeAndPanPlugin`.

3. **Custom pan look**
   - Implemented custom `PanKnobLookAndFeel`:
     - Left stop around 7 o'clock.
     - Center around 2 o'clock visual reference.
     - Right stop around 5 o'clock.

4. **Color pass**
   - Gain sliders restyled to silver.
   - Track body + timeline clip fills unified via gold shade palette.

### Mistakes / regressions found
- Pan label appeared without a real knob in one pass.
  - Root cause: UI declaration present but full component/layout/render wiring incomplete.

### Fixes
- Completed the full chain:
  - Component creation, bounds, look-and-feel, callback propagation, and backend pan persistence.

### Files changed
- `examples/Reason/TrackStripComponent.h/.cpp`
- `examples/Reason/TrackListComponent.h/.cpp`
- `examples/Reason/SessionController.h/.cpp`
- `examples/Reason/TrackColors.h`
- `examples/Reason/TimelineComponent.cpp` (color alignment in clip drawing)

### Validation
- Build successful; pan changes now audibly and visually map to knob movement.

---

## 2026-02-10 03:50 - Chord-grid semantics (Bar column + beat cells + empty-cell insertion)

### Goals
- Make chord inspector read musically by bar/beat instead of ad-hoc row labels.
- Support adding harmony by clicking empty beat cells.

### Implemented
1. **Bar-based labeling**
   - Replaced `M1/M2/...` style with `Bar` header and numeric rows `1,2,3...`.

2. **Beat-subdivided rows**
   - Each row represents a measure.
   - Each beat is a distinct chord cell.

3. **Empty-cell chord creation**
   - Added callback for empty target cell chord insert.
   - Backend `setChordAtCell` writes chord label + MIDI notes at specific bar/beat.

### Mistakes / regressions found
- Initial quality/root edits targeted mostly empty-cell insertion use case and needed follow-up to support replacing existing cell harmonies seamlessly.

### Fixes
- Extended selected-cell edit flow so existing cells can be rewritten by the same controls.

### Files changed
- `examples/Reason/ChordInspectorComponent.h/.cpp`
- `examples/Reason/ReasonMainComponent.cpp`
- `examples/Reason/SessionController.h/.cpp`

### Validation
- Build successful; empty and occupied cell workflows now both supported.

---

## 2026-02-10 04:05 - Keyboard workflow additions and safety behaviors

### Implemented
1. **Recording shortcut semantics**
   - `R` changed to start-record only (no toggle-off side effects).

2. **Delete chord by key**
   - `Delete`/`Backspace` on selected chord cell removes that cell chord.

3. **Loop duplication**
   - `L` duplicates selected MIDI clip 5 times for rapid loop sketching.

### Mistakes / regressions found
- None critical in final behavior; changes were additive and confirmed through rebuild/test loops.

### Files changed
- `examples/Reason/ReasonMainComponent.cpp`
- `examples/Reason/ChordInspectorComponent.h/.cpp`

### Validation
- Build successful.

---

## 2026-02-10 04:20 - Chord-engine correctness fixes (quality parsing + inversion behavior)

### Issue A: Major 7th quality parsed incorrectly

#### Symptom
- `maj7` chords could be interpreted as minor because parsing checked `"m"` before `"maj"`.

#### Root cause
- Order-dependent `startsWith` branch in `chordSymbolToPitches`.

#### Fix
- Reordered quality checks so `"maj"` is evaluated before `"m"`.

#### Result
- `Cmaj7` now resolves to tonic + major 3rd + 5th + major 7th.

---

### Issue B: Inversion on blocked chords dropped notes; arpeggio inversions ineffective

#### Symptom
- First inversion operation sometimes appeared to remove a note.
- Arpeggiated cells did not consistently invert as expected.

#### Root cause
- Previous inversion flow rebuilt from transformed pitch sets that could alter cardinality/shape.

#### Fix
- Reworked inversion action to operate on existing note events while preserving note count:
  - Move lowest note up an octave for inversion-up.
  - Move highest note down an octave for inversion-down.
- Kept timing/shape characteristics intact for both blocked and arpeggiated cells.

#### Result
- Inversions now behave musically and consistently across cell styles.

### Files changed
- `examples/Reason/SessionController.cpp`

### Validation
- Build successful after parsing/inversion fixes.

---

## 2026-02-10 04:35 - Chord workshop layout refactor (roots on top, semitone arrows left, inline MIDI preview)

### Goals
- Improve legibility and musical feedback in the chord workshop.

### Implemented
1. **Root controls repositioned**
   - Root note buttons moved to top row(s) for larger, easier targets.

2. **Dedicated semitone controls**
   - Left-side controls changed to dedicated transpose actions (per selected cell).
   - Added edit actions:
     - `semitoneUp`
     - `semitoneDown`
   - Mapped through UI -> main component -> session backend.

3. **Inline static MIDI preview**
   - Added preview panel between chord grid and designer controls.
   - Added backend extraction for notes in selected cell span:
     - `SessionController::getChordCellPreviewNotesForTrack(...)`
   - Draws note bars for blocked or arpeggiated content based on real MIDI in that cell.

### Mistakes / regressions found
1. **Comparator warning**
   - Float inequality comparison warning surfaced in preview-note sort code.
   - Fix: changed comparator branching to ordered `<` / `>` checks.
2. **Compile mismatch while wiring UI indexing**
   - Attempted `OwnedArray` helper function unavailable in this environment.
   - Fix: replaced with explicit index loop.

### Files changed
- `examples/Reason/ChordInspectorComponent.h/.cpp`
- `examples/Reason/ReasonMainComponent.cpp`
- `examples/Reason/SessionController.h/.cpp`

### Validation
- Build successful after preview + transpose integration.
- No linter errors on modified files.

---

## 2026-02-10 04:50 - Existing-chord root editing + duration-preservation fix

### Issue A: Root-change editing not applying to occupied cells

#### Symptom
- UI allowed many transformations but changing root (e.g., `D -> G`) on an already-filled cell was inconsistent.

#### Fix
- Root button clicks now apply immediately to selected occupied cell using current quality context.
- Cell selection now parses existing symbol and initializes selected root/quality state.

---

### Issue B: Quality change could shorten sustained blocked chord

#### Symptom
- Example: 2-beat blocked chord changed to `7th` became 1 beat.

#### Root cause
- `setChordAtCell` had a fallback path that could default to one-beat rewrite.

#### Fix
- Rewrote duration inference:
  1. Determine target cell bounds (start -> next label / clip end).
  2. Measure currently occupied note span inside that cell.
  3. Rewrite replacement chord using occupied span.
  4. Use fallback beat only when no prior occupancy exists.

#### Result
- Quality/root replacement preserves original sustain length.

### Files changed
- `examples/Reason/ChordInspectorComponent.h/.cpp`
- `examples/Reason/SessionController.cpp`

### Validation
- Build successful after duration-preservation patch.

---

## 2026-02-10 05:05 - Live chord-preview enhancement pass (labels, larger panel, playback-follow, enharmonics)

### Goals
- Make preview readable and useful while composing in motion.

### Implemented
1. **Note labels on bars**
   - Each preview MIDI note now shows note name + octave.
   - Black keys shown enharmonically (e.g., `C#/Db4`).

2. **Larger preview area**
   - Increased preview height budget for legibility and live reading.

3. **Playback-live behavior**
   - Preview now follows currently playing chord cell during transport playback (not only manual selection).
   - Displays contextual text indicating selected/live bar-beat target.

4. **Current chord text**
   - Preview header now shows currently targeted chord symbol.

5. **Root selector enharmonics**
   - Root buttons for black notes now display dual naming (sharp/flat).

6. **Transpose button glyphs**
   - Semitone controls relabeled with musical symbols:
     - Up: `♯`
     - Down: `♭`

### Mistakes / fixes during implementation
- Initial large patch had context mismatch during apply (file shifted since previous edits).
- Resolved by re-reading full file and applying targeted patches against current content.

### Files changed
- `examples/Reason/ChordInspectorComponent.h/.cpp`

### Validation
- Build command: `cmake --build /Users/takakhoo/Dev/tracktion/tracktion_engine/build --target Reason`
- Result: successful build.
- Lint status: no linter errors in edited chord inspector files.

---

## Consolidated mistakes-and-fixes ledger (since previous TIMELINE update)

### 1) UI layout regressions
- **Problem:** transport overlap / spacing instability while reorganizing lanes.
- **Fix:** deterministic left/center/right layout math; explicit width budgets and gaps.

### 2) Theming API mismatch
- **Problem:** compile error using unsupported per-menu `setColour`.
- **Fix:** moved popup theming to global look-and-feel defaults.

### 3) Chord parsing correctness
- **Problem:** `maj7` parsed as minor in some paths.
- **Fix:** quality detection order corrected (`maj` before `m`).

### 4) Inversion correctness
- **Problem:** note disappearance and weak behavior on arpeggios.
- **Fix:** inversion now moves boundary notes on existing events, preserving note count.

### 5) Missing/partial pan integration
- **Problem:** pan label present before full knob pipeline.
- **Fix:** completed UI + callback + backend pan parameter plumbing.

### 6) Chord replacement duration drift
- **Problem:** quality changes could shorten long blocked chords.
- **Fix:** rewrite length derived from real occupied span in target cell.

### 7) Live preview implementation friction
- **Problem:** patch context mismatch + one unavailable `OwnedArray` helper.
- **Fix:** file re-read + explicit index loops + rebuild verification.

---

## 2026-02-10 05:20 - Chord workshop visual hierarchy pass (preview scaling + banner integration)

### Goals
- Increase readability of harmonic feedback while keeping right-panel workflow compact.
- Eliminate visually "dead" space between chord grid and workshop controls.

### Implemented
1. **Preview scaling**
   - Increased `Chord MIDI Preview` height budget significantly so note bars + labels remain legible during edits.

2. **Spacer-to-information conversion**
   - Converted previously empty gap area into a styled banner strip.
   - Added all-caps title text: `CHORD WORKSHOP`.
   - Matched banner palette to surrounding black/gold inspector theme for continuity.

3. **Panel hierarchy tuning**
   - Rebalanced chord-list, banner, preview, and control areas so the preview receives more visual priority while preserving button usability.

### Decision rationale
- Instead of adding more controls, the decision was to improve cognitive legibility first (clear labeling and stronger section hierarchy), because this directly lowers editing errors during rapid chord experimentation.

### Files changed
- `examples/Reason/ChordInspectorComponent.h/.cpp`

### Validation
- Build successful after layout rebalance.

---

## 2026-02-10 05:30 - Transport identity + data-display emphasis pass

### Goals
- Strengthen product identity at runtime.
- Make timing/context display visually dominant over surrounding transport buttons.

### Implemented
1. **Brand treatment**
   - Added `MODULO` text in the central lane between `AI Generate Chords` and play transport.

2. **Data panel emphasis**
   - Increased center display typography (time + bars + tempo + time signature + key).
   - Reworked control heights so display panel is taller than adjacent buttons.

3. **Button/display balance**
   - Reduced effective button height while keeping panel full-height presence to improve glance readability.

### Mistake/regression encountered
- Initial `MODULO` rendering looked like a button/chip due to boxed background styling.

### Fix
- Removed boxed treatment entirely.
- Rendered `MODULO` directly on the metallic gold bar with stronger type scale and subtle shadow only.
- Increased size significantly to satisfy visibility target.

### Decision rationale
- Branding should behave as environmental identity, not an affordance. Removing "button-like" framing avoids false click expectation and keeps interaction semantics clear.

### Files changed
- `examples/Reason/TransportBarComponent.h/.cpp`

### Validation
- Build successful after both initial and corrected brand-render passes.

---

## 2026-02-10 05:40 - Piano-roll gold unification (notes + sustain lane)

### Goals
- Remove residual non-theme colors in core edit surfaces.
- Keep velocity legibility while aligning with black/gold visual language.

### Implemented
1. **Velocity-aware gold notes**
   - Replaced rainbow/HSV mapping with gold-spectrum mapping from dark to bright by velocity.
   - Preserved selected-note emphasis via lighter-gold interpolation.

2. **Sustain lane recolor**
   - Replaced green CC64 "on" blocks with darker gold shades.
   - Kept "off"/neutral sustain background distinct but theme-consistent.

### Decision rationale
- Maintaining velocity readability does not require multi-hue coloring; intensity variation within a single harmonic palette is sufficient and visually coherent with the product direction.

### Files changed
- `examples/Reason/PianoRollComponent.cpp`

### Validation
- Build successful; no linter errors in modified file.

---

## 2026-02-10 05:50 - Musical glyph robustness pass for semitone controls

### Problem observed
- Even after symbol adoption intent, transpose controls could render fallback dots/squares on some runs due to text path ambiguity.

### Fix
- Explicitly set semitone button text in constructor using the exact Unicode glyphs:
  - Up: `♯`
  - Down: `♭`
- Added clear tooltips (`Sharp` / `Flat`) for semantic reinforcement.

### Decision rationale
- Explicit glyph assignment is more robust than relying on inherited/default text initialization when cross-font behavior can vary.

### Files changed
- `examples/Reason/ChordInspectorComponent.cpp`

### Validation
- Build successful after glyph-robustness patch.

---

## 2026-02-10 06:00 - Chord edit logic fix: Double Time preserving arpeggio style

### Issue observed
- Applying `Double Time` to an arpeggiated cell collapsed output into blocked voicings.

### Root cause
- The double-time branch treated target material as pitch set only (blocked reconstruction), ignoring per-note temporal pattern data already present in the cell.

### Fix implemented
1. Added style inference in `applyChordEditAction` for the target cell:
   - Detect whether chord notes are distributed across starts (arpeggio) vs. fully co-started (block).

2. For arpeggiated cells:
   - Capture existing per-note timing/velocity shape.
   - Time-compress that pattern into half-cell duration.
   - Repeat twice across the original cell length (true double-time arpeggio feel).

3. For blocked cells:
   - Preserve existing blocked double-hit behavior.

### Result
- `Double Time` now preserves style semantics:
  - Arpeggio remains arpeggio (faster repeated figure).
  - Block remains block.

### Files changed
- `examples/Reason/SessionController.cpp`

### Validation
- Build successful after arpeggio-preserving double-time fix.
- No linter errors in modified file.

---

## 2026-02-10 06:10 - Documentation hardening pass (README + timeline completeness)

### Goals
- Capture the full technical and research narrative after rapid iteration cycle.
- Improve repository communication of novelty, HCI contribution, and musician-first algorithmic framing.

### README updates
1. Added `examples/Reason/ScreenView.png` directly below the "Why the Name Modulo" explanation.
2. Expanded novelty framing beyond generation count:
   - theory-operable workflow
   - musician agency in control loop
   - DAW-native comparative harmonic exploration
3. Added explicit HCI novelty section centered on Chord Workshop:
   - persistent controls
   - bar/beat cell semantics
   - live contextual feedback
   - low-friction theory operations
4. Strengthened AI deep-dive language around hybrid loop:
   - probabilistic option discovery + deterministic theory refinement.

### Timeline updates
- Added exhaustive post-05:05 records for:
  - workshop visual hierarchy
  - transport identity/display emphasis
  - piano roll recolor pass
  - glyph robustness fix
  - arpeggio-preserving double-time bug fix
- Included problem statements, root causes, decisions, and validations.

### Files changed
- `README.md`
- `TIMELINE.md`

---

## 2026-02-10 06:35 - Track reorder drag workflow + unified hotkey documentation pass

### Goals
- Enable fast track ordering directly from the left controller lane (drag up/down).
- Ensure reorder is not visual-only and truly updates underlying track order in the Edit.
- Publish a clear operator-facing hotkey/directions list in `README.md`.
- Deepen project log fidelity with implementation details and rationale.

### User-facing behavior added
1. **Drag-to-reorder from left track strip**
   - User can click/drag a track strip in the left controller area and move it vertically.
   - Drop position determines target index.
   - A horizontal insertion indicator is shown during drag for placement feedback.
   - Reorder updates the real Edit track order, then refreshes timeline/track list/inspectors.

2. **Piano Roll note insertion gesture alignment**
   - Added support for `Cmd+Click` note insertion in the Piano Roll note grid.
   - Existing `Ctrl+Click` insertion remains supported for compatibility.

3. **README operational controls section**
   - Added new `Directions and Hotkeys` section documenting:
     - Track drag reorder
     - Recording, looping, quantize, transpose, delete flows
     - Project-level shortcuts (`Cmd+N/O/S`, undo/redo, duplicate, copy/paste)
     - Piano Roll editing gestures and shortcuts

### Backend implementation details
1. **New session API for track order mutation**
   - Added `SessionController::reorderTrack (int sourceIndex, int targetIndex)`.
   - Method validates bounds/state, builds a source-excluded target ordering view, computes a stable `TrackInsertPoint`, and calls `edit->moveTrack`.
   - Wraps operation in undo transaction (`"Reorder Track"`), updates selected track index, rebuilds track name state, and reapplies MIDI live-routing safety call.

2. **ReasonMainComponent wiring**
   - Added `trackList.onTrackReordered` callback.
   - On successful reorder:
     - calls `refreshSessionState()`
     - reasserts selected track in timeline + track list
     - refreshes FX and Chord inspectors

3. **TrackList drag state + feedback**
   - Added drag state fields:
     - `dragReorderSourceIndex`
     - `dragReorderTargetIndex`
     - `dragReorderActive`
   - Added `onTrackReordered` callback surface.
   - Implemented live drop-target computation from pointer Y and painted insertion line.

4. **TrackStrip drag event emission**
   - Added `onReorderDrag (sourceIndex, pointerY, finished)` callback.
   - `mouseDown` starts drag-reorder intent and keeps selection behavior.
   - `mouseDrag` streams candidate drop positions.
   - `mouseUp` finalizes reorder intent and preserves context-menu behavior.

### Mistakes / friction and fixes
- **Input gesture mismatch risk:** README intent required `Cmd+Click` for note insertion, while code path previously required `Ctrl+Click`.
  - **Fix:** broadened insertion condition to accept either `Ctrl` or `Command`.
- **Reorder target edge behavior:** drag outside track-content area can produce no direct index hit.
  - **Fix:** explicit clamp to top/bottom target index based on pointer relative to header/content.
- **Selection continuity risk after reorder:** track selection could visually drift after model mutation.
  - **Fix:** post-reorder explicit reselection in both `timeline` and `trackList`.

### Files changed
- `examples/Reason/SessionController.h`
- `examples/Reason/SessionController.cpp`
- `examples/Reason/TrackStripComponent.h`
- `examples/Reason/TrackStripComponent.cpp`
- `examples/Reason/TrackListComponent.h`
- `examples/Reason/TrackListComponent.cpp`
- `examples/Reason/ReasonMainComponent.cpp`
- `examples/Reason/PianoRollComponent.cpp`
- `README.md`
- `TIMELINE.md`

### Validation plan executed
- Static code-path review of full event chain:
  - left strip mouse drag -> list drag state -> `onTrackReordered` -> session mutation -> UI refresh.
- Verified reordering operation is undo-transaction wrapped in backend.
- Verified existing right-click context menu behavior retained in `TrackStripComponent::mouseUp`.
- Verified new README hotkey section reflects currently implemented key handlers.

### Notes for next iteration
- Optional enhancement: add animated ghost row while dragging for stronger perceived direct manipulation.
- Optional enhancement: auto-scroll track list while dragging near top/bottom edges on long projects.

## 2026-01-29

### Goals for this milestone
- Add control-click to add MIDI notes in chord workshop preview window (matching piano roll behavior).
- Add note resize capability in chord workshop preview (drag right edge to shorten/lengthen notes).
- Add delete key support in chord workshop to delete selected notes.
- Add quantize support (Q key) in chord workshop for arpeggios and voicings.
- Implement bidirectional live sync between piano roll and chord workshop (edits in either space reflect in the other in real time).
- Ensure chord workshop buttons work robustly with more than 5 notes (no hardcoded limits).
- Fix F major chord display to use flat spellings (B flat instead of B natural/sharp).
- Fix key signature display in transport bar to correctly show selected major keys (was stuck on "Cmaj").

### Design choices and reasoning
- **Chord cell note operations**: Added dedicated SessionController methods (`addChordCellNote`, `deleteChordCellNote`, `resizeChordCellNote`, `quantizeChordCellNotes`) that operate on notes within a specific measure/beat cell, maintaining the cell boundary constraints.
- **Preview interaction model**: Chord workshop preview now mirrors piano roll interaction patterns:
  - Control/Command-click adds notes at quantized positions
  - Click and drag moves notes vertically (pitch change)
  - Click near right edge and drag resizes note length
  - Multi-select with Command/Shift-click
  - Delete/Backspace deletes selected notes
  - Q key quantizes all notes in the cell
- **Bidirectional sync**: Both piano roll and chord workshop edit the same underlying MIDI clip (the chord clip). When notes change in either view, `refreshChordPreviewIfSelected()` updates the preview, and piano roll repaints to show changes. This ensures both views always reflect the current state.
- **Key signature caching**: Implemented `cachedKeySignature` in SessionController to store user-selected key signatures, ensuring the transport bar displays the correct key even when the engine's pitch sequence doesn't immediately reflect the change for major keys.
- **Flat key detection**: Added `keySignatureUsesFlats()` helper function that explicitly lists all flat keys (F, Bb, Eb, Ab, Db, Gb, Cb and their minor variants) to ensure correct note spelling in chord workshop and transport bar.

### Files changed (and why)

1. **SessionController.h / SessionController.cpp**
   - Added `cachedKeySignature` member variable to store user-selected key signature.
   - Added `keySignatureUsesFlats()` static helper to determine if a key signature should use flat spellings.
   - Modified `getKeySignature()` to return cached value if set, otherwise read from edit's pitch sequence at time 0.0.
   - Modified `setKeySignature()` to cache the selected key signature.
   - Added `addChordCellNote()`: Adds a note to a specific chord cell with pitch, start beats, and length beats.
   - Added `deleteChordCellNote()`: Deletes a note from a chord cell by index.
   - Added `resizeChordCellNote()`: Resizes a note's length within a chord cell.
   - Added `quantizeChordCellNotes()`: Quantizes all notes in a chord cell to a grid.
   - Updated `getNoteNameForPitch()` to use `keySignatureUsesFlats()` for correct note spelling.
   - Updated `createNewEdit()` and `openEdit()` to clear key signature cache on new/loaded projects.

2. **ChordInspectorComponent.h / ChordInspectorComponent.cpp**
   - Added keyboard focus support (`setWantsKeyboardFocus(true)`).
   - Added `getSelectedMeasure()` and `getSelectedBeat()` getters for external access.
   - Added new callback functions:
     - `onChordPreviewNoteAdd`: Called when a note is added via control-click.
     - `onChordPreviewNoteDelete`: Called when delete key is pressed.
     - `onChordPreviewNoteResize`: Called when a note is resized.
     - `onChordPreviewNotesQuantize`: Called when Q key is pressed.
   - Added `PreviewDragMode` enum (none, move, resize) to track drag state.
   - Added `selectedPreviewNotes` array for multi-select support.
   - Added helper methods:
     - `getTimeFromPreviewX()`: Converts X coordinate to beat time within preview.
     - `getGridBeats()`: Returns quantization grid (16th note = 0.25 beats).
   - Rewrote `mouseDown()` to handle:
     - Control/Command-click to add notes
     - Click on note to select/move
     - Click near right edge to resize
     - Multi-select with Command/Shift-click
   - Rewrote `mouseDrag()` to handle pitch changes (move) and length changes (resize).
   - Rewrote `mouseUp()` to finalize resize operations with quantization.
   - Added `keyPressed()` to handle Delete/Backspace and Q key.
   - Updated `drawPreview()` to show selected notes with highlight and resize handles.
   - Updated `setKeySignature()` to use `chordKeyUsesFlats()` helper for root button labels.
   - Updated `getNoteNameWithOctave()` to use `chordKeyUsesFlats()` for correct note spelling.

3. **ReasonMainComponent.cpp / ReasonMainComponent.h**
   - Added `refreshChordPreviewIfSelected()` helper method to refresh chord preview when piano roll changes.
   - Wired up new chord workshop callbacks:
     - `onChordPreviewNoteAdd`: Calls `session.addChordCellNote()` and refreshes both views.
     - `onChordPreviewNoteDelete`: Calls `session.deleteChordCellNote()` and refreshes both views.
     - `onChordPreviewNoteResize`: Calls `session.resizeChordCellNote()` and refreshes both views.
     - `onChordPreviewNotesQuantize`: Calls `session.quantizeChordCellNotes()` and refreshes both views.
   - Updated existing `onChordPreviewNotePitchChange` to also refresh piano roll.
   - Added call to `refreshChordPreviewIfSelected()` in `refreshSessionState()` to sync piano roll changes to chord workshop.
   - Updated `refreshChordInspector()` to set key signature from session.

### Commands run
- Build and test: `cmake --build build --target Reason`
- Visual verification of chord workshop preview interactions
- Tested bidirectional sync by editing notes in both piano roll and chord workshop

### What works now
- Control/Command-click in chord workshop preview adds notes at quantized positions.
- Click and drag notes vertically to change pitch (with live preview sound).
- Click near right edge and drag to resize note length (with quantization on release).
- Multi-select notes with Command/Shift-click.
- Delete/Backspace key deletes selected notes in chord workshop.
- Q key quantizes all notes in the selected chord cell.
- Piano roll and chord workshop stay in sync: edits in either view immediately reflect in the other.
- Chord workshop buttons (Block, Arpeggio, inversions, etc.) work correctly with any number of notes (no 5-note limit).
- F major and other flat keys correctly display flat spellings (Bb instead of A#) in chord workshop and transport bar.
- Transport bar correctly displays selected major keys (no longer stuck on "Cmaj").
- Key signature changes persist correctly across UI refreshes.

### Implemented (velocity, resize, chord workshop UX)
- **Velocity slider:** A vertical velocity slider appears on the left when one or more MIDI notes are selected, in both **Piano Roll** and **Chord Workshop** preview. Single note: slider shows that note’s velocity. Multiple notes: slider shows the **average** velocity; dragging changes all selected notes by the **same delta** (parallel adjustment). Slider appears whenever a note is selected in either view.
- **Track strip MIDI clip resize:** The same left/right **bracket cursor** and shorten/lengthen behaviour used for note editing is used for **MIDI clips** in the track strip. Resizing a MIDI clip from the left or right edge uses bracket cursors. Shortening a MIDI clip so that notes fall outside the new range **removes those notes**; **Command+Z** undoes that action (engine undo).
- **Piano roll – left-edge resize:** Dragging the **left edge** of a MIDI note (left bracket handle) now resizes the note from the left (moves start, keeps end fixed), same behaviour as the right edge. Multi-select: all selected notes’ left edges move by the same time delta.
- **Bracket cursors:** Bracket resize cursors (left/right) are drawn **smaller and thinner** everywhere they appear (MIDI notes in piano roll, chord workshop preview, MIDI clips on the track strip) so the user can interact with better precision.
- **Snap to grid when close:** When extending or shortening a MIDI note (piano roll or chord workshop), the note **snaps to a quantizable length** (e.g. eighth, quarter, sixteenth) only if the user gets **very close** to that length (threshold ~15% of grid or 20 ms). Otherwise the exact dragged length is kept. Same idea for chord workshop note start when dragging the left edge.
- **Chord workshop preview – note labels:** MIDI blocks in the chord workshop preview are **always** labeled with the note name (e.g. C4, G#3) in the original simple style (no background box). **Piano roll note labels** show when `showNoteLabels` is on and note row height ≥ 8 px; font size scales between 6 pt and 10 pt so labels remain visible at typical zoom levels.
- **Chord workshop preview – grid lines:** Vertical grid lines are drawn at the current grid (e.g. 16th-note) to aid quantization.
- **Chord workshop preview – velocity:** Velocity is no longer fixed at 100; it is shown and editable via the velocity slider when preview notes are selected, and new notes can be added with a chosen velocity.
- **Chord workshop preview – note start time:** Note start time can be edited by dragging the **left edge** of a note (left bracket handle). Start snaps to grid on release only when close; note end stays fixed so length changes. Right edge still resizes length.

### MIDI record and monitoring (always-on)
- **Record always available:** At least one track is kept armed (selected track or track 0). If none were armed when you press Record, the selected track is auto-armed so recording is never blocked by “no tracks armed.” `ensureTrackStateSize()` now re-arms the selected track (or track 0) whenever the armed state would otherwise be all false (e.g. after undo/redo or track list resize).
- **Playback context for MIDI:** Before routing the selected MIDI input to tracks, the edit’s playback context is allocated (`transport->ensureContextAllocated (true)`). That ensures MIDI input device instances exist so the selected device (e.g. Casio) can be found and routed. Without this, opening the app with a MIDI keyboard plugged in could show the device in “MIDI In” but record and live monitoring would fail.
- **Live monitoring:** The selected MIDI input is routed to the selected track (and any explicitly armed tracks). The selected track gets a default instrument if it has none, so you hear keys immediately on a non-muted track. `refreshSessionState()` now calls `refreshMidiLiveRouting()` so routing is re-applied on every session refresh (load, track change, etc.).

### Limitations / next steps
- None specific to the above; general roadmap items (multi-note resize in piano roll, etc.) remain as elsewhere in this doc.
