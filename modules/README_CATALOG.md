# Modules Catalog

This directory contains a comprehensive catalog of all files in the modules directory.

## Files Generated

1. **modules_catalog.json** - Full JSON catalog with all file metadata
2. **modules_catalog.tsv** - Tab-separated values version for easy parsing

## Statistics

- **Total files**: 4,563
- **Tracktion Engine files** (excluding JUCE and 3rd party): ~470

## Categories

- **third_party** (736 files) - Third-party libraries (Choc, libsamplerate, etc.)
- **juce** (3,206 files) - JUCE framework
- **tracktion_engine** (470 files) - Tracktion Engine code
  - **plugins** (85 files) - Plugin system
  - **model** (182 files) - Data model (tracks, clips, edit, automation)
  - **playback** (124 files) - Audio playback engine
  - **utilities** (55 files) - Utility functions
  - **audio_files** (33 files) - Audio file handling
  - **tracktion_graph** (47 files) - Audio graph system
  - **control_surfaces** (26 files) - Control surface support
  - **midi** (15 files) - MIDI handling
  - **project** (10 files) - Project management
  - **selection** (8 files) - Selection system
  - **timestretch** (8 files) - Time stretching
  - **testing** (3 files) - Testing utilities
  - **tracktion_core** (19 files) - Core utilities

## Usage

To regenerate the catalog:
```bash
python3 generate_module_catalog.py > modules_catalog.json
```

To filter for Tracktion Engine files only:
```bash
python3 -c "
import json
data = json.load(open('modules_catalog.json'))
tracktion = [f for f in data['files'] if f['category'] not in ['juce', 'third_party']]
print(json.dumps({'files': tracktion}, indent=2))
" > tracktion_only.json
```

## File Format

Each entry contains:
- `path`: Relative path from modules directory
- `filename`: Just the filename
- `extension`: File extension (.h, .cpp, etc.)
- `directory`: Directory containing the file
- `category`: Category classification
- `description`: Human-readable description of what the file does
