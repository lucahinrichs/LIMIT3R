# LIMIT3R - 3-Band Limiter Plugin

A custom C++ / JUCE audio effect plugin implementing a lookahead limiter that works on three frequency bands independently.

> **Status:** Learning Project - Work in Progress

---

## Key Features

* **3-Band Split:** Input is split into low, mid and high bands using single-pole filters (crossovers at 267 Hz and 1500 Hz), computed per channel.
* **Lookahead Delay Line:** Circular buffer that delays the audio by 3 ms, so the level detectors can react before a peak reaches the output. The latency is reported to the host via `setLatencySamples()`.
* **Sample-Rate Independent:** All times and frequencies are defined in Hz / ms and converted to the current sample rate in `prepareToPlay()`.

---

## How It Works

Planned signal flow per channel:

```text
input ──┬──────────────► level detectors (undelayed) ──► gain per band
        │                                                    │
        └─► lookahead delay ─► band split (low/mid/high) ─► × gain ─► sum ─► output
```

The detectors measure the undelayed signal while the audio path is delayed. This way the gain reduction is already in place when a peak arrives at the output.

---

## Tech Stack & Architecture

* **Language:** C++ (C++20)
* **Framework:** JUCE
* **Build System:** Projucer (`.jucer`)
* **Plugin Formats:** VST3, AU, Standalone
* **Tested on:** macOS (deployment target 14.0). JUCE is cross-platform, but other platforms are untested.

---

## Project Structure

```text
LIMIT3R/
├── Source/
│   ├── PluginProcessor.h / .cpp   # Filters, delay line and audio processing
│   └── PluginEditor.h / .cpp      # GUI (placeholder for now)
└── Limit3r.jucer                  # Projucer configuration file
```

`Builds/` and `JuceLibraryCode/` are not part of the repository, Projucer generates them on export.

---

## Building the Plugin

> **Note:** The plugin is not in working condition yet. It currently only splits the signal into bands and delays it, there is no limiting yet. A first testable version is planned within the next few days.

**Requirements:** [JUCE](https://juce.com) and Xcode. The module paths in the `.jucer` file point to the author's machine, so set your own JUCE path in Projucer when prompted.

1. Clone the repository:
   ```bash
   git clone https://github.com/lucahinrichs/LIMIT3R.git
   ```
2. Open `Limit3r.jucer` in **Projucer** and export to Xcode.
3. Build the **Standalone** target for quick testing (use headphones with a live input), or the **VST3 / AU** targets to load the plugin in a DAW.

---

## Roadmap / Planned Features

- [x] Single-pole filter building block
- [x] 3-band split per channel
- [x] Lookahead delay line with latency reporting
- [ ] Per-band level detectors with individual smoothing times
- [ ] Gain computation and makeup gain
- [ ] Noise gate for very quiet signals
- [ ] Parameters (`AudioProcessorValueTreeState`) for the per-band limiter amount
- [ ] GUI with controls for the three bands
- [ ] Adjustable crossover frequencies
- [ ] Selectable detector modes

---

## License

TBD (JUCE is dual-licensed under AGPLv3 / commercial, the license of this repository will follow accordingly).
