#include <cassert>
#include <iostream>
#include <cmath>
#include <thread>
#include "JuceHeader.h"
#include "helm_common.h"
#include "helm_engine.h"
#include "synth_base.h"
#include "aether_patch_serializer.h"
#include "aetherhelm_mcp_server.h"

// Headless test harness
class DummySynth : public SynthBase {
public:
  DummySynth() {
    engine_.init();
    engine_.setSampleRate(44100);
    engine_.setBufferSize(256);
    loadInitPatch();
  }

  const CriticalSection& getCriticalSection() override { return lock_; }
  SynthGuiInterface* getGuiInterface() override { return nullptr; }

private:
  CriticalSection lock_;
};

void testParameterClamping() {
  std::cout << "[RUN] testParameterClamping..." << std::endl;
  DummySynth synth;

  // Cutoff range is [28.0, 127.0]
  // Resonance range is [0.0, 1.0]
  std::string maliciousJson = R"({
    "settings": {
      "cutoff": 500.0,
      "resonance": -10.0,
      "volume": 99.0
    }
  })";

  std::string err;
  bool ok = synth.loadPatchFromJson(maliciousJson, &err);
  assert(ok);

  mopo::control_map controls = synth.getControls();
  float cutoffVal = controls["cutoff"]->value();
  float resVal = controls["resonance"]->value();
  float volVal = controls["volume"]->value();

  // Verify EXACT clamp bounds are enforced and updated immediately in controls
  assert(std::abs(cutoffVal - 127.0f) < 0.001f);
  assert(std::abs(resVal - 0.0f) < 0.001f);
  assert(std::abs(volVal - 1.4143f) < 0.001f);

  std::cout << "  -> Cutoff clamped exactly to: " << cutoffVal << " (max: 127.0)" << std::endl;
  std::cout << "  -> Resonance clamped exactly to: " << resVal << " (min: 0.0)" << std::endl;
  std::cout << "  -> Volume clamped exactly to: " << volVal << " (max: 1.4143)" << std::endl;
  std::cout << "[PASS] testParameterClamping" << std::endl;
}

void testHierarchicalSchema() {
  std::cout << "[RUN] testHierarchicalSchema..." << std::endl;
  DummySynth synth;

  std::string hierarchicalJson = R"({
    "patch_name": "Ambient Drone",
    "author": "AI Sound Designer",
    "oscillators": {
      "osc_1": {
        "volume": 0.85,
        "transpose": -12.0
      },
      "cross_modulation": 0.25
    },
    "filter": {
      "cutoff": 62.5,
      "resonance": 0.45,
      "style": 1.0
    },
    "effects": {
      "reverb": {
        "feedback": 0.95
      }
    }
  })";

  std::string err;
  bool ok = synth.loadPatchFromJson(hierarchicalJson, &err);
  assert(ok);

  mopo::control_map controls = synth.getControls();
  assert(synth.getPatchName() == "Ambient Drone");
  assert(synth.getAuthor() == "AI Sound Designer");
  assert(std::abs(controls["osc_1_volume"]->value() - 0.85f) < 0.01f);
  assert(std::abs(controls["osc_1_transpose"]->value() - (-12.0f)) < 0.01f);
  assert(std::abs(controls["cross_modulation"]->value() - 0.25f) < 0.01f);
  assert(std::abs(controls["cutoff"]->value() - 62.5f) < 0.01f);
  assert(std::abs(controls["resonance"]->value() - 0.45f) < 0.01f);
  assert(std::abs(controls["filter_style"]->value() - 1.0f) < 0.01f);
  assert(std::abs(controls["reverb_feedback"]->value() - 0.95f) < 0.01f);

  std::cout << "[PASS] testHierarchicalSchema" << std::endl;
}

void testJsonRoundtrip() {
  std::cout << "[RUN] testJsonRoundtrip..." << std::endl;
  DummySynth synth;

  synth.setPatchName("Original Patch");
  synth.setAuthor("AA-EION");
  synth.getControls()["cutoff"]->set(77.5f);
  synth.getControls()["resonance"]->set(0.33f);

  std::string exported = synth.exportPatchToJson(false);
  assert(!exported.empty());

  DummySynth synth2;
  std::string err;
  bool ok = synth2.loadPatchFromJson(exported, &err);
  assert(ok);

  assert(synth2.getPatchName() == "Original Patch");
  assert(synth2.getAuthor() == "AA-EION");
  assert(std::abs(synth2.getControls()["cutoff"]->value() - 77.5f) < 0.01f);
  assert(std::abs(synth2.getControls()["resonance"]->value() - 0.33f) < 0.01f);

  std::cout << "[PASS] testJsonRoundtrip" << std::endl;
}

void testMcpServerAudioPreviewAndTools() {
  std::cout << "[RUN] testMcpServerAudioPreviewAndTools..." << std::endl;
  AetherHelmMcpServer server;

  // 1. Parameters list
  std::string params = server.listAvailableParameters();
  assert(!params.empty());
  assert(params.find("parameters") != std::string::npos);

  // 2. Set patch parameters with nested patch_or_params
  std::string setJson = R"({
    "patch_or_params": {
      "cutoff": 55.0,
      "resonance": 0.72
    }
  })";
  std::string setResp = server.setPatchParameters(setJson);
  assert(setResp.find("\"success\":true") != std::string::npos || setResp.find("\"success\": true") != std::string::npos);
  assert(setResp.find("\"parameters_updated\":2") != std::string::npos || setResp.find("\"parameters_updated\": 2") != std::string::npos);

  // 3. Audio preview synthesis
  std::string previewJson = server.triggerPreviewNote(60, 0.8f, 0.2f);
  assert(!previewJson.empty());
  assert(previewJson.find("preview_rendered") != std::string::npos);
  assert(previewJson.find("peak_amplitude") != std::string::npos);

  std::cout << "  -> MCP Preview Result: " << previewJson << std::endl;
  std::cout << "[PASS] testMcpServerAudioPreviewAndTools" << std::endl;
}

void testBackgroundThreadSafety() {
  std::cout << "[RUN] testBackgroundThreadSafety..." << std::endl;
  DummySynth synth;

  std::string patchJson = R"({
    "patch_name": "Thread Safety Patch",
    "settings": {
      "cutoff": 92.0,
      "resonance": 0.4
    }
  })";

  // Execute from an isolated background thread (simulating OpenRouterClient worker thread)
  bool threadSuccess = false;
  std::thread bgWorker([&synth, &patchJson, &threadSuccess]() {
    std::string err;
    threadSuccess = synth.loadPatchFromJson(patchJson, &err);
  });
  bgWorker.join();

  assert(threadSuccess);
  assert(synth.getPatchName() == "Thread Safety Patch");
  assert(std::abs(synth.getControls()["cutoff"]->value() - 92.0f) < 0.01f);

  std::cout << "[PASS] testBackgroundThreadSafety" << std::endl;
}

int main() {
  ScopedJuceInitialiser_GUI juceInit;

  std::cout << "========================================" << std::endl;
  std::cout << "   AetherHelm Automated Test Suite      " << std::endl;
  std::cout << "========================================" << std::endl;

  testParameterClamping();
  testHierarchicalSchema();
  testJsonRoundtrip();
  testMcpServerAudioPreviewAndTools();
  testBackgroundThreadSafety();

  std::cout << "\n[ALL TESTS PASSED SUCCESSFULLY!]" << std::endl;
  return 0;
}
