#include "aetherhelm_mcp_server.h"
#include "aether_patch_serializer.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstdlib>

AetherHelmMcpServer::AetherHelmMcpServer() {
  srand(static_cast<unsigned int>(Time::currentTimeMillis()));
}

AetherHelmMcpServer::~AetherHelmMcpServer() = default;

std::string AetherHelmMcpServer::getCurrentPatch(bool hierarchical) {
  synth_.flushQueues();
  return synth_.exportPatchToJson(hierarchical);
}

std::string AetherHelmMcpServer::setPatchParameters(const std::string& patchOrParamsJson) {
  var parsed;
  Result res = JSON::parse(String(patchOrParamsJson), parsed);
  if (res.failed() || !parsed.isObject())
    return "{\"success\": false, \"error\": \"Invalid JSON object provided.\"}";

  DynamicObject* obj = parsed.getDynamicObject();
  var targetVar = parsed;
  if (obj->hasProperty("patch_or_params") && obj->getProperty("patch_or_params").isObject())
    targetVar = obj->getProperty("patch_or_params");
  else if (obj->hasProperty("patch") && obj->getProperty("patch").isObject())
    targetVar = obj->getProperty("patch");
  else if (obj->hasProperty("parameters") && obj->getProperty("parameters").isObject())
    targetVar = obj->getProperty("parameters");
  else if (obj->hasProperty("settings") && obj->getProperty("settings").isObject()
           && !obj->hasProperty("modulations") && !obj->hasProperty("patch_name"))
    targetVar = obj->getProperty("settings");

  std::string targetJson = JSON::toString(targetVar, false).toStdString();
  std::string err;
  int updatedCount = 0;
  bool ok = synth_.loadPatchFromJson(targetJson, &err, &updatedCount);
  synth_.flushQueues();

  if (!ok && updatedCount == 0) {
    DynamicObject* errResp = new DynamicObject();
    errResp->setProperty("success", false);
    errResp->setProperty("error", String(err.empty() ? "No valid synthesizer parameters updated." : err));
    return JSON::toString(errResp, false).toStdString();
  }

  DynamicObject* resp = new DynamicObject();
  resp->setProperty("success", true);
  resp->setProperty("parameters_updated", updatedCount);
  resp->setProperty("patch_name", synth_.getPatchName());
  resp->setProperty("message", "Updated " + String(updatedCount) + " parameter(s) successfully.");
  return JSON::toString(resp, false).toStdString();
}

static bool matchesCategoryFilter(const std::string& paramCat, const std::string& filterCat) {
  if (filterCat == "all" || filterCat.empty())
    return true;

  std::string f = filterCat;
  std::transform(f.begin(), f.end(), f.begin(), ::tolower);
  std::string c = paramCat;
  std::transform(c.begin(), c.end(), c.begin(), ::tolower);

  if (f == "oscillators" || f == "oscillator" || f == "osc") {
    return c.find("oscillator") != std::string::npos || c == "noise";
  }
  if (f == "filter" || f == "filters") {
    return c == "filter";
  }
  if (f == "envelopes" || f == "envelope" || f == "env") {
    return c.find("envelope") != std::string::npos;
  }
  if (f == "modulators" || f == "modulator" || f == "lfo") {
    return c.find("lfo") != std::string::npos || c.find("step") != std::string::npos || c.find("arp") != std::string::npos;
  }
  if (f == "effects" || f == "effect" || f == "fx") {
    return c == "distortion" || c == "delay" || c == "reverb" || c == "stutter";
  }
  if (f == "global") {
    return c == "global";
  }

  return c.find(f) != std::string::npos || f.find(c) != std::string::npos;
}

std::string AetherHelmMcpServer::listAvailableParameters(const std::string& category, bool includeDetails) {
  DynamicObject* root = new DynamicObject();
  std::map<std::string, mopo::ValueDetails> allDetails = mopo::Parameters::lookup_.getAllDetails();

  Array<var> paramList;
  for (const auto& pair : allDetails) {
    std::string paramCat = AetherPatchSerializer::getParameterCategory(pair.first);

    if (!matchesCategoryFilter(paramCat, category))
      continue;

    DynamicObject* item = new DynamicObject();
    item->setProperty("id", String(pair.first));
    item->setProperty("name", String(pair.second.display_name));
    item->setProperty("category", String(paramCat));
    item->setProperty("min", pair.second.min);
    item->setProperty("max", pair.second.max);
    item->setProperty("default", pair.second.default_value);
    item->setProperty("units", String(pair.second.display_units));
    if (includeDetails)
      item->setProperty("description", String(AetherPatchSerializer::getParameterDescription(pair.first)));
    paramList.add(item);
  }

  root->setProperty("total_parameters", paramList.size());
  root->setProperty("category", String(category));
  root->setProperty("parameters", paramList);
  return JSON::toString(root, true).toStdString();
}

