# AetherHelm

[![CI/CD Multi-Platform Build](https://github.com/AA-EION/AetherHelm/actions/workflows/build-test-release.yml/badge.svg)](https://github.com/AA-EION/AetherHelm/actions/workflows/build-test-release.yml)
[![C++20](https://img.shields.io/badge/standard-C%2B%2B20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![JUCE](https://img.shields.io/badge/framework-JUCE%207%2F8-orange.svg)](https://juce.com/)
[![Formats](https://img.shields.io/badge/formats-VST3%20%7C%20AU%20%7C%20CLAP%20%7C%20Standalone-green.svg)]()
[![MCP](https://img.shields.io/badge/protocol-Model%20Context%20Protocol%20(MCP)-purple.svg)](https://modelcontextprotocol.io/)

**AetherHelm** is a next-generation, AI-driven modular polyphonic synthesizer. Built upon the foundation of Matt Tytel's acclaimed open-source synthesizer Helm, AetherHelm transforms classic subtractive and wavetable synthesis with modern C++20 architecture, idiomatic JUCE CMake support, bidirectional JSON preset schemas, direct OpenRouter AI patch generation, and Model Context Protocol (MCP) server integration.

---

## Key Highlights

- **Modern C++20 Engine**: Clean, performant, and thread-safe audio engine overhauled for C++20 standard compliance.
- **Idiomatic CMake & JUCE Integration**: Replaced legacy project generators with standard `juce_add_plugin` / `juce_generate_juce_header` CMake targets supporting VST3, AU, CLAP, and Standalone.
- **Strict Real-Time Safety**: Zero allocations or locking on the audio callback thread; lock-free communication between GUI, network, and audio engine.
- **Bidirectional JSON Patch Architecture**: Standardized, human-readable, and AI-friendly JSON schema supporting granular serialization, parameter clamping, and seamless import/export.
- **In-App AI Agent (OpenRouter)**: Integrated background client connecting to state-of-the-art LLMs (Claude 3.5 Sonnet, GPT-4o, DeepSeek Coder) for natural language sound design and immediate patch injection.
- **Model Context Protocol (MCP) Server**: Headless MCP server (`aetherhelm-mcp`) enabling direct synthesis control, patch queries, parameter auditioning, and MIDI previewing from Claude Desktop, Cursor, Windsurf, and Cline.
- **Multi-Platform CI/CD**: Automated GitHub Actions matrix generating native builds for Windows (MSVC), macOS (Universal Binary x86_64 + arm64), and Linux.

---

## Synthesis Engine Features

- **Polyphony**: 32-voice true polyphony with smooth voice allocation and unison detune.
- **Oscillators**: Dual primary oscillators with 12 morphing waveforms, cross-modulation, up to 15 unison voices, sub-oscillator with shuffle waveshaping, and feedback loops.
- **Multi-Mode Filter**: 12/24 dB state-variable low-pass, band-pass, high-pass, shelf, formant filter, and analog-style drive.
- **Modulation System**: Dual monophonic LFOs, polyphonic LFO, 32-step sequencer, dual envelopes (amp, filter, mod), and dynamic modulation matrix with live visual feedback.
- **Effects Chain**: Onboard distortion, tempo-synced delay, lush algorithmic reverb, and dynamic stutter effects.

---

## Building AetherHelm

### Prerequisites
- **CMake**: Version 3.22 or newer
- **C++20 Compiler**:
  - Windows: MSVC (Visual Studio 2022)
  - macOS: Xcode 14+ or Clang with Universal Binary support
  - Linux: GCC 11+ or Clang 13+ with audio development libraries (`libasound2-dev`, `libjack-jackd2-dev`, `libfreetype6-dev`, `libx11-dev`, `libgl1-mesa-dev`)

### CMake Build Instructions

```bash
# Clone the repository
git clone https://github.com/AA-EION/AetherHelm.git
cd AetherHelm

# Configure project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build plugin formats and MCP server
cmake --build build --config Release --parallel
```

Build outputs will be generated in `build/AetherHelm_artefacts/Release/` (VST3, AU, Standalone) and `build/aetherhelm-mcp`.

---

## AI Sound Design with OpenRouter

AetherHelm features a dedicated AI generation interface allowing you to describe sound designs in natural language:

> *"Design an aggressive cyberpunk bass with detuned saw waves, a biting comb filter, and an LFO modulating the cutoff tempo-synced at 1/8d"*

### Configuration
1. Open AetherHelm (Standalone or DAW plugin).
2. Open the **AI Synthesis** bar.
3. Enter your **OpenRouter API Key** (or export `OPENROUTER_API_KEY` in your environment).
4. Select your preferred model (default: `anthropic/claude-3.5-sonnet`).
5. Type your sound description and click **Generate**. The synth will asynchronously generate the patch, validate bounds, and inject the sound in real time without audio glitches.

---

## Model Context Protocol (MCP) Server

AetherHelm includes a standalone MCP server (`aetherhelm-mcp`) enabling LLMs in desktop environments to act as an automated sound designer.

### Supported MCP Tools
- `get_current_patch`: Inspects current synthesizer state, oscillator parameters, filter cutoffs, and active modulation matrix.
- `set_patch_parameters`: Modifies specific parameters or injects full patches dynamically.
- `list_available_parameters`: Lists all addressable synth parameters with ranges, defaults, and descriptions.
- `trigger_preview_note`: Renders and previews MIDI audition notes programmatically.
- `generate_patch_from_prompt`: Generates a complete preset from a natural language prompt via OpenRouter.

### Auto-Configuration for Desktop AI

Run the automated installer script to register `aetherhelm-mcp` with your desktop AI clients:

**Windows (PowerShell):**
```powershell
.\scripts\install-mcp.ps1
```

**macOS / Linux (Bash):**
```bash
./scripts/install-mcp.sh
```

Or configure manually in `claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "aetherhelm": {
      "command": "/path/to/aetherhelm-mcp",
      "args": []
    }
  }
}
```

---

## License

AetherHelm is licensed under the GNU General Public License v3.0 (GPLv3). Original synthesizer engine (Helm) Copyright (c) Matt Tytel. Modernization, AI engine, and MCP extensions Copyright (c) AA-EION.
