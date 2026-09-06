#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "JuceHeader.h"
#include "headless_synth.h"
#include "helm_common.h"

class AetherHelmMcpServer {
public:
  AetherHelmMcpServer();
  ~AetherHelmMcpServer();

  void runStdioLoop();

  // First-class MCP synthesis, inspection, and audition tools
  std::string getCurrentPatch(bool hierarchical = false);
  std::string setPatchParameters(const std::string& patchOrParamsJson);
  std::string listAvailableParameters(const std::string& category = "all", bool includeDetails = true);
  std::string getParameterDetails(const std::string& paramNamesJson);
  std::string addModulation(const std::string& source, const std::string& destination, float amount);
  std::string removeModulation(const std::string& source, const std::string& destination);
  std::string getModulationMatrix(const std::string& filterSource = "", const std::string& filterDest = "");
  std::string triggerPreviewNote(int noteNumber = 60, float velocity = 0.8f, float durationSeconds = 1.0f);

  // JSON-RPC and MCP protocol helpers
  void handleJsonRpcMessage(const std::string& message);
  std::string getToolsListJson();

  // Desktop AI Auto-configuration
  static bool installDesktopConfigs(bool claude = true, bool cursor = true,
                                    bool windsurf = true, bool cline = true);

private:
  void sendResponse(const std::string& id, const std::string& resultJson);
  void sendError(const std::string& id, int code, const std::string& message);

  // Unified headless engine backed by HeadlessSynth
  HeadlessSynth synth_;
};