std::string AetherHelmMcpServer::getParameterDetails(const std::string& paramNamesJson) {
  var parsed;
  std::vector<std::string> requestedNames;

  Result res = JSON::parse(String(paramNamesJson), parsed);
  if (res.wasOk()) {
    if (parsed.isObject()) {
      DynamicObject* obj = parsed.getDynamicObject();
      if (obj->hasProperty("parameter"))
        requestedNames.push_back(obj->getProperty("parameter").toString().toStdString());
      else if (obj->hasProperty("parameter_name"))
        requestedNames.push_back(obj->getProperty("parameter_name").toString().toStdString());
      else if (obj->hasProperty("name"))
        requestedNames.push_back(obj->getProperty("name").toString().toStdString());
      else if (obj->hasProperty("id"))
        requestedNames.push_back(obj->getProperty("id").toString().toStdString());

      if (obj->hasProperty("parameters") && obj->getProperty("parameters").isArray()) {
        Array<var>* arr = obj->getProperty("parameters").getArray();
        if (arr) {
          for (const var& v : *arr)
            requestedNames.push_back(v.toString().toStdString());
        }
      }
    } else if (parsed.isArray()) {
      Array<var>* arr = parsed.getArray();
      if (arr) {
        for (const var& v : *arr)
          requestedNames.push_back(v.toString().toStdString());
      }
    } else if (parsed.isString()) {
      requestedNames.push_back(parsed.toString().toStdString());
    }
  } else if (!paramNamesJson.empty() && paramNamesJson.front() != '{' && paramNamesJson.front() != '[') {
    requestedNames.push_back(paramNamesJson);
  }

  if (requestedNames.empty()) {
    DynamicObject* err = new DynamicObject();
    err->setProperty("success", false);
    err->setProperty("error", "No parameter name specified. Provide 'parameter' (string) or 'parameters' (array).");
    return JSON::toString(err, false).toStdString();
  }

  synth_.flushQueues();
  mopo::control_map controls = synth_.getControls();
  Array<var> detailsList;

  for (const std::string& rawName : requestedNames) {
    std::string resolved = AetherPatchSerializer::resolveParameterName(rawName);
    if (!mopo::Parameters::isParameter(resolved)) {
      DynamicObject* notFound = new DynamicObject();
      notFound->setProperty("requested_id", String(rawName));
      notFound->setProperty("found", false);
      notFound->setProperty("error", "Unknown parameter or alias: '" + String(rawName) + "'");
      detailsList.add(notFound);
      continue;
    }

    mopo::ValueDetails vd = mopo::Parameters::getDetails(resolved);
    DynamicObject* item = new DynamicObject();
    item->setProperty("id", String(resolved));
    item->setProperty("requested_id", String(rawName));
    item->setProperty("name", String(vd.display_name));
    item->setProperty("category", String(AetherPatchSerializer::getParameterCategory(resolved)));
    item->setProperty("min", vd.min);
    item->setProperty("max", vd.max);
    item->setProperty("default", vd.default_value);

    float currentVal = vd.default_value;
    auto it = controls.find(resolved);
    if (it != controls.end() && it->second)
      currentVal = it->second->value();
    item->setProperty("current_value", currentVal);

    item->setProperty("units", String(vd.display_units));
    item->setProperty("description", String(AetherPatchSerializer::getParameterDescription(resolved)));
    item->setProperty("found", true);
    detailsList.add(item);
  }

  DynamicObject* root = new DynamicObject();
  root->setProperty("success", true);
  root->setProperty("parameters", detailsList);
  return JSON::toString(root, true).toStdString();
}

std::string AetherHelmMcpServer::addModulation(const std::string& source, const std::string& destination, float amount) {
  std::string resSource = AetherPatchSerializer::resolveModulationSourceName(source);
  std::string resDest = AetherPatchSerializer::resolveParameterName(destination);

  bool validSource = AetherPatchSerializer::isValidModulationSource(resSource) || (synth_.getModSource(resSource) != nullptr);
  if (!validSource) {
    DynamicObject* err = new DynamicObject();
    err->setProperty("success", false);
    err->setProperty("error", "Invalid modulation source: '" + String(source) + "'. Available sources: mono_lfo_1, mono_lfo_2, poly_lfo, mod_envelope, fil_envelope, amp_envelope, step_sequencer, velocity, note, mod_wheel, aftertouch, pitch_wheel, random.");
    return JSON::toString(err, false).toStdString();
  }

  if (!mopo::Parameters::isParameter(resDest)) {
    DynamicObject* err = new DynamicObject();
    err->setProperty("success", false);
    err->setProperty("error", "Invalid modulation destination parameter: '" + String(destination) + "'.");
    return JSON::toString(err, false).toStdString();
  }

  if (!std::isfinite(amount))
    amount = 0.0f;

  float clamped = std::clamp(amount, -1.0f, 1.0f);
  synth_.changeModulationAmount(resSource, resDest, clamped);
  synth_.flushQueues();

  DynamicObject* resp = new DynamicObject();
  resp->setProperty("success", true);
  resp->setProperty("source", String(resSource));
  resp->setProperty("destination", String(resDest));
  resp->setProperty("amount", clamped);
  resp->setProperty("message", "Modulation connection configured from '" + String(resSource) + "' to '" + String(resDest) + "' with amount " + String(clamped, 3));
  return JSON::toString(resp, false).toStdString();
}

std::string AetherHelmMcpServer::removeModulation(const std::string& source, const std::string& destination) {
  std::string resSource = AetherPatchSerializer::resolveModulationSourceName(source);
  std::string resDest = AetherPatchSerializer::resolveParameterName(destination);

  bool found = false;
  for (mopo::ModulationConnection* conn : synth_.getModulationConnections()) {
    if (conn && conn->source == resSource && conn->destination == resDest) {
      found = true;
      break;
    }
  }

  if (!found) {
    DynamicObject* err = new DynamicObject();
    err->setProperty("success", false);
    err->setProperty("error", "No active modulation connection found between source '" + String(source) + "' and destination '" + String(destination) + "'.");
    return JSON::toString(err, false).toStdString();
  }

  synth_.changeModulationAmount(resSource, resDest, 0.0f);
  synth_.flushQueues();

  DynamicObject* resp = new DynamicObject();
  resp->setProperty("success", true);
  resp->setProperty("source", String(resSource));
  resp->setProperty("destination", String(resDest));
  resp->setProperty("message", "Modulation connection from '" + String(resSource) + "' to '" + String(resDest) + "' removed successfully.");
  return JSON::toString(resp, false).toStdString();
}

