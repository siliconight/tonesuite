# Toney

A JUCE-based **AU / VST3 / Standalone** mastering chain plugin: passive program
EQ + variable-mu compressor + dynamic EQ + tape stage + dynamic resonance
suppressor, driven from a single interactive spectrum display.

## What it does

A single-canvas mastering processor with five stages in series. Default chain:

```
[ Program EQ  <->  Vari-Mu Comp ]  ->  Dynamic EQ  ->  Tape  ->  Resonance Suppressor  ->  Output
```

The EQ and compressor pair can be swapped with the routing arrow in the
header. Every stage can be bypassed independently via its chain pill.

### Program EQ
A four-band passive-style topology: low-frequency boost shelf and cut shelf
that share a stepped frequency selector (20 / 30 / 60 / 100 Hz), high-frequency
peak with adjustable bandwidth (3 / 4 / 5 / 8 / 10 / 12 / 16 kHz), and a
high-frequency cut shelf (5 / 10 / 20 kHz). Followed by a tube-style soft
saturation drive stage.

### Variable-mu compressor
Stereo-linked, peak-detected, with input gain that drives both the signal and
the detector. The "ratio" rises with level rather than being a fixed knob -
soft onset, grabby when pushed. Six time-constant positions; positions 5 and 6
have program-dependent release (fast initial release, slow tail after sustained
gain reduction).

### Dynamic EQ
Three fully-independent bands. Each has its own bandpass sidechain (the band
only reacts to its own frequency content), with target gain, threshold,
attack, release. Positive gain = upward dynamic, negative = downward.

### Tape
Drive into asymmetric tanh saturation, low-frequency head bump, high-frequency
rolloff, output trim. Three speed presets retune the bump and rolloff.

### Dynamic resonance suppressor
24 narrow bandpass filters log-spaced 200 Hz - 14 kHz. Each band's envelope is
compared against the spatial average of its four nearest neighbors; bands
sticking up above the neighborhood by more than `Threshold` dB are attenuated
up to `Depth` dB. Off by default - destructive opt-in processing.

## Interface

- **Header.** Five chain pills + routing arrow + global output + master
  bypass. Click any pill to toggle that stage. Click the arrow to flip EQ/Comp
  routing direction.
- **Spectrum view.** Log-frequency horizontal axis (20 Hz - 20 kHz), dB
  vertical axis. Live FFT of the plugin output drawn translucent behind a
  cream EQ-response curve.
- **Draggable handles.** Each EQ band - four static, three dynamic - is a dot
  on the curve.
  - Drag vertical = gain. Drag horizontal = frequency. Mouse wheel = Q (where
    applicable).
  - The program EQ handles snap to the stepped frequency selectors as you drag.
  - Dynamic-band handles have a halo whose radius and alpha track live
    activity in real time.
- **Module grid.** 2x2 below the spectrum: Dynamics inspector | Compressor |
  Tape | Resonance suppressor.

## Build & install for Logic Pro

### Prerequisites

- macOS 11 (Big Sur) or newer
- Xcode 14+ with command-line tools: `xcode-select --install`
- CMake 3.22+: `brew install cmake`

### Build

```bash
git clone https://github.com/<you>/<this-repo>.git Toney
cd Toney
bash build.sh
```

That script:

1. Configures a universal-binary (Apple Silicon + Intel) Xcode project
2. Builds Release
3. Copies the AU to `~/Library/Audio/Plug-Ins/Components/Toney.component`
   and the VST3 to `~/Library/Audio/Plug-Ins/VST3/Toney.vst3`
4. Runs `auval -v aufx Pteq Yorc` (same validation Logic uses internally)

### Open it in Logic

1. Open Logic Pro
2. **Logic Pro -> Settings -> Plug-In Manager**
3. Find **Toney** in the list. If it's red, click **Reset & Rescan
   Selection**.
4. Close the manager. The plugin is now under **Audio FX -> YourCompany ->
   Toney** on any channel.

If Logic doesn't see it at all, clear the AU cache and force a rescan:

```bash
rm -rf ~/Library/Caches/AudioUnitCache
killall -9 AudioComponentRegistrar 2>/dev/null
```

then reopen Logic.

### Renaming

The 4-character codes `Pteq` / `Yorc` are placeholders that need to be
unique across every AU on your machine. If `auval` ever fails with a
collision, edit `CMakeLists.txt` and pick different ones:

