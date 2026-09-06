#include "aetherhelm_mcp_server.h"
#include "aether_patch_serializer.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

AetherHelmMcpServer::AetherHelmMcpServer() = default;

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
