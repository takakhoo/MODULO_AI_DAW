# MODULO AI DAW

MODULO (Musician-Owned DAW User-Led Orchestration) is a standalone DAW built on Tracktion Engine + JUCE, with an AI-native harmony workflow focused on chord generation and exploration.

This repository contains the code needed to build and run MODULO, including the custom DAW app and integrated ReaLchords-based generation flow.

![MODULO Startup Screen](examples/Reason/Modulo_Loading_1920x1080.png)

## Why the Name "MODULO"

`MODULO` is not only an acronym. In computation, the modulo (`%`) operator returns the remainder after division. The remainder metaphor is central to this project:

- Most Music AI systems optimize toward a single "best" continuation.
- MODULO instead exposes the "remainder" of plausible harmonic futures.
- By generating multiple chord alternatives for the same melody, it helps the musician navigate the residual solution space that one-shot generation usually hides.

In short, MODULO is designed to return what deterministic generation leaves on the table, then give musicians theory-native controls to shape those alternatives into production-ready harmony.

![MODULO Chord Workshop View](examples/Reason/ScreenView.png)

## What MODULO Does

- Timeline-based DAW editing with audio + MIDI tracks.
- Real-time MIDI monitoring, recording, clip editing, and plugin hosting.
- Piano roll editing with lasso multi-select, smooth drag/resize, velocity coloring, and zoom.
- Chord option generation for melodies using a ReaLchords batch server.
- Chord inspection panel with playback-follow highlighting.
- Enhanced transport UI with bars/beats + time + tempo + key + time signature.
- Branded `MODULO.app` bundle with custom dock icon.

## Directions and Hotkeys

Core interaction directions:

- Reorder tracks by dragging a track from the left track controller area up/down.
- Open/close Piano Roll for selected clip with `E`, then edit notes directly in-grid.
- Click a chord cell in Chord Workshop to target edits, then use persistent edit buttons below.

Global / arrangement hotkeys:

- `Space`: toggle play/stop, and if recording is active, stop + commit recording take.
- `R`: start recording (does not toggle recording off).
- `L`: create loop copies (adds 5 duplicates of selected MIDI clip forward in time).
- `U`: move all MIDI notes in selected track up one octave.
- `D`: move all MIDI notes in selected track down one octave.
- `Delete` / `Backspace`: delete selected chord cell (if any), else selected clip, else selected track.
- `I`: add idea marker at current insertion time.
- `E`: toggle Piano Roll for currently selected clip.

Project/session shortcuts:

- `Cmd+N`: new project.
- `Cmd+O`: open project.
- `Cmd+S`: save project.
- `Cmd+Shift+S`: save project as.
- `Cmd+Z`: undo (preserves open Piano Roll state).
- `Cmd+Shift+Z`: redo (preserves open Piano Roll state).
- `Cmd+D`: duplicate selected track.
- `Cmd+C` / `Cmd+V`: copy/paste selected clip at cursor position.

Piano Roll shortcuts and gestures:

- `Cmd+Click` (or `Ctrl+Click`) in note grid: add a note at clicked position using current grid length.
- `Q`: quantize selected notes to current grid subdivision.
- `Cmd+A`: select all notes in active Piano Roll clip.
- `Cmd+C` / `Cmd+V`: copy/paste selected notes in Piano Roll.
- `Delete` / `Backspace`: delete selected note(s).
- Mouse wheel / trackpad: horizontal scroll timeline in Piano Roll.
- `Cmd + Mouse wheel` (or pinch zoom): horizontal zoom around cursor anchor.

## Deep Learning + HCI Thesis Positioning

MODULO is centered on a single research claim: **reinforcement-learning-driven harmony generation becomes substantially more useful when paired with an HCI layer that encodes expert music-theory workflows**.

- **Deep learning core:** MODULO's harmony backend is informed by ReaLchords, which uses reinforcement learning for adaptive online accompaniment under temporal and harmonic constraints.
- **HCI core:** model outputs are exposed as editable harmonic hypotheses in the Chord Workshop, where musicians inspect, transform, and compare options at bar/beat resolution.
- **Joint novelty:** MODULO evaluates success by interaction quality in addition to output quality: decision speed, editability, and theory-consistent control.

