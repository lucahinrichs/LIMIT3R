# LIMIT3R - 3-Band Limiter Plugin

A custom C++ / JUCE audio effect plugin implementing a lookahead limiter with three frequency bands, each with its own gain stage and smoothing time.

> **Status:** Learning Project - Work in Progress

---

<img width="594" height="350" alt="LIMIT3R_GUI_1 0" src="https://github.com/user-attachments/assets/ea374d07-69ea-4731-b039-a5e9411d8ec2" />


## Key Features

* **3-Band Split:** Single-pole crossovers at 267 Hz and 1500 Hz, computed per channel.
* **Lookahead:** 3 ms delay line with latency reported to the host via `setLatencySamples()`.
* **Per-Band Gain Stage:** Own envelope smoothing and gain computation for each band.
* **Makeup Gain and Noise Gate:** Fixed makeup plus an output fader for level-matched A/B comparison, and a gate for near-silent signals.
* **Parameters:** One limiting fader per band and a makeup fader via `AudioProcessorValueTreeState`.
* **Custom GUI:** Gradient background, faders, level meters and gain reduction bars.
* **Sample-Rate Independent:** Times and frequencies are converted to the current sample rate in `prepareToPlay()`.

---

## How It Works

Signal flow per channel:

```text
input ──┬──────────────► level detectors (undelayed) ──► gain per band
        │                                                    │
        └─► lookahead delay ─► band split (low/mid/high) ─► × gain ─► sum ─► makeup ─► output
```

The detectors measure the undelayed signal while the audio path is delayed. This way the gain reduction is already in place when a peak arrives at the output.

All three detectors currently listen to the full-band input signal, so this is a frequency-dependent limiter with a shared detector rather than a classic multiband limiter with fully separate detection. A per-band detection mode is planned (see roadmap).

---

## Tech Stack & Architecture

* **Language:** C++ (C++20)
* **Framework:** JUCE
* **Build System:** Projucer (`.jucer`)
* **Plugin Formats:** VST3, AU, Standalone
* **Tested on:** macOS (deployment target 14.0) as AU and VST3 in a DAW. The Standalone target is untested. JUCE is cross-platform, but other platforms are untested.

---

## Project Structure

```text
LIMIT3R/
├── Source/
│   ├── PluginProcessor.h / .cpp   # Filters, delay line, detectors and audio processing
│   └── PluginEditor.h / .cpp      # GUI: faders, level meters, gain reduction bars
└── Limit3r.jucer                  # Projucer configuration file
```

`Builds/` and `JuceLibraryCode/` are not part of the repository, Projucer generates them on export.

---

## Building the Plugin

> **Note:** The plugin builds and processes audio, but it is still a work in progress. Parameters are not saved with DAW projects yet.

**Requirements:** [JUCE](https://juce.com) and Xcode. The module paths in the `.jucer` file point to the author's machine, so set your own JUCE path in Projucer when prompted.

1. Clone the repository:
   ```bash
   git clone https://github.com/lucahinrichs/LIMIT3R.git
   ```
2. Open `Limit3r.jucer` in **Projucer** and export to Xcode.
3. Build the **VST3 / AU** targets and load the plugin in a DAW. The **Standalone** target is also available, but untested (use headphones with a live input).

---

## Roadmap / Planned Features

- [x] Single-pole filter building block
- [x] 3-band split per channel
- [x] Lookahead delay line with latency reporting
- [x] Per-band level detectors with individual smoothing times
- [x] Gain computation and makeup gain
- [x] Noise gate for very quiet signals
- [x] Parameters (`AudioProcessorValueTreeState`) for the per-band limiter amount
- [x] GUI with faders, level meters and gain reduction bars
- [ ] Save and restore parameters with the DAW project
- [ ] Parameter smoothing
- [ ] Adjustable crossover frequencies
- [ ] Selectable detector modes (full-band or per band)
- [ ] Custom look and feel