```cmake
COMPANY_NAME             "YourActualName"
BUNDLE_ID                "com.yourname.yourpluginname"
PLUGIN_MANUFACTURER_CODE Abcd     # any unique 4-char, >=1 uppercase
PLUGIN_CODE              Wxyz     # any unique 4-char, >=1 uppercase
PRODUCT_NAME             "Your Plugin Name"
```

Codes must be unique across all AUs on the user's machine - if two plugins
share a code, Logic will reject one of them.

### CI builds via GitHub Actions

`.github/workflows/build.yml` builds a universal-binary AU + VST3 on every
push and on any tag matching `v*`. It validates the AU with `auval` and
uploads the bundles as a `Toney-macOS.zip` workflow artifact. Tag pushes
also create a GitHub Release with the zip attached.

## Implementation notes

- **Oversampling.** The EQ tube saturation and the tape saturation each run
  inside their own `juce::dsp::Oversampling` (2x FIR halfband, linear phase,
  `useIntegerLatency=true`). Oversamplers run unconditionally so reported
  latency stays constant whether stages are bypassed or driven. Linear
  filters run at native rate. Total latency is ~18 samples at 44.1 kHz
  (~0.4 ms), reported via `setLatencySamples`.
- **Dynamic EQ topology.** Each band uses `output = input + (G - 1) *
  activeAmount * BP(input)` - parallel BP-and-add. Lets the gain modulate at
  audio rate without re-cooking biquad coefficients. Good approximation of a
  peaking biquad for moderate gain/Q; diverges at very deep cuts with very
  narrow Q.
- **Resonance suppressor topology.** Same parallel BP-and-add but with
  negative gain, applied across 24 fixed bandpass filters whose envelopes
  are compared spatially (band vs neighborhood) each sample.
- **FFT analyzer.** Single-producer / single-consumer ring buffer. Audio
  thread writes mono-summed samples; UI thread (30 Hz timer) reads the
  latest 2048 samples, windows, FFTs, and updates smoothed dB magnitudes
  with fast-rise / slow-fall ballistics.
- **No malware/anti-features.** Standalone build, no telemetry, no network
  calls (`JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`).

## Files

```
Toney/
├── CMakeLists.txt              # universal binary, macOS settings, ad-hoc signing
├── build.sh                    # one-command local build + install + auval
├── README.md                   # this file
├── LICENSE                     # MIT
├── .gitignore
├── .github/workflows/build.yml # CI: universal-binary AU/VST3 + auval + zip
└── Source/
    ├── PluginProcessor.h / .cpp    # routing, parameters, signal chain
    ├── PluginEditor.h   / .cpp     # main editor, chain pills, modules
    ├── SpectrumView.h   / .cpp     # interactive spectrum + draggable handles
    ├── Analyzer.h                  # FFT analyzer (SPSC ring buffer)
    ├── VariableMuComp.h            # variable-mu compressor DSP
    ├── DynamicEQ.h                 # parallel BP-and-add dynamic band
    ├── TapeStage.h                 # saturation (oversampled) + filters
    └── ResonanceSuppressor.h       # 24-band spatial resonance suppressor
```

## License

MIT - see `LICENSE`.

## Trademarks & disclaimers

This plugin is an **original implementation** of well-known audio-processor
**topologies and circuit categories**, including passive program EQs,
variable-mu compressors, magnetic-tape saturation, and dynamic / multiband
resonance processing. These topologies and the underlying DSP concepts are
described in publicly available technical literature (textbooks, the AES
Journal, academic theses, vendor application notes) and are not themselves
protected by trademark or patent.

This plugin is **not affiliated with, endorsed by, or sponsored by** any
manufacturer of hardware or software audio equipment. No brand names, model
numbers, or product names of third-party manufacturers are used to identify
this plugin or any of its modules. Any resemblance in sonic character to a
particular hardware unit is the result of implementing the same broad
DSP/circuit category, not a claim of equivalence to or compatibility with
any specific product.

Logic Pro is a trademark of Apple Inc., used here in a purely descriptive
sense to indicate compatibility ("the plugin can be loaded in Logic Pro").

**Before publishing or distributing this plugin commercially you should
have a lawyer in your jurisdiction review your final naming, marketing
copy, GUI text, and product graphics.** The notes above are not legal
advice; they reflect a reasonable engineering precaution (use generic
topology names, avoid brand references, claim no affiliation), but
specific risks depend on jurisdiction, marketing claims, and the precise
choices you make.