std::string AetherHelmMcpServer::getModulationMatrix(const std::string& filterSource, const std::string& filterDest) {
  synth_.flushQueues();

  std::string resFilterSource = filterSource.empty() ? "" : AetherPatchSerializer::resolveModulationSourceName(filterSource);
  std::string resFilterDest = filterDest.empty() ? "" : AetherPatchSerializer::resolveParameterName(filterDest);

  DynamicObject* root = new DynamicObject();
  Array<var> connsArray;

  std::set<mopo::ModulationConnection*> modConnections = synth_.getModulationConnections();
  for (mopo::ModulationConnection* conn : modConnections) {
    if (!conn || conn->amount.value() == 0.0f) continue;
    if (!resFilterSource.empty() && conn->source != resFilterSource) continue;
    if (!resFilterDest.empty() && conn->destination != resFilterDest) continue;

    DynamicObject* item = new DynamicObject();
    item->setProperty("source", String(conn->source));
    item->setProperty("destination", String(conn->destination));
    item->setProperty("amount", conn->amount.value());
    connsArray.add(item);
  }

  root->setProperty("count", connsArray.size());
  root->setProperty("connections", connsArray);

  Array<var> sourcesArray;
  for (const std::string& src : AetherPatchSerializer::getAvailableModulationSources())
    sourcesArray.add(String(src));
  root->setProperty("available_sources", sourcesArray);

  return JSON::toString(root, true).toStdString();
}

std::string AetherHelmMcpServer::triggerPreviewNote(int noteNumber, float velocity, float durationSeconds) {
  noteNumber = std::clamp(noteNumber, 0, 127);
  velocity = std::clamp(velocity, 0.0f, 1.0f);
  durationSeconds = std::clamp(durationSeconds, 0.05f, 5.0f);

  const int sampleRate = 44100;
  const int bufferSize = 256;
  synth_.getEngine()->setSampleRate(sampleRate);
  synth_.getEngine()->setBufferSize(bufferSize);

  int totalSamples = static_cast<int>(durationSeconds * sampleRate);
  int noteOffSample = static_cast<int>(durationSeconds * 0.8f * sampleRate);

  synth_.getEngine()->noteOn(static_cast<float>(noteNumber), velocity, 0, 0);

  int samplesRendered = 0;
  float peakVal = 0.0f;
  double sumSquares = 0.0;
  int sampleCount = 0;
  bool noteOffSent = false;

  while (samplesRendered < totalSamples) {
    if (!noteOffSent && samplesRendered >= noteOffSample) {
      synth_.getEngine()->allNotesOff(0);
      noteOffSent = true;
    }

    synth_.renderBlock();

    const mopo::mopo_float* left = synth_.getEngine()->output(0)->buffer;
    const mopo::mopo_float* right = synth_.getEngine()->output(1)->buffer;

    for (int i = 0; i < bufferSize; ++i) {
      float l = std::abs(static_cast<float>(left[i]));
      float r = std::abs(static_cast<float>(right[i]));
      float s = std::max(l, r);
      if (s > peakVal) peakVal = s;
      sumSquares += (l * l + r * r) * 0.5;
      sampleCount++;
    }

    samplesRendered += bufferSize;
  }

  synth_.getEngine()->allNotesOff(0);

  double rms = (sampleCount > 0) ? std::sqrt(sumSquares / sampleCount) : 0.0;
  float peakDb = (peakVal > 1e-5f) ? (20.0f * std::log10(peakVal)) : -100.0f;
  double rmsDb = (rms > 1e-5) ? (20.0 * std::log10(rms)) : -100.0;

  DynamicObject* report = new DynamicObject();
  report->setProperty("status", "preview_rendered");
  report->setProperty("midi_note", noteNumber);
  report->setProperty("velocity", velocity);
  report->setProperty("duration_seconds", durationSeconds);
  report->setProperty("peak_amplitude", peakVal);
  report->setProperty("peak_db", peakDb);
  report->setProperty("rms_level", rms);
  report->setProperty("rms_db", rmsDb);
  report->setProperty("clipping_detected", peakVal > 1.0f);
  report->setProperty("signal_detected", peakVal > 0.0001f);

  return JSON::toString(report, true).toStdString();
}

std::string AetherHelmMcpServer::morphPatch(const std::string& targetPatchOrParamsJson, float factor) {
  factor = std::clamp(factor, 0.0f, 1.0f);

  var parsed;
  Result res = JSON::parse(String(targetPatchOrParamsJson), parsed);
  if (res.failed() || !parsed.isObject()) {
    DynamicObject* errResp = new DynamicObject();
    errResp->setProperty("success", false);
    errResp->setProperty("error", "Invalid target patch JSON provided.");
    return JSON::toString(errResp, false).toStdString();
  }

  std::map<std::string, float> targetControls;
  auto extractTargets = [&targetControls](auto& self, const var& node, const std::string& prefix) -> void {
    if (!node.isObject()) return;
    DynamicObject* d = node.getDynamicObject();
    for (const auto& prop : d->getProperties()) {
      std::string key = prop.name.toString().toStdString();
      if (key == "modulations" || (prefix.empty() && (key == "patch_name" || key == "author" ||
          key == "folder_name" || key == "synth_name" || key == "synth_version" || key == "license"))) {
        continue;
      }
      const var& val = prop.value;

      if (val.isObject()) {
        std::string nextPrefix;
        if (key == "settings" || key == "parameters" || key == "patch_or_params" || key == "patch" || key == "target_patch")
          nextPrefix = prefix;
        else
          nextPrefix = prefix.empty() ? key : (prefix + "_" + key);
        self(self, val, nextPrefix);
      } else if (val.isDouble() || val.isInt() || val.isInt64() || val.isBool()) {
        float fVal = val.isBool() ? ((bool)val ? 1.0f : 0.0f) : static_cast<float>(val);
        if (std::isnan(fVal) || std::isinf(fVal)) continue;
        std::string fullKey = prefix.empty() ? key : (prefix + "_" + key);
        std::string resolved = AetherPatchSerializer::resolveParameterName(fullKey);
        if (resolved.empty()) resolved = AetherPatchSerializer::resolveParameterName(key);
        if (resolved.empty()) resolved = fullKey;
        targetControls[resolved] = fVal;
      }
    }
  };

  extractTargets(extractTargets, parsed, "");

  mopo::control_map currentControls = synth_.getControls();
  int morphedCount = 0;
  for (const auto& pair : targetControls) {
    auto it = currentControls.find(pair.first);
    if (it != currentControls.end() && it->second != nullptr) {
      float currentVal = it->second->value();
      float targetVal = pair.second;
      float morphedVal = mopo::utils::interpolate(currentVal, targetVal, factor);
      AetherPatchSerializer::applyControl(&synth_, pair.first, morphedVal);
      morphedCount++;
    }
  }

  synth_.flushQueues();

  DynamicObject* resp = new DynamicObject();
  resp->setProperty("success", true);
  resp->setProperty("factor", factor);
  resp->setProperty("parameters_morphed", morphedCount);
  resp->setProperty("patch_name", synth_.getPatchName());
  resp->setProperty("message", "Morphed " + String(morphedCount) + " parameter(s) with factor " + String(factor, 2) + ".");
  return JSON::toString(resp, false).toStdString();
}

