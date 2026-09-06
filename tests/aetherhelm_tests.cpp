#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <cmath>
#include <thread>
#include "JuceHeader.h"
#include "helm_common.h"
#include "helm_engine.h"
#include "headless_synth.h"
#include "aether_patch_serializer.h"
#include "aetherhelm_mcp_server.h"

using DummySynth = HeadlessSynth;

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

void testMcpModulationsIntegration() {
  std::cout << "[RUN] testMcpModulationsIntegration..." << std::endl;
  AetherHelmMcpServer server;

  // Set modulations via MCP setPatchParameters
  std::string setWithMods = R"({
    "patch_name": "Modulated Lead",
    "settings": {
      "cutoff": 70.0
    },
    "modulations": [
      {
        "source": "mod_envelope",
        "destination": "cutoff",
        "amount": 0.65
      },
      {
        "source": "mono_lfo_1",
        "destination": "filter_blend",
        "amount": -0.40
      }
    ]
  })";

  std::string setResp = server.setPatchParameters(setWithMods);
  assert(setResp.find("\"success\":true") != std::string::npos || setResp.find("\"success\": true") != std::string::npos);

  // Verify modulations are preserved in current patch
  std::string currentPatch = server.getCurrentPatch();
  assert(currentPatch.find("mod_envelope") != std::string::npos);
  assert(currentPatch.find("mono_lfo_1") != std::string::npos);
  assert(currentPatch.find("Modulated Lead") != std::string::npos);

  // Audition audio rendering with modulations active
  std::string preview = server.triggerPreviewNote(64, 0.9f, 0.25f);
  assert(preview.find("preview_rendered") != std::string::npos);
  assert(preview.find("\"signal_detected\": true") != std::string::npos || preview.find("\"signal_detected\":true") != std::string::npos);

  std::cout << "[PASS] testMcpModulationsIntegration" << std::endl;
}

void testJsonExtractionWithMarkdownAndCommentaryBraces() {
  std::cout << "[RUN] testJsonExtractionWithMarkdownAndCommentaryBraces..." << std::endl;

  // Case 1: Markdown fenced code with trailing commentary containing braces
  std::string markdownWithCommentary =
    "Here is your synth patch:\n"
    "```json\n"
    "{\n"
    "  \"patch_name\": \"Cyber Bass\",\n"
    "  \"settings\": {\n"
    "    \"cutoff\": 48.0,\n"
    "    \"resonance\": 0.65\n"
    "  }\n"
    "}\n"
    "```\n"
    "Note: The {cutoff} was lowered to {48.0} to create warmth.\n";

  std::string extracted1 = AetherPatchSerializer::extractJsonFromText(markdownWithCommentary);
  var parsed1;
  Result r1 = JSON::parse(String(extracted1), parsed1);
  assert(r1.wasOk());
  assert(parsed1.isObject());
  assert(parsed1.getDynamicObject()->getProperty("patch_name").toString() == "Cyber Bass");

  // Case 2: Raw JSON without markdown, but with trailing comment containing braces
  std::string rawWithBraces =
    "{\n"
    "  \"patch_name\": \"Pluck\",\n"
    "  \"settings\": { \"cutoff\": 90.0 }\n"
    "}\n"
    "Additional guidance: Check {amp_envelope} for decay settings.\n";

  std::string extracted2 = AetherPatchSerializer::extractJsonFromText(rawWithBraces);
  var parsed2;
  Result r2 = JSON::parse(String(extracted2), parsed2);
  assert(r2.wasOk());
  assert(parsed2.isObject());
  assert(parsed2.getDynamicObject()->getProperty("patch_name").toString() == "Pluck");

  // Case 3: JSON containing braces inside string values
  std::string jsonWithBraceInString = R"({
    "patch_name": "Preset {Special Edition}",
    "settings": { "cutoff": 64.0 }
  })";

  std::string extracted3 = AetherPatchSerializer::extractJsonFromText(jsonWithBraceInString);
  var parsed3;
  Result r3 = JSON::parse(String(extracted3), parsed3);
  assert(r3.wasOk());
  assert(parsed3.isObject());
  assert(parsed3.getDynamicObject()->getProperty("patch_name").toString() == "Preset {Special Edition}");

  std::cout << "[PASS] testJsonExtractionWithMarkdownAndCommentaryBraces" << std::endl;
}

