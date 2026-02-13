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

- **Tracks:** Reorder by dragging in the left track controller. **Piano Roll:** open/close with `E`; edit notes in-grid (drag to move, drag left/right edges to resize; velocity slider when notes selected). **Chord Workshop:** click a cell to target it, then use the persistent edit buttons and preview (drag note edges to resize or move start; velocity slider when notes selected).

### Command reference

<sub>

| Action | Key / gesture |
|--------|----------------|
| **Transport & arrangement** | |
| Play / stop | `Space` |
| Stop + commit take (while recording) | `Space` |
| Start recording | `R` |
| Loop copies (5× selected MIDI clip) | `L` |
| Transpose track up / down 1 octave | `U` / `D` |
| Add idea marker | `I` |
| Toggle Piano Roll for selected clip | `E` |
| Delete chord cell → clip → track | `Delete` / `Backspace` |
| **Project** | |
| New / Open / Save / Save As | `Cmd+N` / `Cmd+O` / `Cmd+S` / `Cmd+Shift+S` |
| Undo / Redo | `Cmd+Z` / `Cmd+Shift+Z` |
| Duplicate track | `Cmd+D` |
| Copy / paste clip at cursor | `Cmd+C` / `Cmd+V` |
| **Piano Roll** (when focused) | |
| Add note at click | `Cmd+Click` or `Ctrl+Click` |
| Quantize selected notes | `Q` |
| Select all notes | `Cmd+A` |
| Copy / paste notes | `Cmd+C` / `Cmd+V` |
| Delete selected note(s) | `Delete` / `Backspace` |
| Horizontal scroll / zoom | Mouse wheel / `Cmd+wheel` (or pinch) |
| **Chord Workshop** (preview focused) | |
| Delete selected note(s) | `Delete` / `Backspace` |
| Quantize notes in cell | `Q` |

</sub>

## Research positioning and novelty

MODULO’s core claim: **RL-driven harmony generation becomes much more useful when paired with an HCI layer that encodes expert music-theory workflows.** Model intelligence and interface intelligence are co-dependent.

**Novelty (without repetition):**

- **Multi-hypothesis harmony:** Melody-conditioned generation returns several viable accompaniments as parallel, comparable MIDI tracks—not a single one-shot prediction. The musician chooses and refines in arrangement context.
- **Chord Workshop as theory-first HCI:** A persistent bar/beat grid where users edit generated harmony with real theory operations (quality, root, inversion, octave, semitone, block/arpeggio, double-time). Controls stay visible; chord preview and playback feedback are live. Expert users operate in theory terms without losing DAW immediacy.
- **Production loop, not demo:** The loop runs inside a real DAW—recording, plugins, transport, timeline. Outputs are immediately playable and editable; candidate ranking happens by the musician during playback, not by model confidence alone.
- **Controlled diversity:** Multiple candidates plus Chord Workshop transformations reduce mode collapse and keep harmonic function, voicing, and timing inspectable and controllable.

**Technical outline:** The harmony backend is informed by ReaLchords (RL for adaptive online accompaniment). Melody is converted to a time-stepped representation for local serving; the system requests several candidates per melody and instantiates each as an editable MIDI track. Chord Workshop then applies deterministic theory operations (quality, inversion, block/arpeggio, etc.) with beat-accurate semantics. The result is a hybrid loop: deep-learning option discovery plus theory operations for refinement.

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

