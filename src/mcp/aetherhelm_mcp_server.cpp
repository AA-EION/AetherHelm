#include "aetherhelm_mcp_server.h"
#include "aether_patch_serializer.h"
#include "openrouter_client.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

AetherHelmMcpServer::AetherHelmMcpServer() = default;

AetherHelmMcpServer::~AetherHelmMcpServer() = default;

std::string AetherHelmMcpServer::getCurrentPatch() {
  return synth_.exportPatchToJson(false);
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

  std::string targetJson = JSON::toString(targetVar, false).toStdString();
  std::string err;
  int updatedCount = 0;
  bool ok = synth_.loadPatchFromJson(targetJson, &err, &updatedCount);
  if (!ok) {
    DynamicObject* errResp = new DynamicObject();
    errResp->setProperty("success", false);
    errResp->setProperty("error", String(err));
    return JSON::toString(errResp, false).toStdString();
  }

  DynamicObject* resp = new DynamicObject();
  resp->setProperty("success", true);
  resp->setProperty("parameters_updated", updatedCount);
  resp->setProperty("patch_name", synth_.getPatchName());
  return JSON::toString(resp, false).toStdString();
}

std::string AetherHelmMcpServer::listAvailableParameters() {
  DynamicObject* root = new DynamicObject();
  std::map<std::string, mopo::ValueDetails> allDetails = mopo::Parameters::lookup_.getAllDetails();

  Array<var> paramList;
  for (const auto& pair : allDetails) {
    DynamicObject* item = new DynamicObject();
    item->setProperty("id", String(pair.first));
    item->setProperty("name", String(pair.second.display_name));
    item->setProperty("min", pair.second.min);
    item->setProperty("max", pair.second.max);
    item->setProperty("default", pair.second.default_value);
    item->setProperty("units", String(pair.second.display_units));
    paramList.add(item);
  }
  root->setProperty("parameters", paramList);
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

  DynamicObject* report = new DynamicObject();
  report->setProperty("status", "preview_rendered");
  report->setProperty("midi_note", noteNumber);
  report->setProperty("velocity", velocity);
  report->setProperty("duration_seconds", durationSeconds);
  report->setProperty("peak_amplitude", peakVal);
  report->setProperty("rms_level", rms);
  report->setProperty("signal_detected", peakVal > 0.0001f);

  return JSON::toString(report, true).toStdString();
}