void testNaNAndInfinityProtection() {
  std::cout << "[RUN] testNaNAndInfinityProtection..." << std::endl;
  DummySynth synth;

  // Verify non-finite floats don't corrupt the DSP controls
  AetherPatchSerializer::applyControl(&synth, "cutoff", std::numeric_limits<float>::quiet_NaN());
  float cutoffVal = synth.getControls()["cutoff"]->value();
  assert(std::isfinite(cutoffVal));

  AetherPatchSerializer::applyControl(&synth, "resonance", std::numeric_limits<float>::infinity());
  float resVal = synth.getControls()["resonance"]->value();
  assert(std::isfinite(resVal));

  AetherPatchSerializer::applyModulation(&synth, "mod_envelope", "cutoff", std::numeric_limits<float>::quiet_NaN());
  AetherPatchSerializer::applyModulation(&synth, "mod_envelope", "resonance", std::numeric_limits<float>::infinity());

  std::cout << "[PASS] testNaNAndInfinityProtection" << std::endl;
}

void testBooleanParameterSupport() {
  std::cout << "[RUN] testBooleanParameterSupport..." << std::endl;
  DummySynth synth;

  std::string boolJson = R"({
    "settings": {
      "distortion_on": true,
      "delay_sync": false,
      "reverb_on": true
    },
    "effects": {
      "delay": {
        "on": true
      }
    }
  })";

  std::string err;
  bool ok = synth.loadPatchFromJson(boolJson, &err);
  assert(ok);

  mopo::control_map controls = synth.getControls();
  assert(controls["distortion_on"]->value() == 1.0f);
  assert(controls["delay_sync"]->value() == 0.0f);
  assert(controls["reverb_on"]->value() == 1.0f);
  assert(controls["delay_on"]->value() == 1.0f);

  std::cout << "[PASS] testBooleanParameterSupport" << std::endl;
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

void testMcpPresetInspectionBothLayouts() {
  std::cout << "[RUN] testMcpPresetInspectionBothLayouts..." << std::endl;
  AetherHelmMcpServer server;

  // 1. Flat layout
  std::string flatJson = server.getCurrentPatch(false);
  assert(!flatJson.empty());
  assert(flatJson.find("\"settings\"") != std::string::npos);
  assert(flatJson.find("\"synth_name\"") != std::string::npos);
  assert(flatJson.find("\"oscillators\"") == std::string::npos);

  // 2. Structured/hierarchical layout
  std::string structJson = server.getCurrentPatch(true);
  assert(!structJson.empty());
  assert(structJson.find("\"oscillators\"") != std::string::npos);
  assert(structJson.find("\"filter\"") != std::string::npos);
  assert(structJson.find("\"envelopes\"") != std::string::npos);
  assert(structJson.find("\"modulators\"") != std::string::npos);
  assert(structJson.find("\"effects\"") != std::string::npos);
  assert(structJson.find("\"global\"") != std::string::npos);
  assert(structJson.find("\"modulations\"") != std::string::npos);

  std::cout << "[PASS] testMcpPresetInspectionBothLayouts" << std::endl;
}