std::string AetherHelmMcpServer::randomizeSection(const std::string& section, float intensity) {
  intensity = std::clamp(intensity, 0.0f, 1.0f);
  mopo::control_map controls = synth_.getControls();
  std::map<std::string, mopo::ValueDetails> allDetails = mopo::Parameters::lookup_.getAllDetails();

  int randomizedCount = 0;
  for (const auto& pair : allDetails) {
    const std::string& paramName = pair.first;
    std::string paramCat = AetherPatchSerializer::getParameterCategory(paramName);

    if (!matchesCategoryFilter(paramCat, section))
      continue;

    // Safety: don't randomize master output volume to avoid deafening clicks or complete silence
    if (paramName == "volume" || paramName == "beats_per_minute")
      continue;

    auto it = controls.find(paramName);
    if (it == controls.end() || it->second == nullptr)
      continue;

    float minVal = pair.second.min;
    float maxVal = pair.second.max;
    float currentVal = it->second->value();

    float randFrac = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float randTarget = minVal + randFrac * (maxVal - minVal);

    float newVal = mopo::utils::interpolate(currentVal, randTarget, intensity);
    newVal = (std::clamp)(newVal, minVal, maxVal);

    AetherPatchSerializer::applyControl(&synth_, paramName, newVal);
    randomizedCount++;
  }

  synth_.flushQueues();

  DynamicObject* resp = new DynamicObject();
  resp->setProperty("success", true);
  resp->setProperty("section", String(section));
  resp->setProperty("intensity", intensity);
  resp->setProperty("parameters_randomized", randomizedCount);
  resp->setProperty("message", "Randomized " + String(randomizedCount) + " parameter(s) in section '" + String(section) + "' with intensity " + String(intensity, 2) + ".");
  return JSON::toString(resp, false).toStdString();
}

std::string AetherHelmMcpServer::describePatch(bool /*verbose*/) {
  mopo::control_map controls = synth_.getControls();

  auto getVal = [&controls](const std::string& name, float def = 0.0f) -> float {
    auto it = controls.find(name);
    return (it != controls.end() && it->second != nullptr) ? it->second->value() : def;
  };

  std::string name = synth_.getPatchName().toStdString();
  std::string author = synth_.getAuthor().toStdString();
  if (name.empty()) name = "Init Patch";
  if (author.empty()) author = "Unknown";

  float cutoff = getVal("cutoff", 64.0f);
  float res = getVal("resonance", 0.0f);
  float osc1Vol = getVal("osc_1_volume", 1.0f);
  float osc2Vol = getVal("osc_2_volume", 0.0f);
  float subVol = getVal("sub_volume", 0.0f);
  float noiseVol = getVal("noise_volume", 0.0f);
  float attack = getVal("amp_attack", 0.01f);
  float release = getVal("amp_release", 0.2f);
  float filterStyle = getVal("filter_style", 0.0f);

  std::string style;
  if (cutoff < 55.0f && (subVol > 0.3f || osc1Vol > 0.5f) && attack < 0.05f) {
    style = "Bass";
  } else if (attack > 0.3f || release > 1.5f) {
    style = "Pad / Atmospheric";
  } else if (attack < 0.02f && release < 0.35f) {
    style = "Pluck / Percussive Lead";
  } else {
    style = "Lead / Synth";
  }

  Array<var> activeFx;
  if (getVal("distortion_on", 0.0f) > 0.5f) activeFx.add("Distortion");
  if (getVal("delay_on", 0.0f) > 0.5f) activeFx.add("Delay");
  if (getVal("reverb_on", 0.0f) > 0.5f) activeFx.add("Reverb");
  if (getVal("stutter_on", 0.0f) > 0.5f) activeFx.add("Stutter");

  int modCount = 0;
  for (mopo::ModulationConnection* conn : synth_.getModulationConnections()) {
    if (conn && conn->amount.value() != 0.0f)
      modCount++;
  }

  std::ostringstream desc;
  desc << "### AetherHelm Patch: " << name << "\n"
       << "**Author:** " << author << "\n"
       << "**Sonic Profile:** " << style << "\n\n"
       << "**Oscillator Setup:**\n"
       << "- Osc 1 Vol: " << osc1Vol << ", Osc 2 Vol: " << osc2Vol
       << ", Sub Vol: " << subVol << ", Noise Vol: " << noiseVol << "\n\n"
       << "**Filter Section:**\n"
       << "- Style: " << (filterStyle == 0.0f ? "12dB State Variable" : (filterStyle == 1.0f ? "24dB State Variable" : "Formant/Ladder"))
       << ", Cutoff: " << cutoff << " st, Resonance: " << res << "\n\n"
       << "**Envelope Timing:**\n"
       << "- Amp Attack: " << attack << " s, Amp Release: " << release << " s\n\n"
       << "**Active Effects:** " << (activeFx.isEmpty() ? "None" : JSON::toString(activeFx).toStdString()) << "\n"
       << "**Active Modulations:** " << modCount << " routed connection(s).";

  DynamicObject* resp = new DynamicObject();
  resp->setProperty("success", true);
  resp->setProperty("patch_name", String(name));
  resp->setProperty("author", String(author));
  resp->setProperty("sonic_profile", String(style));
  resp->setProperty("description", String(desc.str()));
  resp->setProperty("active_effects", activeFx);
  resp->setProperty("modulations_count", modCount);

  return JSON::toString(resp, true).toStdString();
}

