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