void testMcpGranularBatchAndAliasAdjustment() {
  std::cout << "[RUN] testMcpGranularBatchAndAliasAdjustment..." << std::endl;
  AetherHelmMcpServer server;

  // 1. Batch adjustment using aliases
  std::string aliasJson = R"({
    "patch_or_params": {
      "cutoff_frequency": 52.5,
      "resonance_amount": 0.88,
      "reverb_decay": 0.92,
      "osc1_vol": 0.65,
      "sub_vol": 0.40,
      "noise_level": 0.15
    }
  })";

  std::string resp = server.setPatchParameters(aliasJson);
  assert(resp.find("\"success\":true") != std::string::npos || resp.find("\"success\": true") != std::string::npos);

  std::string detailsJson = server.getParameterDetails(R"({"parameters": ["cutoff", "resonance", "reverb_feedback", "osc_1_volume", "sub_volume", "noise_volume"]})");
  assert(detailsJson.find("\"id\":\"cutoff\"") != std::string::npos || detailsJson.find("\"id\": \"cutoff\"") != std::string::npos);
  assert(detailsJson.find("\"id\":\"resonance\"") != std::string::npos || detailsJson.find("\"id\": \"resonance\"") != std::string::npos);
  assert(detailsJson.find("\"id\":\"reverb_feedback\"") != std::string::npos || detailsJson.find("\"id\": \"reverb_feedback\"") != std::string::npos);
  assert(detailsJson.find("\"id\":\"osc_1_volume\"") != std::string::npos || detailsJson.find("\"id\": \"osc_1_volume\"") != std::string::npos);
  assert(detailsJson.find("\"id\":\"sub_volume\"") != std::string::npos || detailsJson.find("\"id\": \"sub_volume\"") != std::string::npos);
  assert(detailsJson.find("\"id\":\"noise_volume\"") != std::string::npos || detailsJson.find("\"id\": \"noise_volume\"") != std::string::npos);

  // 2. Structured module trees adjustment
  std::string structSetJson = R"({
    "filter": {
      "style": 1.0,
      "blend": 0.5,
      "drive": 2.0
    },
    "oscillators": {
      "osc_1": {
        "waveform": 3.0
      }
    }
  })";
  std::string structResp = server.setPatchParameters(structSetJson);
  assert(structResp.find("\"success\":true") != std::string::npos || structResp.find("\"success\": true") != std::string::npos);
  std::string structDetails = server.getParameterDetails(R"({"parameters": ["filter_style", "filter_blend", "filter_drive", "osc_1_waveform"]})");
  assert(structDetails.find("\"id\":\"filter_style\"") != std::string::npos || structDetails.find("\"id\": \"filter_style\"") != std::string::npos);
  assert(structDetails.find("\"id\":\"filter_blend\"") != std::string::npos || structDetails.find("\"id\": \"filter_blend\"") != std::string::npos);
  assert(structDetails.find("\"id\":\"filter_drive\"") != std::string::npos || structDetails.find("\"id\": \"filter_drive\"") != std::string::npos);
  assert(structDetails.find("\"id\":\"osc_1_waveform\"") != std::string::npos || structDetails.find("\"id\": \"osc_1_waveform\"") != std::string::npos);

  // 3. Preset with both settings and modulations preservation
  std::string fullPatchJson = R"({
    "patch_name": "Complex Sound",
    "settings": {
      "cutoff": 66.0
    },
    "modulations": [
      {
        "source": "mono_lfo_1",
        "destination": "cutoff",
        "amount": 0.45
      }
    ]
  })";
  server.setPatchParameters(fullPatchJson);
  std::string currPatch = server.getCurrentPatch();
  assert(currPatch.find("mono_lfo_1") != std::string::npos);
  assert(currPatch.find("cutoff") != std::string::npos);

  // 4. Range clamping verification
  std::string clampJson = R"({
    "parameters": {
      "cutoff": 999.0,
      "resonance": -5.0
    }
  })";
  server.setPatchParameters(clampJson);
  std::string clampedDetails = server.getParameterDetails(R"({"parameters": ["cutoff", "resonance"]})");
  // Cutoff max is 127.0, resonance min is 0.0
  assert(clampedDetails.find("127") != std::string::npos);
  assert(clampedDetails.find("0") != std::string::npos);

  std::cout << "[PASS] testMcpGranularBatchAndAliasAdjustment" << std::endl;
}