std::string AetherHelmMcpServer::validatePatch(const std::string& candidatePatchJson) {
  var parsed;
  Result res = JSON::parse(String(candidatePatchJson), parsed);
  if (res.failed() || !parsed.isObject()) {
    DynamicObject* errResp = new DynamicObject();
    errResp->setProperty("valid", false);
    Array<var> errs;
    errs.add("JSON parse error: " + res.getErrorMessage());
    errResp->setProperty("errors", errs);
    return JSON::toString(errResp, false).toStdString();
  }

  Array<var> errors;
  Array<var> warnings;
  int checked = 0;

  DynamicObject* rootObj = parsed.getDynamicObject();
  std::map<std::string, mopo::ValueDetails> allDetails = mopo::Parameters::lookup_.getAllDetails();

  auto validateKeyVal = [&](const std::string& fullKey, const std::string& rawKey, const var& val) {
    checked++;
    std::string resolved = AetherPatchSerializer::resolveParameterName(fullKey);
    if (resolved.empty()) resolved = AetherPatchSerializer::resolveParameterName(rawKey);
    if (resolved.empty()) resolved = fullKey;

    auto it = allDetails.find(resolved);
    if (it == allDetails.end()) {
      warnings.add("Unknown or unmapped parameter: '" + String(fullKey) + "'");
      return;
    }

    if (val.isDouble() || val.isInt() || val.isInt64()) {
      double d = (double)val;
      if (std::isnan(d) || std::isinf(d)) {
        errors.add("Non-finite numeric value (NaN/Inf) for parameter '" + String(fullKey) + "'");
      } else if (d < it->second.min - 0.001 || d > it->second.max + 0.001) {
        warnings.add("Parameter '" + String(fullKey) + "' value " + String(d) +
                     " exceeds valid range [" + String(it->second.min) + ", " +
                     String(it->second.max) + "] (will be clamped)");
      }
    } else if (val.isBool()) {
      // Valid boolean parameter
    } else {
      errors.add("Invalid non-numeric value for parameter '" + String(fullKey) + "'");
    }
  };

  auto visitObject = [&](auto& self, const var& node, const std::string& prefix) -> void {
    if (!node.isObject()) return;
    DynamicObject* d = node.getDynamicObject();
    for (const auto& prop : d->getProperties()) {
      std::string key = prop.name.toString().toStdString();
      const var& val = prop.value;

      if (key == "modulations" || (prefix.empty() && (key == "patch_name" || key == "author" ||
          key == "folder_name" || key == "synth_name" || key == "synth_version" || key == "license"))) {
        continue;
      }

      if (val.isObject()) {
        std::string nextPrefix;
        if (key == "settings" || key == "parameters" || key == "patch_or_params" || key == "patch" || key == "target_patch")
          nextPrefix = prefix;
        else
          nextPrefix = prefix.empty() ? key : (prefix + "_" + key);
        self(self, val, nextPrefix);
      } else {
        std::string fullKey = prefix.empty() ? key : (prefix + "_" + key);
        validateKeyVal(fullKey, key, val);
      }
    }
  };

  visitObject(visitObject, parsed, "");

  // Modulations validation (check root, settings, or patch)
  const Array<var>* mods = nullptr;
  if (rootObj->hasProperty("modulations") && rootObj->getProperty("modulations").isArray()) {
    mods = rootObj->getProperty("modulations").getArray();
  } else if (rootObj->hasProperty("settings") && rootObj->getProperty("settings").isObject()) {
    DynamicObject* s = rootObj->getProperty("settings").getDynamicObject();
    if (s->hasProperty("modulations") && s->getProperty("modulations").isArray())
      mods = s->getProperty("modulations").getArray();
  } else if (rootObj->hasProperty("patch") && rootObj->getProperty("patch").isObject()) {
    DynamicObject* p = rootObj->getProperty("patch").getDynamicObject();
    if (p->hasProperty("modulations") && p->getProperty("modulations").isArray())
      mods = p->getProperty("modulations").getArray();
  }

  if (mods) {
    for (int i = 0; i < mods->size(); ++i) {
      var mVar = (*mods)[i];
      if (!mVar.isObject()) {
        errors.add("Modulation at index " + String(i) + " is not an object.");
        continue;
      }
      DynamicObject* m = mVar.getDynamicObject();
      if (!m->hasProperty("source") || !m->hasProperty("destination") || !m->hasProperty("amount")) {
        errors.add("Modulation at index " + String(i) + " missing source, destination, or amount.");
      } else {
        std::string src = m->getProperty("source").toString().toStdString();
        std::string dst = m->getProperty("destination").toString().toStdString();
        var amtVar = m->getProperty("amount");
        if (amtVar.isDouble() || amtVar.isInt() || amtVar.isInt64()) {
          double amt = (double)amtVar;
          if (std::isnan(amt) || std::isinf(amt)) {
            errors.add("Modulation at index " + String(i) + " has non-finite amount (NaN/Inf).");
          } else if (amt < -1.0 || amt > 1.0) {
            warnings.add("Modulation amount " + String(amt) + " exceeds [-1.0, 1.0] (will be clamped).");
          }
        } else {
          errors.add("Modulation at index " + String(i) + " amount must be numeric.");
        }
        std::string resSrc = AetherPatchSerializer::resolveModulationSourceName(src);
        std::string resDst = AetherPatchSerializer::resolveParameterName(dst);
        if (!AetherPatchSerializer::isValidModulationSource(src)) errors.add("Invalid modulation source: '" + String(src) + "'");
        if (allDetails.find(resDst) == allDetails.end()) warnings.add("Unknown modulation destination: '" + String(dst) + "'");
      }
    }
  }

  bool valid = errors.isEmpty();
  DynamicObject* resp = new DynamicObject();
  resp->setProperty("valid", valid);
  resp->setProperty("errors", errors);
  resp->setProperty("warnings", warnings);
  resp->setProperty("parameters_checked", checked);
  return JSON::toString(resp, true).toStdString();
}