This is the key thesis framing: model intelligence and interface intelligence are co-dependent.

## Novelty and Thesis Focus

The contribution is not isolated "AI chord generation." It is a musician-first system where RL-informed harmony generation, music-theory operations, and timeline context stay unified in one DAW loop.

Core contribution axes:

- **Multi-hypothesis harmony, not one-shot prediction:** melody-conditioned generation returns several viable accompaniments as parallel, comparable tracks.
- **Chord Workshop post-generation transformations:** users edit generated harmony with real theory operations (quality, inversion, octave, semitone, block/arpeggio, double-time) directly on bar/beat cells.
- **DAW-native control loop:** outputs are instantly playable/editable MIDI in arrangement context, with transport and instrumentation feedback.
- **Musician agency at expert level:** harmonic function, voicing movement, and timing remain inspectable and controllable throughout.

This is the thesis direction: expanding harmonic decision space for advanced users who think in functional harmony, voice-leading, and arrangement context, while keeping iteration speed high enough for practical songwriting and production.

### Why this matters for Music AI (hard claim)

- **From answer-machine to co-creator:** the model proposes; the musician decides.
- **From offline demo to production runtime:** the loop runs in a real DAW (recording, plugins, transport, timeline).
- **From generic output to controlled diversity:** multiple candidates plus Chord Workshop transformations reduce mode collapse and improve controllability.
- **From black-box output to expert operation:** music-theory reasoning is first-class, not post-hoc.

## Music-Theory-First Interaction Design (HCI Novelty)

MODULO's HCI contribution is the **Chord Workshop**: a persistent, grid-aware harmony environment that tightly couples symbolic intent, temporal structure, and audible feedback.

- **Bar/beat cell model:** Each row is a bar, subdivided by beats, so harmony edits occur where musicians already reason temporally.
- **Persistent controls instead of hidden popups:** Theory operations remain visible and discoverable, reducing interaction overhead and mode confusion.
- **Live chord-state feedback:** The workshop tracks currently playing chord position and displays chord MIDI preview with pitch labels, so edits can be evaluated while transport is running.
- **Embodied theory operations:** Actions such as inversion, semitone shift, and quality changes manipulate the actual MIDI realizations and preserve timing semantics.
- **Comparative workflow support:** Multiple generated chord-option tracks can be inspected and reshaped quickly, enabling A/B-style harmonic decision making.

This is the HCI novelty: expert users operate in theory terms without losing DAW immediacy, and AI outputs remain transparent to musical reasoning.

## AI Technical Deep Dive

MODULO's harmony subsystem follows an online accompaniment framing rather than an offline "generate entire song and accept" paradigm.

- **Reinforcement-learning-informed generation objective:** the ReaLchords direction emphasizes adaptive accompaniment quality over time, aligning chord decisions with evolving melodic context.
- **Online conditioning pipeline:** melody is converted into a time-stepped representation suitable for local model serving, preserving strict alignment to timeline context.
- **Multi-hypothesis generation:** the system requests several accompaniment candidates for the same melody and instantiates each directly as an editable MIDI track.
- **Timeline-faithful reconstruction:** model-domain events are converted back into beat-accurate DAW MIDI clips with bar/beat coherence.
- **Chord Workshop post-generation transformations:** generated chords are transformed with deterministic operations (quality/root/inversion/octave/semitone/block/arpeggio/double-time) while preserving cell semantics.
- **Gesture-level rendering controls:** block/arpeggio behavior, beat-locked arpeggio timing, and sustain-shaping help close the gap between model output and production-ready material.
- **In-context evaluation loop:** candidate ranking is performed by the musician during playback in arrangement context, not by model confidence alone.

The net effect is a hybrid intelligence loop: deep-learning-based option discovery plus Chord Workshop theory operations for controlled refinement.

## Architecture Overview

- `examples/Reason/`  
  Main MODULO app implementation (UI, timeline, transport, recording, piano roll, chord inspector).
- `examples/Reason/SessionController.*`  
  Core DAW/session logic: transport, recording, MIDI routing, plugin insertion, clip operations, chord-track creation.