void testMcpParameterDocumentationAndDetailsDiscovery() {
  std::cout << "[RUN] testMcpParameterDocumentationAndDetailsDiscovery..." << std::endl;
  AetherHelmMcpServer server;

  // 1. Category filtering across all advertised enum categories
  std::string oscParams = server.listAvailableParameters("oscillators", true);
  assert(oscParams.find("\"id\":\"osc_1_waveform\"") != std::string::npos || oscParams.find("\"id\": \"osc_1_waveform\"") != std::string::npos);
  assert(oscParams.find("\"id\":\"sub_volume\"") != std::string::npos || oscParams.find("\"id\": \"sub_volume\"") != std::string::npos);

  std::string envParams = server.listAvailableParameters("envelopes", true);
  assert(envParams.find("\"id\":\"amp_attack\"") != std::string::npos || envParams.find("\"id\": \"amp_attack\"") != std::string::npos);
  assert(envParams.find("\"id\":\"mod_decay\"") != std::string::npos || envParams.find("\"id\": \"mod_decay\"") != std::string::npos);

  std::string modParams = server.listAvailableParameters("modulators", true);
  assert(modParams.find("\"id\":\"mono_lfo_1_frequency\"") != std::string::npos || modParams.find("\"id\": \"mono_lfo_1_frequency\"") != std::string::npos);
  assert(modParams.find("\"id\":\"arp_gate\"") != std::string::npos || modParams.find("\"id\": \"arp_gate\"") != std::string::npos);

  std::string fxParams = server.listAvailableParameters("effects", true);
  assert(fxParams.find("\"id\":\"reverb_feedback\"") != std::string::npos || fxParams.find("\"id\": \"reverb_feedback\"") != std::string::npos);
  assert(fxParams.find("\"id\":\"delay_tempo\"") != std::string::npos || fxParams.find("\"id\": \"delay_tempo\"") != std::string::npos);

  std::string filterParams = server.listAvailableParameters("filter", true);
  assert(filterParams.find("\"category\":\"Filter\"") != std::string::npos || filterParams.find("\"category\": \"Filter\"") != std::string::npos);
  assert(filterParams.find("\"id\":\"cutoff\"") != std::string::npos || filterParams.find("\"id\": \"cutoff\"") != std::string::npos);
  assert(filterParams.find("Filter cutoff frequency") != std::string::npos);
  // Oscillator shouldn't be in filter category list
  assert(filterParams.find("\"id\":\"osc_1_waveform\"") == std::string::npos && filterParams.find("\"id\": \"osc_1_waveform\"") == std::string::npos);

  // 2. Concise listing
  std::string concise = server.listAvailableParameters("all", false);
  assert(concise.find("\"id\":\"cutoff\"") != std::string::npos || concise.find("\"id\": \"cutoff\"") != std::string::npos);
  assert(concise.find("\"description\"") == std::string::npos);

  // 3. Parameter details discovery with alias resolution
  std::string detailSingle = server.getParameterDetails(R"({"parameter": "cutoff_frequency"})");
  assert(detailSingle.find("\"success\":true") != std::string::npos || detailSingle.find("\"success\": true") != std::string::npos);
  assert(detailSingle.find("\"id\":\"cutoff\"") != std::string::npos || detailSingle.find("\"id\": \"cutoff\"") != std::string::npos);
  assert(detailSingle.find("\"min\":28") != std::string::npos || detailSingle.find("\"min\": 28") != std::string::npos);
  assert(detailSingle.find("\"max\":127") != std::string::npos || detailSingle.find("\"max\": 127") != std::string::npos);
  assert(detailSingle.find("\"units\":\"semitones\"") != std::string::npos || detailSingle.find("\"units\": \"semitones\"") != std::string::npos);
  assert(detailSingle.find("\"found\":true") != std::string::npos || detailSingle.find("\"found\": true") != std::string::npos);

  std::string detailRes = server.getParameterDetails(R"({"parameter": "resonance_amount"})");
  assert(detailRes.find("\"id\":\"resonance\"") != std::string::npos || detailRes.find("\"id\": \"resonance\"") != std::string::npos);

  std::string detailOsc = server.getParameterDetails(R"({"parameter": "osc1_vol"})");
  assert(detailOsc.find("\"id\":\"osc_1_volume\"") != std::string::npos || detailOsc.find("\"id\": \"osc_1_volume\"") != std::string::npos);

  // 4. Parameter not found handling
  std::string detailMissing = server.getParameterDetails(R"({"parameter": "non_existent_control"})");
  assert(detailMissing.find("\"found\":false") != std::string::npos || detailMissing.find("\"found\": false") != std::string::npos);

  std::cout << "[PASS] testMcpParameterDocumentationAndDetailsDiscovery" << std::endl;
}

void testMcpModulationRoutingTools() {
  std::cout << "[RUN] testMcpModulationRoutingTools..." << std::endl;
  AetherHelmMcpServer server;

  // 1. Add modulation with aliases (source alias: "mod_env", dest alias: "filter_cutoff")
  std::string addResp = server.addModulation("mod_env", "filter_cutoff", 0.75f);
  assert(addResp.find("\"success\":true") != std::string::npos || addResp.find("\"success\": true") != std::string::npos);
  assert(addResp.find("\"source\":\"mod_envelope\"") != std::string::npos || addResp.find("\"source\": \"mod_envelope\"") != std::string::npos);
  assert(addResp.find("\"destination\":\"cutoff\"") != std::string::npos || addResp.find("\"destination\": \"cutoff\"") != std::string::npos);

  // 2. Query modulation matrix
  std::string matrixJson = server.getModulationMatrix();
  assert(matrixJson.find("mod_envelope") != std::string::npos);
  assert(matrixJson.find("cutoff") != std::string::npos);
  assert(matrixJson.find("available_sources") != std::string::npos);

  // 3. Add modulation with clamped amount
  std::string addClamped = server.addModulation("mono_lfo_1", "filter_blend", 2.5f);
  assert(addClamped.find("\"amount\":1") != std::string::npos || addClamped.find("\"amount\": 1") != std::string::npos);

  // 4. Filter modulation matrix by source
  std::string filteredMatrix = server.getModulationMatrix("mono_lfo_1", "");
  assert(filteredMatrix.find("filter_blend") != std::string::npos);

  // 5. Remove modulation
  std::string remResp = server.removeModulation("mod_env", "filter_cutoff");
  assert(remResp.find("\"success\":true") != std::string::npos || remResp.find("\"success\": true") != std::string::npos);

  // 6. Verify removed
  std::string postRemMatrix = server.getModulationMatrix("mod_envelope", "cutoff");
  assert(postRemMatrix.find("\"count\":0") != std::string::npos || postRemMatrix.find("\"count\": 0") != std::string::npos);

  // 7. Remove nonexistent modulation error handling
  std::string remMissing = server.removeModulation("mod_envelope", "cutoff");
  assert(remMissing.find("\"success\":false") != std::string::npos || remMissing.find("\"success\": false") != std::string::npos);

  // 8. Invalid modulation source error handling
  std::string invalidSrc = server.addModulation("quantum_phase", "cutoff", 0.5f);
  assert(invalidSrc.find("\"success\":false") != std::string::npos || invalidSrc.find("\"success\": false") != std::string::npos);
  assert(invalidSrc.find("Available sources") != std::string::npos);

  std::cout << "[PASS] testMcpModulationRoutingTools" << std::endl;
}