bool AetherHelmMcpServer::installDesktopConfigs(bool claude, bool cursor, bool windsurf, bool cline) {
  File selfExe = File::getSpecialLocation(File::currentExecutableFile);
  String exePath = selfExe.getFullPathName();

  auto registerMcpInConfig = [&exePath](const File& configFile, const String& configName) {
    if (!configFile.getParentDirectory().exists())
      configFile.getParentDirectory().createDirectory();

    var configVar;
    if (configFile.exists()) {
      Result r = JSON::parse(configFile.loadFileAsString(), configVar);
      if (r.failed() || !configVar.isObject())
        configVar = new DynamicObject();
    } else {
      configVar = new DynamicObject();
    }

    DynamicObject* root = configVar.getDynamicObject();
    var mcpServersVar = root->getProperty("mcpServers");
    if (!mcpServersVar.isObject()) {
      mcpServersVar = new DynamicObject();
      root->setProperty("mcpServers", mcpServersVar);
    }

    DynamicObject* mcpServers = mcpServersVar.getDynamicObject();
    DynamicObject* aetherhelmEntry = new DynamicObject();
    aetherhelmEntry->setProperty("command", exePath);
    aetherhelmEntry->setProperty("args", Array<var>());

    mcpServers->setProperty("aetherhelm", aetherhelmEntry);

    configFile.replaceWithText(JSON::toString(configVar, true));
    std::cout << "Successfully registered AetherHelm MCP server with " << configName.toStdString() << "\n";
    return true;
  };

  // 1. Claude Desktop
  if (claude) {
#if defined(_WIN32)
    File claudeDir = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Claude");
#elif defined(__APPLE__)
    File claudeDir = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Application Support/Claude");
#else
    File claudeDir = File::getSpecialLocation(File::userHomeDirectory).getChildFile(".config/Claude");
#endif
    File claudeConfig = claudeDir.getChildFile("claude_desktop_config.json");
    registerMcpInConfig(claudeConfig, "Claude Desktop");
  }

  // 2. Cursor
  if (cursor) {
    File cursorDir = File::getSpecialLocation(File::userHomeDirectory).getChildFile(".cursor");
    File cursorConfig = cursorDir.getChildFile("mcp.json");
    registerMcpInConfig(cursorConfig, "Cursor");
  }

  // 3. Windsurf
  if (windsurf) {
    File windsurfDir = File::getSpecialLocation(File::userHomeDirectory).getChildFile(".codeium/windsurf");
    File windsurfConfig = windsurfDir.getChildFile("mcp_config.json");
    registerMcpInConfig(windsurfConfig, "Windsurf");
  }

  // 4. Cline / Roo-Code
  if (cline) {
#if defined(_WIN32)
    File clineDir = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Code/User/globalStorage/saoudrizwan.claude-dev/settings");
    File rooDir = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Code/User/globalStorage/rooveterinaryinc.roo-cline/settings");
#elif defined(__APPLE__)
    File appSupport = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Application Support");
    File clineDir = appSupport.getChildFile("Code/User/globalStorage/saoudrizwan.claude-dev/settings");
    File rooDir = appSupport.getChildFile("Code/User/globalStorage/rooveterinaryinc.roo-cline/settings");
#else
    File clineDir = File::getSpecialLocation(File::userHomeDirectory).getChildFile(".config/Code/User/globalStorage/saoudrizwan.claude-dev/settings");
    File rooDir = File::getSpecialLocation(File::userHomeDirectory).getChildFile(".config/Code/User/globalStorage/rooveterinaryinc.roo-cline/settings");
#endif
    registerMcpInConfig(clineDir.getChildFile("cline_mcp_settings.json"), "Cline");
    registerMcpInConfig(rooDir.getChildFile("cline_mcp_settings.json"), "Roo-Code");
  }

  return true;
}

