# MODULO AI DAW

MODULO (Musician-Owned DAW User-Led Orchestration) is a standalone DAW built on Tracktion Engine + JUCE, with an AI-native harmony workflow focused on chord generation and exploration.

This repository contains the code needed to build and run MODULO, including the custom DAW app and integrated ReaLchords-based generation flow.

## What MODULO Does

- Timeline-based DAW editing with audio + MIDI tracks.
- Real-time MIDI monitoring, recording, clip editing, and plugin hosting.
- Piano roll editing with lasso multi-select, smooth drag/resize, velocity coloring, and zoom.
- Chord option generation for melodies using a ReaLchords batch server.
- Chord inspection panel with playback-follow highlighting.
- Enhanced transport UI with bars/beats + time + tempo + key + time signature.
- Branded `Modulo.app` bundle with custom dock icon.

## Novelty and Thesis Focus

The core novelty is the harmony-assistance workflow:

- Melody-conditioned chord generation using a ReaLchords model path.
- Fast generation of multiple chord alternatives for one melody phrase.
- Immediate in-DAW placement of generated chord options on dedicated tracks.
- Human-in-the-loop compositional exploration of harmonic possibilities.

This focuses on expanding the creative "space of possibilities" for harmony rather than producing a single deterministic accompaniment.

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

Note: The internal CMake target name remains `Reason` for stability, but it builds `Modulo.app`.

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

## What You Need to Clone for MODULO

This repo is centered on the MODULO app path:

- Primary runtime app: `examples/Reason` (branded as Modulo at build/runtime).
- Core engine dependencies are kept in-repo because they are required to compile and run MODULO.
- Other Tracktion example targets are present, but MODULO development is concentrated in the files above.

## Repository

Target GitHub repository: [takakhoo/MODULO_AI_DAW](https://github.com/takakhoo/MODULO_AI_DAW)

