#pragma once

#include <string>
#include <functional>
#include <vector>
#include "JuceHeader.h"

class SynthBase;

class OpenRouterClient : public Thread {
public:
  struct RequestData {
    SynthBase* synth = nullptr;
    std::string userPrompt;
    std::string model;
    std::function<void(bool success, const std::string& message)> callback;
  };

  OpenRouterClient();
  ~OpenRouterClient() override;

  static OpenRouterClient* instance();

  static String getApiKey();
  static void setApiKey(const String& apiKey);

  static std::vector<std::string> getAvailableModels();

  void requestPatchGeneration(SynthBase* synth,
                              const std::string& userPrompt,
                              const std::string& model,
                              std::function<void(bool success, const std::string& message)> callback);

  void cancelPendingFor(SynthBase* synth);

  bool isBusy() const { return isThreadRunning(); }

private:
  void run() override;

  CriticalSection request_lock_;
  RequestData current_request_;

  static std::string buildSystemPrompt();
  static std::string extractJsonContent(const std::string& rawResponse);
};