std::string AetherHelmMcpServer::getToolsListJson() {
  return R"json({
    "tools": [
      {
        "name": "get_current_patch",
        "description": "Reads the active preset configuration and modulation mappings of the AetherHelm synthesizer, supporting both flat and structured/hierarchical layouts.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "layout": {
              "type": "string",
              "enum": ["flat", "structured", "hierarchical"],
              "description": "Layout format: 'flat' (default) returns all parameters under 'settings', while 'structured' (or 'hierarchical') groups parameters into modules (oscillators, filter, envelopes, modulators, effects, global)."
            },
            "hierarchical": {
              "type": "boolean",
              "description": "Optional boolean shortcut: true for structured/hierarchical, false for flat."
            }
          }
        }
      },
      {
        "name": "set_patch_parameters",
        "description": "Granular and batch synthesizer parameter adjustment. Supports flat settings dictionaries, nested hierarchical module trees ('filter', 'oscillators', 'effects', etc.), parameter alias resolution (e.g. 'filter_cutoff' -> 'cutoff', 'reverb_size' -> 'reverb_feedback'), and automatic range clamping to safe DSP bounds.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "patch_or_params": {
              "type": "object",
              "description": "JSON object of parameter values or hierarchical module trees (or complete preset patch)."
            },
            "parameters": {
              "type": "object",
              "description": "Alternative parameter dictionary key."
            },
            "settings": {
              "type": "object",
              "description": "Alternative flat settings dictionary key."
            }
          }
        }
      },
      {
        "name": "list_available_parameters",
        "description": "Returns comprehensive parameter documentation and schema discovery for all addressable synth controls, including min/max valid bounds, default values, display units, functional descriptions, and module categories.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "category": {
              "type": "string",
              "enum": ["all", "oscillators", "filter", "envelopes", "modulators", "effects", "global"],
              "description": "Optional filter by module category (default: 'all')."
            },
            "include_details": {
              "type": "boolean",
              "description": "Whether to return full schema details (min, max, defaults, descriptions) or concise IDs (default: true)."
            }
          }
        }
      },
      {
        "name": "get_parameter_details",
        "description": "Inspects specific parameters with alias resolution, providing current value, valid min/max ranges, defaults, display units, functional description, and module category.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "parameter": {
              "type": "string",
              "description": "Parameter name or alias to query (e.g. 'cutoff', 'filter_cutoff', 'osc_1_unison_voices', 'reverb_feedback')."
            },
            "parameters": {
              "type": "array",
              "items": { "type": "string" },
              "description": "Optional list of parameter names/aliases to inspect simultaneously."
            }
          }
        }
      },
      {
        "name": "add_modulation",
        "description": "Routes a modulation source to any synthesis destination parameter with alias resolution and bipolar depth clamping (-1.0 to 1.0).",
        "inputSchema": {
          "type": "object",
          "properties": {
            "source": {
              "type": "string",
              "description": "Modulation source name or alias (e.g. 'mono_lfo_1', 'poly_lfo', 'mod_envelope', 'velocity', 'note', 'mod_wheel', 'random', 'step_sequencer')."
            },
            "destination": {
              "type": "string",
              "description": "Destination parameter name or alias (e.g. 'cutoff', 'filter_blend', 'osc_1_tune', 'resonance')."
            },
            "amount": {
              "type": "number",
              "description": "Modulation depth between -1.0 and +1.0."
            }
          },
          "required": ["source", "destination", "amount"]
        }
      },
      {
        "name": "remove_modulation",
        "description": "Disconnects an existing modulation routing connection between a source and a destination parameter.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "source": {
              "type": "string",
              "description": "Modulation source name or alias (e.g. 'mod_envelope', 'mono_lfo_1')."
            },
            "destination": {
              "type": "string",
              "description": "Destination parameter name or alias (e.g. 'cutoff', 'filter_blend')."
            }
          },
          "required": ["source", "destination"]
        }
      },
      {
        "name": "get_modulation_matrix",
        "description": "Queries all active modulation connections in the synthesizer along with all available modulation sources, filterable by source or destination.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "source": {
              "type": "string",
              "description": "Optional source filter."
            },
            "destination": {
              "type": "string",
              "description": "Optional destination filter."
            }
          }
        }
      },
      {
        "name": "trigger_preview_note",
        "description": "Synthesizes an audition MIDI note to preview audio output, returning peak amplitude, peak dBFS, RMS power, RMS dBFS, clipping detection, and signal detection metrics.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "note_number": {
              "type": "integer",
              "description": "MIDI note number 0-127 (e.g. 60 for Middle C). Default is 60."
            },
            "velocity": {
              "type": "number",
              "description": "MIDI velocity 0.0 to 1.0. Default is 0.8."
            },
            "duration_seconds": {
              "type": "number",
              "description": "Audition note duration in seconds (0.05 to 5.0). Default is 1.0."
            }
          }
        }
      },
      {
        "name": "morph_patch",
        "description": "Smoothly interpolates parameters between current synthesizer state and a target patch or settings object using a blend factor (0.0 = current patch, 1.0 = target patch).",
        "inputSchema": {
          "type": "object",
          "properties": {
            "target_patch": {
              "type": "object",
              "description": "Target patch JSON object or settings dictionary."
            },
            "factor": {
              "type": "number",
              "description": "Morph blend factor between 0.0 (no change) and 1.0 (fully target)."
            }
          },
          "required": ["target_patch", "factor"]
        }
      },
      {
        "name": "randomize_section",
        "description": "Randomizes synthesizer parameters within a specified module section ('filter', 'oscillators', 'envelopes', 'modulators', 'effects', or 'all') by a controllable intensity factor, keeping audio levels and critical DSP bounds safe.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "section": {
              "type": "string",
              "enum": ["all", "oscillators", "filter", "envelopes", "modulators", "effects"],
              "description": "Synthesizer section or category to randomize. Default is 'all'."
            },
            "intensity": {
              "type": "number",
              "description": "Randomization intensity from 0.0 (subtle deviation) to 1.0 (full random). Default is 0.5."
            }
          }
        }
      },
      {
        "name": "describe_patch",
        "description": "Analyzes the current active patch state, synthesizing a sonic character profile, timbre classification (Lead, Bass, Pad, Pluck), oscillator setup, filtering characteristics, and modulation summary.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "verbose": {
              "type": "boolean",
              "description": "Whether to include detailed descriptions. Default is false."
            }
          }
        }
      },
      {
        "name": "validate_patch",
        "description": "Validates a candidate patch JSON object against AetherHelm schema, verifying parameter ranges, resolving aliases, and detecting non-finite floats (NaN/Inf) or invalid modulation routes.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "patch": {
              "type": "object",
              "description": "Candidate patch JSON object to validate."
            }
          },
          "required": ["patch"]
        }
      }
    ]
  })json";
}

void AetherHelmMcpServer::sendResponse(const std::string& id, const std::string& resultJson) {
  std::cout << "{\"jsonrpc\": \"2.0\", \"id\": " << id << ", \"result\": " << resultJson << "}\n" << std::flush;
}

void AetherHelmMcpServer::sendError(const std::string& id, int code, const std::string& message) {
  std::cout << "{\"jsonrpc\": \"2.0\", \"id\": " << id << ", \"error\": {\"code\": " << code << ", \"message\": \"" << message << "\"}}\n" << std::flush;
}

