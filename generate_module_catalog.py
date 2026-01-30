#!/usr/bin/env python3
"""
Generate a machine-readable catalog of all files in the modules directory.
"""
import os
import json
from pathlib import Path

def get_file_description(filepath, filename):
    """Generate description based on file path and name"""
    path_lower = filepath.lower()
    name_lower = filename.lower()
    
    # Third party libraries
    if '3rd_party' in path_lower:
        if 'choc' in path_lower:
            return "Third-party library: Choc (audio/MIDI/utilities framework)"
        elif 'libsamplerate' in path_lower:
            return "Third-party library: libsamplerate (audio sample rate conversion)"
        elif 'magic_enum' in path_lower:
            return "Third-party library: magic_enum (enum reflection for C++)"
        elif 'doctest' in path_lower:
            return "Third-party library: doctest (testing framework)"
        elif 'rpmalloc' in path_lower:
            return "Third-party library: rpmalloc (memory allocator)"
        elif 'crill' in path_lower:
            return "Third-party library: crill (lock-free data structures)"
        elif 'expected' in path_lower:
            return "Third-party library: expected (std::expected implementation)"
        elif 'nanorange' in path_lower:
            return "Third-party library: nanorange (ranges library)"
        elif 'rigtorp' in path_lower:
            return "Third-party library: rigtorp (MPMC queue)"
        return "Third-party library file"
    
    # Tracktion Engine files
    if 'tracktion_engine' in path_lower:
        # Plugins
        if 'plugins/effects' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            effects = {
                'reverb': 'Reverb effect plugin (room size, damp, wet/dry)',
                'compressor': 'Compressor/Limiter effect plugin (threshold, ratio, attack, release)',
                'equaliser': '4-band EQ effect plugin',
                'delay': 'Delay effect plugin',
                'chorus': 'Chorus effect plugin',
                'phaser': 'Phaser effect plugin',
                'pitchshift': 'Pitch shifter effect plugin',
                'lowpass': 'Low-pass filter effect plugin',
                'impulseresponse': 'Impulse Response/Convolver effect plugin',
                'sampler': 'Sampler plugin (sample playback)',
                'fourosc': '4OSC subtractive synth plugin',
                'midimodifier': 'MIDI modifier plugin',
                'midipatchbay': 'MIDI patch bay plugin',
                'patchbay': 'Audio patch bay plugin',
                'tonegenerator': 'Tone generator plugin',
                'latency': 'Latency compensation plugin'
            }
            return effects.get(name_clean.lower(), f'Effect plugin: {name_clean}')
        
        elif 'plugins/internal' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            internals = {
                'volumeandpan': 'Volume & Pan plugin (default on tracks)',
                'vca': 'VCA (Voltage Controlled Amplifier) plugin',
                'levelmeter': 'Level meter plugin (default on tracks)',
                'auxsend': 'Aux send plugin',
                'auxreturn': 'Aux return plugin',
                'freezepoint': 'Freeze point plugin',
                'insertplugin': 'Insert plugin',
                'textplugin': 'Text annotation plugin',
                'rewireplugin': 'ReWire plugin support',
                'racktype': 'Rack type definition',
                'rackinstance': 'Rack instance plugin'
            }
            return internals.get(name_clean.lower(), f'Internal plugin: {name_clean}')
        
        elif 'plugins/external' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            externals = {
                'externalplugin': 'External plugin wrapper (VST3/AU/LADSPA)',
                'externalautomatableparameter': 'Automation parameter for external plugins',
                'externalpluginblacklist': 'Blacklist management for problematic external plugins',
                'vstxml': 'VST XML metadata handling'
            }
            return externals.get(name_clean.lower(), f'External plugin system: {name_clean}')
        
        elif 'plugins/cmajor' in path_lower:
            return 'Cmajor plugin format support'
        
        elif 'plugins/airwindows' in path_lower:
            return 'AirWindows plugin integration'
        
        elif 'plugins/ara' in path_lower:
            return 'ARA (Audio Random Access) plugin support (e.g. Melodyne)'
        
        elif 'plugins' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            core = {
                'plugin': 'Base Plugin class (all plugins inherit from this)',
                'pluginlist': 'PluginList container (manages plugins on tracks/clips)',
                'pluginmanager': 'PluginManager (registration, discovery, creation)',
                'pluginwindowstate': 'Plugin window state management',
                'pluginscanhelpers': 'Plugin scanning utilities'
            }
            return core.get(name_clean.lower(), f'Plugin system: {name_clean}')
        
        # Model
        elif 'model/tracks' in path_lower:
            return f'Track model class: {filename.replace("tracktion_", "").replace(".h", "").replace(".cpp", "")}'
        
        elif 'model/clips' in path_lower:
            return f'Clip model class: {filename.replace("tracktion_", "").replace(".h", "").replace(".cpp", "")}'
        
        elif 'model/edit' in path_lower:
            return f'Edit (project) model: {filename.replace("tracktion_", "").replace(".h", "").replace(".cpp", "")}'
        
        elif 'model/automation' in path_lower:
            if 'modifiers' in path_lower:
                name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
                mods = {
                    'lfo': 'LFO automation modifier',
                    'envelopefollower': 'Envelope follower automation modifier',
                    'breakpointoscillator': 'Breakpoint oscillator automation modifier',
                    'step': 'Step automation modifier',
                    'random': 'Random automation modifier',
                    'miditracker': 'MIDI tracker automation modifier'
                }
                return mods.get(name_clean.lower(), f'Automation modifier: {name_clean}')
            else:
                name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
                autos = {
                    'automatableparameter': 'Automatable parameter (plugin parameters)',
                    'automatableedititem': 'Base class for automatable edit items',
                    'automationcurve': 'Automation curve (bezier-based)',
                    'automationcurvelist': 'List of automation curves',
                    'automationmode': 'Automation recording/playback modes',
                    'automationrecordmanager': 'Automation recording manager',
                    'modifier': 'Base automation modifier class',
                    'macroparameter': 'Macro parameter (controls multiple parameters)',
                    'midilearn': 'MIDI learn system',
                    'parametercontrolmappings': 'Parameter control mappings',
                    'parameterchangehandler': 'Parameter change event handler'
                }
                return autos.get(name_clean.lower(), f'Automation system: {name_clean}')
        
        elif 'model/export' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            exports = {
                'renderer': 'Audio renderer (offline rendering)',
                'rendermanager': 'Render manager (background rendering jobs)',
                'renderoptions': 'Render options configuration',
                'exportjob': 'Export job definition',
                'exportable': 'Exportable interface',
                'archivefile': 'Archive file handling',
                'referencedmateriallist': 'Referenced material list for exports'
            }
            return exports.get(name_clean.lower(), f'Export/rendering: {name_clean}')
        
        # Playback
        elif 'playback/graph' in path_lower:
            return f'Audio graph node: {filename.replace("tracktion_", "").replace(".h", "").replace(".cpp", "")}'
        
        elif 'playback/devices' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            devices = {
                'inputdevice': 'Base input device class',
                'waveinputdevice': 'Audio input device',
                'midiinputdevice': 'MIDI input device',
                'physicalmidiinputdevice': 'Physical MIDI input device',
                'virtualmidiinputdevice': 'Virtual MIDI input device',
                'outputdevice': 'Base output device class',
                'waveoutputdevice': 'Audio output device',
                'midioutputdevice': 'MIDI output device',
                'wavedevicedescription': 'Audio device description'
            }
            return devices.get(name_clean.lower(), f'Device class: {name_clean}')
        
        elif 'playback' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            playback = {
                'transportcontrol': 'Transport control (play/stop/record)',
                'editplaybackcontext': 'Edit playback context',
                'editinputdevices': 'Edit input device management',
                'devicemanager': 'Audio/MIDI device manager',
                'abletonlink': 'Ableton Link synchronization',
                'levelmeasurer': 'Audio level measurement',
                'midinotedispatcher': 'MIDI note dispatcher',
                'hostedaudiodevice': 'Hosted audio device wrapper',
                'scopedsteadyload': 'Scoped steady load (performance monitoring)',
                'mpestarttrimmer': 'MPE start trimmer'
            }
            return playback.get(name_clean.lower(), f'Playback system: {name_clean}')
        
        # Audio files
        elif 'audio_files' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            audio_files = {
                'audiofile': 'Audio file wrapper',
                'audiofilemanager': 'Audio file manager (caching)',
                'audiofilecache': 'Audio file cache',
                'audioformatmanager': 'Audio format manager (WAV/AIFF/FLAC/OGG/MP3)',
                'audiofileutils': 'Audio file utilities',
                'audiofilewriter': 'Audio file writer',
                'smartthumbnail': 'Smart audio thumbnail generator',
                'recordingthumbnailmanager': 'Recording thumbnail manager',
                'bufferedaudioreader': 'Buffered audio reader',
                'bufferedfilereader': 'Buffered file reader',
                'audioproxygenerator': 'Audio proxy generator',
                'audiofifo': 'Audio FIFO buffer',
                'loopinfo': 'Audio loop information'
            }
            return audio_files.get(name_clean.lower(), f'Audio file handling: {name_clean}')
        
        # MIDI
        elif 'midi' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            midi = {
                'midichannel': 'MIDI channel',
                'midicontrollerevent': 'MIDI controller event',
                'midiexpression': 'MIDI expression',
                'activenotelist': 'Active MIDI note list'
            }
            return midi.get(name_clean.lower(), f'MIDI handling: {name_clean}')
        
        # Utilities
        elif 'utilities' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            utils = {
                'engine': 'Engine class (main entry point)',
                'enginebehaviour': 'Engine behavior customization',
                'uibehaviour': 'UI behavior customization',
                'propertystorage': 'Property storage (settings)',
                'temporaryfilemanager': 'Temporary file manager',
                'crashtracer': 'Crash tracing utilities',
                'threads': 'Thread utilities',
                'fileutilities': 'File utilities',
                'identifiers': 'ValueTree identifiers',
                'valuetreeutilities': 'ValueTree utilities',
                'audioutilities': 'Audio utilities',
                'audioscratchbuffer': 'Audio scratch buffer',
                'audiofadecurve': 'Audio fade curve',
                'parameterhelpers': 'Parameter helper functions',
                'oscillators': 'Oscillator functions',
                'envelope': 'Envelope functions',
                'pitch': 'Pitch utilities',
                'spline': 'Spline interpolation',
                'ditherer': 'Audio dithering',
                'appfunctions': 'Application helper functions',
                'plugincomponent': 'Plugin UI component',
                'curveeditor': 'Automation curve editor',
                'backgroundjobs': 'Background job system',
                'externalplayheadsynchroniser': 'External playhead synchronization',
                'sharedtimer': 'Shared timer',
                'screensaverdefeater': 'Screen saver defeater',
                'cpumeasurement': 'CPU usage measurement',
                'constrainedcachedvalue': 'Constrained cached value',
                'atomicwrapper': 'Atomic wrapper utilities',
                'asyncfunctionutils': 'Async function utilities',
                'safe scopedlistener': 'Safe scoped listener',
                'scopedlistener': 'Scoped listener',
                'mousehoverdetector': 'Mouse hover detector',
                'miscutilities': 'Miscellaneous utilities',
                'testutilities': 'Test utilities',
                'types': 'Type definitions',
                'settingid': 'Setting ID definitions',
                'binarydata': 'Binary data embedding'
            }
            return utils.get(name_clean.lower(), f'Utility: {name_clean}')
        
        # Project
        elif 'project' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            projects = {
                'project': 'Project class',
                'projectmanager': 'Project manager',
                'projectitem': 'Project item',
                'projectitemid': 'Project item ID',
                'projectsearchindex': 'Project search index'
            }
            return projects.get(name_clean.lower(), f'Project management: {name_clean}')
        
        # Selection
        elif 'selection' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            selections = {
                'selectable': 'Selectable interface',
                'selectionmanager': 'Selection manager',
                'clipboard': 'Clipboard operations',
                'selectableclass': 'Selectable class utilities'
            }
            return selections.get(name_clean.lower(), f'Selection system: {name_clean}')
        
        # Control surfaces
        elif 'control_surfaces' in path_lower:
            return f'Control surface: {filename.replace("tracktion_", "").replace(".h", "").replace(".cpp", "")}'
        
        # Time stretch
        elif 'timestretch' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            timestretch = {
                'timestretch': 'Time stretching implementation',
                'readaheadtimestretcher': 'Read-ahead time stretcher',
                'tempodetect': 'Tempo detection',
                'beatdetect': 'Beat detection'
            }
            return timestretch.get(name_clean.lower(), f'Time stretching: {name_clean}')
        
        # Testing
        elif 'testing' in path_lower:
            return f'Testing utilities: {filename.replace("tracktion_", "").replace(".h", "").replace(".cpp", "")}'
        
        # Main module files
        elif 'tracktion_engine.h' in filename:
            return 'Main Tracktion Engine header'
        elif 'tracktion_engine_' in filename and filename.endswith('.cpp'):
            return 'Tracktion Engine module implementation (compiled unit)'
        
    # Tracktion Graph
    elif 'tracktion_graph' in path_lower:
        if 'utilities' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            graph_utils = {
                'audiobufferpool': 'Audio buffer pool (memory management)',
                'audiobufferstack': 'Audio buffer stack',
                'audiofifo': 'Audio FIFO',
                'midimessagearray': 'MIDI message array',
                'midimessagewithsource': 'MIDI message with source',
                'semaphore': 'Semaphore implementation',
                'threads': 'Thread utilities',
                'realtimespinlock': 'Real-time spin lock',
                'lockfreeobject': 'Lock-free object wrapper',
                'latencyprocessor': 'Latency processor',
                'gluecode': 'Glue code utilities',
                'performancemeasurement': 'Performance measurement',
                'allocation': 'Memory allocation utilities'
            }
            return graph_utils.get(name_clean.lower(), f'Graph utility: {name_clean}')
        elif 'nodes' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            nodes = {
                'connectednode': 'Connected node (graph connections)',
                'latencynode': 'Latency compensation node',
                'summingnode': 'Summing/mixing node'
            }
            return nodes.get(name_clean.lower(), f'Graph node: {name_clean}')
        elif 'players' in path_lower:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            players = {
                'nodeplayerutilities': 'Node player utilities',
                'simplenodeplayer': 'Simple node player'
            }
            return players.get(name_clean.lower(), f'Node player: {name_clean}')
        else:
            name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
            graph_core = {
                'node': 'Base Node class (audio graph node)',
                'nodeplayer': 'Node player (executes graph)',
                'lockfreemultithreadednodeplayer': 'Lock-free multi-threaded node player',
                'multithreadednodeplayer': 'Multi-threaded node player',
                'nodeplayerthreadpools': 'Node player thread pools',
                'playhead': 'Playhead (playback position tracking)',
                'playheadstate': 'Playhead state',
                'utility': 'Graph utility functions',
                'testnodes': 'Test nodes',
                'testutilities': 'Test utilities'
            }
            return graph_core.get(name_clean.lower(), f'Graph core: {name_clean}')
    
    # Tracktion Core
    elif 'tracktion_core' in path_lower:
        name_clean = filename.replace('tracktion_', '').replace('.h', '').replace('.cpp', '')
        core = {
            'types': 'Core type definitions',
            'time': 'Time representation and utilities',
            'timerange': 'Time range class',
            'tempo': 'Tempo sequence and calculations',
            'maths': 'Math utilities',
            'hash': 'Hash functions',
            'bezier': 'Bezier curve utilities',
            'cpu': 'CPU detection',
            'benchmark': 'Benchmark utilities',
            'algorithmadapters': 'Algorithm adapters',
            'audioreader': 'Audio reader interface',
            'multiplewriterseqlock': 'Multiple writer sequence lock'
        }
        return core.get(name_clean.lower(), f'Core utility: {name_clean}')
    
    # JUCE (external framework)
    elif 'juce' in path_lower:
        return 'JUCE framework file (audio/GUI framework)'
    
    return f'Module file: {filename}'

