# CompSat – Compressor + Saturator (VST3)

One plugin, two stages: a soft-knee compressor and an oversampled saturator.
You can swap the order (Comp > Sat or Sat > Comp).

## Controls

**Compressor**: Threshold, Ratio, Attack, Release, Knee, Makeup, Mix (parallel compression)
**Saturation**: Drive, Tone (+/-6 dB high shelf at 3 kHz), Mix, Type (Tape / Tube / Soft Clip)
**Global**: Order, Output. The GR meter at the top shows gain reduction.

Saturator runs 4x oversampled to keep aliasing down. It adds a few samples of
latency, which the plugin reports to your DAW so everything stays in time.

## Build it (one time setup)

You need: **CMake 3.22+**, **Git**, and a C++ compiler.

- **Windows**: install Visual Studio 2022 Community ("Desktop development with C++") + CMake + Git
- **macOS**: `xcode-select --install`, then `brew install cmake`
- **Linux**: `sudo apt install build-essential cmake git libasound2-dev libfreetype-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev`

Then, from this folder:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The first run downloads JUCE (a few minutes). When it finishes, the VST3 is copied
automatically to your system VST3 folder:

- Windows: `C:\Program Files\Common Files\VST3`
- macOS: `~/Library/Audio/Plug-Ins/VST3`
- Linux: `~/.vst3`

(If Windows refuses to copy because of permissions, run the terminal as Administrator,
or grab the file from `build\CompSat_artefacts\Release\VST3\`.)

Rescan plugins in your DAW. A **Standalone** app is built too, in
`build/CompSat_artefacts/Release/Standalone/`, handy for quick testing.

## Customising

- Change `COMPANY_NAME` / `PLUGIN_MANUFACTURER_CODE` in `CMakeLists.txt` to your own.
- DSP lives in `Source/PluginProcessor.cpp` (`runCompressor`, `shape`, `runSaturator`).