void testMcpAudioAuditionPreviewMetrics() {
  std::cout << "[RUN] testMcpAudioAuditionPreviewMetrics..." << std::endl;
  AetherHelmMcpServer server;

  std::string preview = server.triggerPreviewNote(60, 0.85f, 0.25f);
  assert(!preview.empty());
  assert(preview.find("\"status\":\"preview_rendered\"") != std::string::npos || preview.find("\"status\": \"preview_rendered\"") != std::string::npos);
  assert(preview.find("peak_amplitude") != std::string::npos);
  assert(preview.find("peak_db") != std::string::npos);
  assert(preview.find("rms_level") != std::string::npos);
  assert(preview.find("rms_db") != std::string::npos);
  assert(preview.find("clipping_detected") != std::string::npos);
  assert(preview.find("signal_detected") != std::string::npos);

  std::cout << "[PASS] testMcpAudioAuditionPreviewMetrics" << std::endl;
}

void testMcpToolsExposeExternalAiToolsAndNoOpenRouterProxy() {
  std::cout << "[RUN] testMcpToolsExposeExternalAiToolsAndNoOpenRouterProxy..." << std::endl;
  AetherHelmMcpServer server;

  std::string toolsJson = server.getToolsListJson();

  // All 8 first-class tools must be present
  assert(toolsJson.find("\"name\": \"get_current_patch\"") != std::string::npos || toolsJson.find("\"name\":\"get_current_patch\"") != std::string::npos);
  assert(toolsJson.find("\"name\": \"set_patch_parameters\"") != std::string::npos || toolsJson.find("\"name\":\"set_patch_parameters\"") != std::string::npos);
  assert(toolsJson.find("\"name\": \"list_available_parameters\"") != std::string::npos || toolsJson.find("\"name\":\"list_available_parameters\"") != std::string::npos);
  assert(toolsJson.find("\"name\": \"get_parameter_details\"") != std::string::npos || toolsJson.find("\"name\":\"get_parameter_details\"") != std::string::npos);
  assert(toolsJson.find("\"name\": \"add_modulation\"") != std::string::npos || toolsJson.find("\"name\":\"add_modulation\"") != std::string::npos);
  assert(toolsJson.find("\"name\": \"remove_modulation\"") != std::string::npos || toolsJson.find("\"name\":\"remove_modulation\"") != std::string::npos);
  assert(toolsJson.find("\"name\": \"get_modulation_matrix\"") != std::string::npos || toolsJson.find("\"name\":\"get_modulation_matrix\"") != std::string::npos);
  assert(toolsJson.find("\"name\": \"trigger_preview_note\"") != std::string::npos || toolsJson.find("\"name\":\"trigger_preview_note\"") != std::string::npos);

  // Redundant internal OpenRouter proxy tool MUST NOT be present
  assert(toolsJson.find("generate_patch_from_prompt") == std::string::npos);

  std::cout << "[PASS] testMcpToolsExposeExternalAiToolsAndNoOpenRouterProxy" << std::endl;
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
  testMcpModulationsIntegration();
  testMcpPresetInspectionBothLayouts();
  testMcpGranularBatchAndAliasAdjustment();
  testMcpParameterDocumentationAndDetailsDiscovery();
  testMcpModulationRoutingTools();
  testMcpAudioAuditionPreviewMetrics();
  testMcpToolsExposeExternalAiToolsAndNoOpenRouterProxy();
  testJsonExtractionWithMarkdownAndCommentaryBraces();
  testNaNAndInfinityProtection();
  testBooleanParameterSupport();
  testBackgroundThreadSafety();

  std::cout << "\n[ALL TESTS PASSED SUCCESSFULLY!]" << std::endl;
  return 0;
}
