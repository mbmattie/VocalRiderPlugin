# Vocal Rider Plugin

A real-time vocal level rider plugin that automatically adjusts gain to maintain consistent vocal levels. Built with JUCE framework.

## Features

- **RMS-based level detection** for smooth, musical gain adjustments
- **+/- 12dB gain range** for moderate dynamics control
- **Real-time gain reduction metering**
- **VST3 and AU format support**
- **macOS and Windows compatible**

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Target Level | -40 to 0 dB | -18 dB | Desired RMS output level |
| Speed | 0-100% | 50% | How fast gain adjustments occur |
| Range | 0-12 dB | 12 dB | Maximum gain adjustment |
| Mix | 0-100% | 100% | Dry/wet blend |

## Building

### Prerequisites

- **CMake** 3.22 or later
- **Xcode** (macOS) with Command Line Tools
- **Visual Studio 2022** (Windows)
- **Git**

### Build Instructions

1. Clone this repository with submodules:
   ```bash
   git clone --recursive https://github.com/yourusername/VocalRiderPlugin.git
   ```

2. If you already cloned without submodules, initialize them:
   ```bash
   git submodule update --init --recursive
   ```

3. Create a build directory and configure:
   ```bash
   mkdir build && cd build
   cmake ..
   ```

4. Build the plugin:
   ```bash
   cmake --build . --config Release
   ```

5. The built plugins will be in `build/VocalRider_artefacts/`

### macOS Quick Build

```bash
mkdir build && cd build
cmake -G Xcode ..
open VocalRider.xcodeproj
```

### Windows Quick Build

```bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
```
Then open `VocalRider.sln` in Visual Studio.

## Project Structure

```
VocalRiderPlugin/
├── CMakeLists.txt          # Build configuration
├── JUCE/                   # JUCE framework (submodule)
├── Source/
│   ├── PluginProcessor.*   # Audio processing
│   ├── PluginEditor.*      # User interface
│   ├── DSP/
│   │   ├── RMSDetector.*   # RMS level detection
│   │   └── GainSmoother.*  # Gain envelope
│   └── UI/
│       ├── LevelMeter.*    # Gain reduction meter
│       └── CustomLookAndFeel.*  # Visual styling
└── Resources/              # Images, fonts, etc.
```

## License

Copyright (c) 2026 MBM Audio. All rights reserved.