std::string AetherHelmMcpServer::generatePatchFromPrompt(const std::string& prompt, const std::string& model) {
  String apiKey = OpenRouterClient::getApiKey();
  if (apiKey.isEmpty())
    return "{\"success\": false, \"error\": \"OpenRouter API Key not configured.\"}";

  std::string currentPatch = getCurrentPatch();

  DynamicObject* payload = new DynamicObject();
  payload->setProperty("model", model.empty() ? "anthropic/claude-3.5-sonnet" : String(model));

  Array<var> messages;
  DynamicObject* sysMsg = new DynamicObject();
  sysMsg->setProperty("role", "system");
  sysMsg->setProperty("content", "You are an expert synthesizer patch designer for AetherHelm. Return ONLY valid JSON with updated synthesizer settings.");
  messages.add(sysMsg);

  DynamicObject* userMsg = new DynamicObject();
  userMsg->setProperty("role", "user");
  userMsg->setProperty("content", "Current patch:\n" + String(currentPatch) + "\n\nUser prompt: " + String(prompt));
  messages.add(userMsg);

  payload->setProperty("messages", messages);

  URL url("https://openrouter.ai/api/v1/chat/completions");
  URL postUrl = url.withPOSTData(JSON::toString(payload));
  String headers = "Authorization: Bearer " + apiKey + "\nContent-Type: application/json\n";

  int statusCode = 0;
  String respText;
#if defined(JUCE_MAJOR_VERSION) && (JUCE_MAJOR_VERSION >= 6)
  std::unique_ptr<InputStream> stream(postUrl.createInputStream(
    URL::InputStreamOptions(URL::ParameterHandling::inPostData)
      .withExtraHeaders(headers)
      .withConnectionTimeoutMs(25000)
      .withStatusCode(&statusCode)
  ));
#else
  std::unique_ptr<InputStream> stream(postUrl.createInputStream(
    true, nullptr, nullptr, headers, 25000, nullptr, &statusCode
  ));
#endif

  if (stream)
    respText = stream->readEntireStreamAsString();

  if (statusCode != 200 || respText.isEmpty())
    return "{\"success\": false, \"error\": \"OpenRouter request failed.\"}";

  var parsedResp;
  if (JSON::parse(respText, parsedResp).failed())
    return "{\"success\": false, \"error\": \"Failed to parse response.\"}";

  String content;
  DynamicObject* respObj = parsedResp.getDynamicObject();
  if (respObj->hasProperty("choices") && respObj->getProperty("choices").isArray()) {
    Array<var>* choices = respObj->getProperty("choices").getArray();
    if (choices && choices->size() > 0) {
      DynamicObject* choice = (*choices)[0].getDynamicObject();
      if (choice && choice->hasProperty("message")) {
        DynamicObject* msg = choice->getProperty("message").getDynamicObject();
        if (msg && msg->hasProperty("content"))
          content = msg->getProperty("content").toString();
      }
    }
  }

  std::string jsonSub = AetherPatchSerializer::extractJsonFromText(content.toStdString());
  if (!jsonSub.empty() && jsonSub.front() == '{' && jsonSub.back() == '}') {
    setPatchParameters(jsonSub);
    return "{\"success\": true, \"patch\": " + jsonSub + "}";
  }

  return "{\"success\": false, \"error\": \"No valid JSON patch returned.\"}";
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
        "description": "Reads the active preset configuration and modulation mappings of AetherHelm synthesizer.",
        "inputSchema": {
          "type": "object",
          "properties": {}
        }
      },
      {
        "name": "set_patch_parameters",
        "description": "Applies granular parameter changes or loads a complete patch JSON into AetherHelm.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "patch_or_params": {
              "type": "object",
              "description": "JSON dictionary of parameters and their numeric values, or full patch object."
            }
          },
          "required": ["patch_or_params"]
        }
      },
      {
        "name": "list_available_parameters",
        "description": "Returns all addressable modulation parameters, valid min/max ranges, defaults, and units.",
        "inputSchema": {
          "type": "object",
          "properties": {}
        }
      },
      {
        "name": "trigger_preview_note",
        "description": "Triggers a MIDI audition note to synthesize audio and inspect peak/RMS signal levels.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "note_number": { "type": "integer", "description": "MIDI note 0-127 (e.g. 60 for Middle C)" },
            "velocity": { "type": "number", "description": "MIDI velocity 0.0 to 1.0" },
            "duration_seconds": { "type": "number", "description": "Audition note duration in seconds" }
          },
          "required": ["note_number"]
        }
      },
      {
        "name": "generate_patch_from_prompt",
        "description": "Generates a complete sound design preset from a natural language prompt via OpenRouter.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "prompt": { "type": "string", "description": "Sound description, e.g. 'Cyberpunk reese bass'" },
            "model": { "type": "string", "description": "OpenRouter model name (e.g. 'anthropic/claude-3.5-sonnet')" }
          },
          "required": ["prompt"]
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
      resultText = getCurrentPatch();
    } else if (toolName == "set_patch_parameters") {
      std::string argStr = argsVar.isObject() ? JSON::toString(argsVar).toStdString() : "{}";
      resultText = setPatchParameters(argStr);
    } else if (toolName == "list_available_parameters") {
      resultText = listAvailableParameters();
    } else if (toolName == "trigger_preview_note") {
      int note = 60;
      float vel = 0.8f;
      float dur = 1.0f;
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("note_number")) note = static_cast<int>(a->getProperty("note_number"));
        if (a->hasProperty("velocity")) vel = static_cast<float>(a->getProperty("velocity"));
        if (a->hasProperty("duration_seconds")) dur = static_cast<float>(a->getProperty("duration_seconds"));
      }
      resultText = triggerPreviewNote(note, vel, dur);
    } else if (toolName == "generate_patch_from_prompt") {
      std::string prompt;
      std::string model = "anthropic/claude-3.5-sonnet";
      if (argsVar.isObject()) {
        DynamicObject* a = argsVar.getDynamicObject();
        if (a->hasProperty("prompt")) prompt = a->getProperty("prompt").toString().toStdString();
        if (a->hasProperty("model")) model = a->getProperty("model").toString().toStdString();
      }
      resultText = generatePatchFromPrompt(prompt, model);
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
