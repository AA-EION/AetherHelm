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

  // Tool handlers
  std::string getCurrentPatch();
  std::string setPatchParameters(const std::string& patchOrParamsJson);
  std::string listAvailableParameters();
  std::string triggerPreviewNote(int noteNumber, float velocity, float durationSeconds);
  std::string generatePatchFromPrompt(const std::string& prompt, const std::string& model);

  // Desktop AI Auto-configuration
  static bool installDesktopConfigs(bool claude = true, bool cursor = true,
                                    bool windsurf = true, bool cline = true);

private:
  void handleJsonRpcMessage(const std::string& message);
  void sendResponse(const std::string& id, const std::string& resultJson);
  void sendError(const std::string& id, int code, const std::string& message);

  std::string getToolsListJson();

  // Unified headless engine backed by HeadlessSynth
  HeadlessSynth synth_;
};