def main():
    modules_path = Path('modules')
    files_list = []
    
    for root, dirs, files in os.walk(modules_path):
        # Skip .git and other hidden dirs
        dirs[:] = [d for d in dirs if not d.startswith('.')]
        
        for file in files:
            if file.endswith(('.h', '.cpp', '.hpp', '.c', '.cmake', '.md', '.txt', '.tcc')):
                filepath = os.path.join(root, file)
                rel_path = os.path.relpath(filepath, 'modules')
                desc = get_file_description(rel_path, file)
                
                files_list.append({
                    'path': rel_path,
                    'filename': file,
                    'extension': Path(file).suffix,
                    'directory': os.path.dirname(rel_path),
                    'description': desc,
                    'category': determine_category(rel_path)
                })
    
    # Sort by path
    files_list.sort(key=lambda x: x['path'])
    
    # Output as JSON
    output = {
        'total_files': len(files_list),
        'categories': get_category_counts(files_list),
        'files': files_list
    }
    
    print(json.dumps(output, indent=2))

def determine_category(path):
    """Determine the category for a file"""
    path_lower = path.lower()
    if '3rd_party' in path_lower:
        return 'third_party'
    elif 'tracktion_engine' in path_lower:
        if 'plugins' in path_lower:
            return 'plugins'
        elif 'model' in path_lower:
            return 'model'
        elif 'playback' in path_lower:
            return 'playback'
        elif 'audio_files' in path_lower:
            return 'audio_files'
        elif 'utilities' in path_lower:
            return 'utilities'
        elif 'midi' in path_lower:
            return 'midi'
        elif 'project' in path_lower:
            return 'project'
        elif 'selection' in path_lower:
            return 'selection'
        elif 'control_surfaces' in path_lower:
            return 'control_surfaces'
        elif 'timestretch' in path_lower:
            return 'timestretch'
        elif 'testing' in path_lower:
            return 'testing'
        return 'tracktion_engine'
    elif 'tracktion_graph' in path_lower:
        return 'tracktion_graph'
    elif 'tracktion_core' in path_lower:
        return 'tracktion_core'
    elif 'juce' in path_lower:
        return 'juce'
    return 'unknown'

def get_category_counts(files_list):
    """Get counts per category"""
    counts = {}
    for f in files_list:
        cat = f['category']
        counts[cat] = counts.get(cat, 0) + 1
    return counts

if __name__ == '__main__':
    main()