- `examples/Reason/ReasonMainComponent.*`  
  Main orchestration and ReaLchords request/response integration.
- `tools/realchords/`  
  Local ReaLchords batch server environment and launch scripts.

## Build and Run (macOS)

### 1) Clone

```bash
git clone https://github.com/takakhoo/MODULO_AI_DAW.git
cd MODULO_AI_DAW
```

### 2) Configure + Build

```bash
cmake -S . -B build
cmake --build build --target Reason
```

Note: The internal CMake target name remains `Reason` for stability, but it builds `MODULO.app`.

### 3) Launch

```bash
open "build/examples/Reason/Reason_artefacts/Modulo.app"
```

## ReaLchords Setup (Chord Generation)

MODULO expects a local ReaLchords batch server. If not running, generation requests fail gracefully with startup hints.

Typical local launch:

```bash
tools/realchords/.venv/bin/python tools/realchords/realchords_batch_server.py
```

Optional environment overrides:

- `REALCHORDS_HOST` (default `127.0.0.1`)
- `REALCHORDS_PORT` (default `8090`)

See `examples/Reason/REALCHORDS_WALKTHROUGH.md` for deeper integration/training notes.

## Credits and Citations

### Tracktion Engine Credit

MODULO is built on top of Tracktion Engine and JUCE. Tracktion Engine provides the core sequencing/editing/playback/plugin-host infrastructure that MODULO extends with custom UI and AI-assisted harmony workflows.

- Tracktion Engine repository: <https://github.com/Tracktion/tracktion_engine>
- Feature overview: `FEATURES.md` in this repository
- Tracktion Engine docs: <https://tracktion.github.io/tracktion_engine/modules.html>
- Tracktion benchmarks: <https://tracktion.github.io/tracktion_engine/benchmarks.html>
- Tracktion Developers page (licensing): <https://www.tracktion.com/develop/tracktion-engine>
- Tracktion Engine JUCE forum category: <https://forum.juce.com/c/tracktion-engine>

### ReaLchords Credit

MODULO's adaptive chord accompaniment direction is informed by ReaLchords:

**Adaptive Accompaniment with ReaLchords**  
Yusong Wu, Tim Cooijmans, Kyle Kastner, Adam Roberts, Ian Simon, Alexander Scarlatos, Chris Donahue, Cassie Tarakajian, Shayegan Omidshafiei, Aaron Courville, Pablo Samuel Castro, Natasha Jaques, Cheng-Zhi Anna Huang.  
Accepted at ICML 2024.  
arXiv: <https://arxiv.org/abs/2506.14723>  
DOI: <https://doi.org/10.48550/arXiv.2506.14723>

Key technical ideas in ReaLchords that motivate this project:

- online accompaniment generation for live/co-creative settings
- RL fine-tuning for adaptive temporal + harmonic coherence
- teacher-guided divergence/distillation to retain strong future-aware priors while operating online

## Licensing and Compliance Notes

When cloning/building/distributing MODULO, ensure licensing obligations are satisfied:

- Tracktion Engine uses GPL/commercial licensing; review Tracktion terms before distribution.
- JUCE licensing is separate; Tracktion Engine is not covered by a JUCE license, and vice versa.
- Some Tracktion features and terms vary by license tier/version (including enterprise scenarios).
- ReaLchords/RealJam and related model assets may carry separate upstream terms.
- Third-party libraries included in Tracktion Engine have their own licenses (see upstream Tracktion license documentation and notices).

In short: MODULO is a derivative application built on major upstream ecosystems, and proper attribution plus license compliance is required for real-world release.

## What You Need to Clone for MODULO

This repo is centered on the MODULO app path:

- Primary runtime app: `examples/Reason` (branded as MODULO at build/runtime).
- Core engine dependencies are kept in-repo because they are required to compile and run MODULO.
- Other Tracktion example targets are present, but MODULO development is concentrated in the files above.

## Repository

Target GitHub repository: [takakhoo/MODULO_AI_DAW](https://github.com/takakhoo/MODULO_AI_DAW)

