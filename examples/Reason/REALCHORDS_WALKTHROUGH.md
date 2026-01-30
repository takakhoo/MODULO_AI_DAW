# ReaLchords Deep Walkthrough — Get 5 Chord Options into Reason

This document walks through the [lukewys/realchords-pytorch](https://github.com/lukewys/realchords-pytorch) repo step-by-step so we can generate **5 different chord options** for a given melody and wire that into the Reason DAW.

---

## 1. What the repo gives you

| Component | Purpose |
|-----------|--------|
| **ReaLchords** | Online chord accompaniment model (melody in → chords out). Trained with MLE + RL (reward models + distillation from offline teacher). |
| **ReaLJam** | Real-time jamming server + web UI. You play melody (MIDI or keyboard); model returns chords. Pretrained checkpoints auto-download. |
| **GAPT** | Variant with Generative Adversarial Post-Training (mitigates reward hacking). |
| **Datasets** | Hooktheory, POP909, Nottingham, Wikifonia — only needed if you **train** models. For inference, use pretrained checkpoints. |

**For Reason we need:**  
- **Inference only** (no training): run ReaLJam server or load a checkpoint and run inference in a small script.  
- **Output:** 5 distinct chord progressions for one input melody (via sampling).

---

## 2. Step-by-step: Fast path (run ReaLJam, no clone)

### 2.1 Environment

**ReaLJam requires Python ≥ 3.10.** If your system `python3` is 3.9 or lower, install 3.10+ (e.g. `brew install python@3.12` and use `python3.12`, or use pyenv) before creating the venv.

```bash
# Use Python >= 3.10
python3 --version   # or python3.12 --version

# Optional: dedicated venv (recommended)
cd /Users/takakhoo/Dev/tracktion/tracktion_engine
python3 -m venv .venv_realchords   # or: python3.12 -m venv .venv_realchords
source .venv_realchords/bin/activate   # macOS

pip install --upgrade pip
pip install realjam
```

### 2.2 Start the server

```bash
realjam-start-server
```

- First run downloads pretrained checkpoints to `$HOME/.realjam/checkpoints/`.
- Server listens on `http://127.0.0.1:8080` (default).
- Open that URL in a browser: you get the ReaLJam UI (MIDI or keyboard input → live chord accompaniment).

### 2.3 Optional: ONNX for speed

```bash
realjam-start-server --port 8080 --onnx
```

- Uses ONNX for faster inference. Use `realjam-start-server -h` for `onnx_provider` (e.g. CUDA).

**At this point you have:** a working chord-accompaniment backend. It is built for **streaming** (real-time jamming), not yet “give me 5 options for this melody.” That comes in Phase 3.

---

## 3. Step-by-step: Full repo (for “5 options” and custom inference)

### 3.1 Clone and install

```bash
cd /Users/takakhoo/Dev/tracktion/tracktion_engine  # or a sibling dir, e.g. ../reason_ml
git clone https://github.com/lukewys/realchords-pytorch.git
cd realchords-pytorch
pip install -e .
```

### 3.2 Repo layout (what matters for us)

| Path | Use |
|------|-----|
| `realchords/` | Core package (models, dataset loaders, utils). |
| `realchords/realjam/` | ReaLJam server and real-time logic. |
| `scripts/` | Training and conversion (e.g. `generate_checkpoint_for_realjam.py`). |
| `configs/` | YAML configs for training and RL. |
| **Checkpoints** | ReaLJam uses `$HOME/.realjam/checkpoints/`. Full training checkpoints are on Hugging Face (see README link). |

### 3.3 Start server from repo (alternative to `realjam-start-server`)

```bash
# From realchords-pytorch/
python -m realchords.realjam.server --port 8080
# Optional: --ssl for HTTPS
```

Same behavior as `realjam-start-server`; useful if you are editing the server or adding an “offline 5 options” endpoint.

---

## 4. Data format (so we can send melody from Reason)

From the paper and repo:

- **Time:** Quantized to **sixteenth-note frames**. 4 frames = 1 beat (quarter note).
- **Melody (input):** Each frame is one of:
  - **Note-on:** MIDI pitch 0–127.
  - **Note-hold:** Same pitch, continuation of previous note.
  - **Silence:** No note.
- **Chords (output):** Each frame is one of:
  - **Chord onset:** Symbol from vocabulary (e.g. "C", "Am", "Fmaj7").
  - **Chord-hold:** Previous chord continues.
  - **Silence.**

Max sequence length in training is **256 frames** (64 beats).  
Chord vocabulary: e.g. Hooktheory-style names; in the repo, `data/cache/chord_names.json` (or augmented) when using the dataset pipeline.

**For Reason:**  
- Export the selected melody (e.g. from a MidiClip) as a sequence of (time, pitch) events.  
- Quantize to sixteenth notes and convert to frame tokens (note-on / hold / silence).  
- Send that to the backend (see Phase 4 in TIMELINE).

---

## 5. How to get 5 different chord options

ReaLchords is **autoregressive**: at each frame it predicts the next chord given past melody and chords. To get **5 different** progressions for the **same** melody:

1. **Run inference 5 times** with **sampling** (do not use argmax only).
2. Use **temperature** (e.g. 0.8–1.0) and/or **nucleus (top_p)** so each run can produce a different sequence.
3. Optionally **top_k** to avoid very low-probability tokens.

**Where to implement:**

- **If ReaLJam server exposes an “offline” or “batch” API** that accepts a full melody and returns one chord sequence: call it 5 times with different random seeds or temperature (if the API allows).  
- **If it only does real-time streaming:** add a small **offline inference script** in the repo that:
  1. Loads the pretrained decoder-only chord model (same one ReaLJam uses).
  2. Loads melody tokens (from file or stdin).
  3. Runs 5 autoregressive samples with temperature/top_p.
  4. Writes 5 chord progressions (e.g. JSON) to stdout or a file.

**Next step:** Inspect `realchords/realjam/server` (and any inference helpers in `realchords/`) to see how the model is loaded and how one chord sequence is generated. Then add a function or script that returns 5 sequences.

---

## 6. Checkpoints (no training)

- **ReaLJam:** Pretrained checkpoints download automatically to `$HOME/.realjam/checkpoints/` when you run the server. No Hugging Face login required for this path.
- **Full training checkpoints:** The README says: “All trained model checkpoints … can be downloaded from the huggingface page.” Follow the link in the repo README for MLE, reward, and RL checkpoints if you need a specific variant (e.g. GAPT).
- **MLE → ReaLJam format:** If you have only an MLE checkpoint (e.g. `logs/decoder_only_online_chord/step=11000.ckpt`), convert it for ReaLJam:

```bash
python scripts/generate_checkpoint_for_realjam.py \
  --checkpoint_path logs/decoder_only_online_chord/step=11000.ckpt \
  --save_dir checkpoints
```

RL checkpoints can be used directly with ReaLJam.

---

## 7. Datasets (only if you train)

You **do not** need datasets for “5 chord options” in Reason; only for training or fine-tuning.

If you ever train:

- **Hooktheory:** ~19K songs.  
  `python scripts/download_hooktheory_dataset.py --verify` then `convert_hooktheory_to_cache.py` (+ `--augmentation` for transposition).
- **POP909, Nottingham, Wikifonia:** Similar pattern (download → convert → optional augmentation).  
- Cache format: `data/cache/<name>/*.jsonl` (train/valid/test 80/10/10).  
- Chord names: `data/cache/chord_names.json` and `chord_names_augmented.json`.

ReaLchords (paper) uses Hooktheory; GAPT uses Hooktheory + POP909 + Nottingham.

---

## 8. Training (skip for now)

Training is only needed if you want to reproduce or modify the model. It requires:

- MLE pre-training (decoder-only and/or encoder-decoder).
- Reward model training (contrastive + discriminative, possibly multi-scale).
- RL training (with optional GAPT).

Hardware: e.g. 48GB VRAM per GPU. We do **not** need this for integrating “5 chord options” into Reason; pretrained weights are enough.

---

## 9. Wiring Reason (C++) to ReaLchords (Python)

- **Reason** is C++/JUCE/Tracktion; **ReaLchords** is Python/PyTorch. They communicate via:
  - **Option A (recommended):** HTTP. Run ReaLJam (or a thin Flask/FastAPI app that wraps the “5 options” inference) on localhost. Reason sends POST with melody (e.g. JSON frame tokens), receives JSON with 5 chord progressions.
  - **Option B:** Subprocess. Reason runs a Python script; melody in (stdin or temp file), 5 options out (stdout or temp file). Simpler but less flexible.

**Concrete steps:**

1. Implement or use an HTTP endpoint: **input** = melody (frame-based or MIDI-like JSON), **output** = 5 chord progressions (e.g. list of chord symbols per frame or per beat).
2. In Reason, when the user asks for “chord options” for the current selection:
   - Export melody from the Edit (selected clip/region) to the same input format.
   - POST to `http://127.0.0.1:8080/...` (or your custom port/path).
   - Parse JSON response and show 5 options in the UI.
3. On “Apply,” write the chosen progression into the Edit (e.g. new MidiClip or chord track) using Tracktion APIs.

---

## 10. Order of operations (summary)

| Order | Task | Status |
|-------|------|--------|
| 1 | Install `realjam` and run `realjam-start-server`; confirm UI in browser | Do first |
| 2 | (Optional) Clone `realchords-pytorch`, `pip install -e .`, run server from repo | If you want to add “5 options” logic |
| 3 | Inspect server/inference code; implement “5 chord options” (script or new endpoint) | Next |
| 4 | Define melody JSON format (Reason → backend) and chord JSON format (backend → Reason) | With step 3 |
| 5 | In Reason: export melody, POST to backend, parse 5 options, show in UI | After 3–4 |
| 6 | “Apply” chosen option to Edit (chord track or MIDI) | After 5 |

---

## 11. Citations (for thesis / docs)

```bibtex
@inproceedings{wu2024adaptive,
  title={Adaptive accompaniment with ReaLchords},
  author={Wu, Yusong and Cooijmans, Tim and Kastner, Kyle and Roberts, Adam and Simon, Ian and Scarlatos, Alexander and Donahue, Chris and Tarakajian, Cassie and Omidshafiei, Shayegan and Courville, Aaron and others},
  booktitle={Forty-first International Conference on Machine Learning},
  year={2024},
}

@inproceedings{scarlatos2025realjam,
  title={ReaLJam: Real-Time Human-AI Music Jamming with Reinforcement Learning-Tuned Transformers},
  author={Scarlatos, Alexander and Wu, Yusong and Simon, Ian and Roberts, Adam and Cooijmans, Tim and Jaques, Natasha and Tarakajian, Cassie and Huang, Anna},
  booktitle={Proceedings of the Extended Abstracts of the CHI Conference on Human Factors in Computing Systems},
  pages={1--9},
  year={2025}
}
```

---

You can start **right now** with:

```bash
python3 -m venv .venv_realchords && source .venv_realchords/bin/activate
pip install realjam
realjam-start-server
```

Then open `http://127.0.0.1:8080` and confirm chords are generated in response to melody input. After that, we add the “5 options” path and the Reason ↔ backend wiring as above.