void AetherHelmMcpServer::handleJsonRpcMessage(const std::string& msg) {
  var req;
  Result r = JSON::parse(String(msg), req);
  if (r.failed() || !req.isObject())
    return;

  DynamicObject* obj = req.getDynamicObject();
  std::string method = obj->getProperty("method").toString().toStdString();
  var idVar = obj->getProperty("id");
  std::string idStr = idVar.isVoid() ? "null" : JSON::toString(idVar).toStdString();

  if (method == "initialize") {
    std::string initResp = R"({
      "protocolVersion": "2024-11-05",
      "capabilities": {
        "tools": {}
      },
      "serverInfo": {
        "name": "aetherhelm-mcp",
        "version": "1.0.0"
      }
    })";
    sendResponse(idStr, initResp);
  } else if (method == "ping") {
    sendResponse(idStr, "{}");
  } else if (method == "notifications/initialized") {
    // Client initialized notification
  } else if (method == "tools/list") {
    sendResponse(idStr, getToolsListJson());
  } else if (method == "tools/call") {
    var paramsVar = obj->getProperty("params");
    if (!paramsVar.isObject()) {
      sendError(idStr, -32602, "Invalid params object");
      return;
    }
    DynamicObject* paramsObj = paramsVar.getDynamicObject();
    std::string toolName = paramsObj->getProperty("name").toString().toStdString();
    var argsVar = paramsObj->getProperty("arguments");

    std::string resultText;

    if (toolName == "get_current_patch") {
      bool hier = false;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("hierarchical") && (bool)a->getProperty("hierarchical"))
          hier = true;
        if (a->hasProperty("layout")) {
          String layout = a->getProperty("layout").toString().toLowerCase();
          if (layout == "structured" || layout == "hierarchical")
            hier = true;
        }
      }
      resultText = getCurrentPatch(hier);
    } else if (toolName == "set_patch_parameters") {
      std::string argStr = argsVar.isObject() ? JSON::toString(argsVar).toStdString() : "{}";
      resultText = setPatchParameters(argStr);
    } else if (toolName == "list_available_parameters") {
      std::string cat = "all";
      bool details = true;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("category")) cat = a->getProperty("category").toString().toStdString();
        if (a->hasProperty("include_details")) details = (bool)a->getProperty("include_details");
      }
      resultText = listAvailableParameters(cat, details);
    } else if (toolName == "get_parameter_details") {
      std::string argStr = argsVar.isObject() ? JSON::toString(argsVar).toStdString() : "{}";
      resultText = getParameterDetails(argStr);
    } else if (toolName == "add_modulation") {
      std::string src, dest;
      float amt = 0.0f;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("source")) src = a->getProperty("source").toString().toStdString();
        if (a->hasProperty("destination")) dest = a->getProperty("destination").toString().toStdString();
        if (a->hasProperty("amount")) amt = static_cast<float>(a->getProperty("amount"));
      }
      resultText = addModulation(src, dest, amt);
    } else if (toolName == "remove_modulation") {
      std::string src, dest;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("source")) src = a->getProperty("source").toString().toStdString();
        if (a->hasProperty("destination")) dest = a->getProperty("destination").toString().toStdString();
      }
      resultText = removeModulation(src, dest);
    } else if (toolName == "get_modulation_matrix") {
      std::string srcFilter, destFilter;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("source")) srcFilter = a->getProperty("source").toString().toStdString();
        if (a->hasProperty("destination")) destFilter = a->getProperty("destination").toString().toStdString();
      }
      resultText = getModulationMatrix(srcFilter, destFilter);
    } else if (toolName == "trigger_preview_note") {
      int note = 60;
      float vel = 0.8f;
      float dur = 1.0f;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("note_number")) note = static_cast<int>(a->getProperty("note_number"));
        else if (a->hasProperty("note")) note = static_cast<int>(a->getProperty("note"));
        else if (a->hasProperty("midi_note")) note = static_cast<int>(a->getProperty("midi_note"));
        else if (a->hasProperty("pitch")) note = static_cast<int>(a->getProperty("pitch"));

        if (a->hasProperty("velocity")) vel = static_cast<float>(a->getProperty("velocity"));
        else if (a->hasProperty("vel")) vel = static_cast<float>(a->getProperty("vel"));

        if (a->hasProperty("duration_seconds")) dur = static_cast<float>(a->getProperty("duration_seconds"));
        else if (a->hasProperty("duration")) dur = static_cast<float>(a->getProperty("duration"));
        else if (a->hasProperty("seconds")) dur = static_cast<float>(a->getProperty("seconds"));
        else if (a->hasProperty("sec")) dur = static_cast<float>(a->getProperty("sec"));
      }
      resultText = triggerPreviewNote(note, vel, dur);
    } else if (toolName == "morph_patch") {
      std::string targetJson = "{}";
      float factor = 0.5f;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("target_patch")) targetJson = JSON::toString(a->getProperty("target_patch")).toStdString();
        else if (a->hasProperty("patch")) targetJson = JSON::toString(a->getProperty("patch")).toStdString();
        else targetJson = JSON::toString(argsVar).toStdString();
        if (a->hasProperty("factor")) factor = static_cast<float>(a->getProperty("factor"));
      }
      resultText = morphPatch(targetJson, factor);
    } else if (toolName == "randomize_section") {
      std::string sec = "all";
      float intensity = 0.5f;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("section")) sec = a->getProperty("section").toString().toStdString();
        if (a->hasProperty("intensity")) intensity = static_cast<float>(a->getProperty("intensity"));
      }
      resultText = randomizeSection(sec, intensity);
    } else if (toolName == "describe_patch") {
      bool verbose = false;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("verbose")) verbose = (bool)a->getProperty("verbose");
      }
      resultText = describePatch(verbose);
    } else if (toolName == "validate_patch") {
      std::string candJson = "{}";
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("patch")) candJson = JSON::toString(a->getProperty("patch")).toStdString();
        else candJson = JSON::toString(argsVar).toStdString();
      }
      resultText = validatePatch(candJson);
    } else {
      sendError(idStr, -32601, "Unknown tool: " + toolName);
      return;
    }

    DynamicObject* callResp = new DynamicObject();
    Array<var> contentArray;
    DynamicObject* contentItem = new DynamicObject();
    contentItem->setProperty("type", "text");
    contentItem->setProperty("text", String(resultText));
    contentArray.add(contentItem);
    callResp->setProperty("content", contentArray);

    if (resultText.find("\"success\": false") != std::string::npos ||
        resultText.find("\"success\":false") != std::string::npos) {
      callResp->setProperty("isError", true);
    }

    sendResponse(idStr, JSON::toString(callResp, false).toStdString());
  } else {
    if (!idVar.isVoid())
      sendError(idStr, -32601, "Method not supported: " + method);
  }
}

void AetherHelmMcpServer::runStdioLoop() {
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    handleJsonRpcMessage(line);
  }
}
