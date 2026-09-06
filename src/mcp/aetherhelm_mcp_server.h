#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "JuceHeader.h"
#include "helm_engine.h"
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

  // Headless audio synth engine for MCP
  mopo::HelmEngine engine_;
  mopo::control_map controls_;
  std::set<mopo::ModulationConnection*> mod_connections_;
  mopo::ModulationConnectionBank modulation_bank_;
  std::string patch_name_;
  std::string author_;

  void initHeadlessSynth();
  void applyHeadlessControl(const std::string& name, float value);
};
