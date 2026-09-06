#include "openrouter_client.h"
#include "synth_base.h"
#include "load_save.h"
#include "aether_patch_serializer.h"
#include <regex>

OpenRouterClient::OpenRouterClient() : Thread("AetherHelmOpenRouterClient") {}

OpenRouterClient::~OpenRouterClient() {
  stopThread(5000);
}

OpenRouterClient* OpenRouterClient::instance() {
  static OpenRouterClient client;
  return &client;
}

String OpenRouterClient::getApiKey() {
  String envKey = SystemStats::getEnvironmentVariable("OPENROUTER_API_KEY", "");
  if (envKey.isNotEmpty())
    return envKey;

  var config = LoadSave::getConfigVar();
  if (config.isObject() && config.getDynamicObject()->hasProperty("openrouter_api_key"))
    return config.getDynamicObject()->getProperty("openrouter_api_key").toString();

  return "";
}

void OpenRouterClient::setApiKey(const String& apiKey) {
  var config = LoadSave::getConfigVar();
  if (!config.isObject())
    config = new DynamicObject();

  config.getDynamicObject()->setProperty("openrouter_api_key", apiKey);
  LoadSave::saveVarToConfig(config.getDynamicObject());
}

std::vector<std::string> OpenRouterClient::getAvailableModels() {
  return {
    "anthropic/claude-3.5-sonnet",
    "openai/gpt-4o",
    "deepseek/deepseek-coder",
    "meta-llama/llama-3.3-70b-instruct",
    "google/gemini-2.5-flash"
  };
}

void OpenRouterClient::requestPatchGeneration(SynthBase* synth,
                                             const std::string& userPrompt,
                                             const std::string& model,
                                             std::function<void(bool success, const std::string& message)> callback) {
  if (isThreadRunning()) {
    if (callback)
      callback(false, "An AI request is already in progress.");
    return;
  }

  {
    ScopedLock lock(request_lock_);
    current_request_.synth = synth;
    current_request_.userPrompt = userPrompt;
    current_request_.model = model.empty() ? "anthropic/claude-3.5-sonnet" : model;
    current_request_.callback = callback;
  }

  startThread();
}

std::string OpenRouterClient::buildSystemPrompt() {
  return "You are an expert audio synthesizer sound designer and DSP architect for the AetherHelm synthesizer.\n"
         "When given a user description of a sound, you must generate or modify synthesizer parameters to fulfill the sound design.\n"
         "You MUST respond ONLY with valid JSON. Do not include introductory text, explanations, or commentary.\n"
         "The JSON structure can either be the flat settings format:\n"
         "{\n"
         "  \"patch_name\": \"Cyber Bass\",\n"
         "  \"author\": \"AI Assistant\",\n"
         "  \"settings\": {\n"
         "    \"cutoff\": 45.0, \"resonance\": 0.6, \"filter_style\": 1, \"fil_env_depth\": 36.0,\n"
         "    \"osc_1_waveform\": 4, \"osc_1_volume\": 0.7, \"osc_1_unison_voices\": 3, \"osc_1_unison_detune\": 12,\n"
         "    \"osc_2_waveform\": 4, \"osc_2_volume\": 0.6, \"osc_2_transpose\": -12,\n"
         "    \"amp_attack\": 0.01, \"amp_decay\": 0.8, \"amp_sustain\": 0.7, \"amp_release\": 0.3,\n"
         "    \"fil_attack\": 0.01, \"fil_decay\": 0.4, \"fil_sustain\": 0.2, \"fil_release\": 0.3,\n"
         "    \"distortion_on\": 1, \"distortion_drive\": 8.0, \"distortion_mix\": 0.4,\n"
         "    \"delay_on\": 1, \"delay_sync\": 1, \"delay_tempo\": 9, \"delay_dry_wet\": 0.25,\n"
         "    \"reverb_on\": 1, \"reverb_dry_wet\": 0.3, \"reverb_feedback\": 0.85\n"
         "  },\n"
         "  \"modulations\": [\n"
         "    { \"source\": \"mod_envelope\", \"destination\": \"cutoff\", \"amount\": 0.45 },\n"
         "    { \"source\": \"mono_lfo_1\", \"destination\": \"filter_blend\", \"amount\": 0.3 }\n"
         "  ]\n"
         "}\n"
         "Available mod sources: 'mono_lfo_1', 'mono_lfo_2', 'poly_lfo', 'fil_envelope', 'mod_envelope', 'amp_envelope', 'velocity', 'note', 'mod_wheel', 'aftertouch'.\n"
         "Key parameters: cutoff (28..127), resonance (0..1), filter_style (0..2), fil_env_depth (-128..128), osc_1_waveform (0..10), osc_2_waveform (0..10), sub_volume (0..1), distortion_on (0..1), delay_on (0..1), reverb_on (0..1).\n";
}

std::string OpenRouterClient::extractJsonContent(const std::string& raw) {
  std::string s = raw;
  size_t start = s.find('{');
  size_t end = s.rfind('}');
  if (start != std::string::npos && end != std::string::npos && end > start)
    return s.substr(start, end - start + 1);
  return raw;
}

void OpenRouterClient::run() {
  RequestData req;
  {
    ScopedLock lock(request_lock_);
    req = current_request_;
  }

  String apiKey = getApiKey();
  if (apiKey.isEmpty()) {
    if (req.callback) {
      MessageManager::callAsync([cb = req.callback]() {
        cb(false, "OpenRouter API Key is missing. Please configure it in AetherHelm.");
      });
    }
    return;
  }

  std::string currentPatchJson = req.synth ? req.synth->exportPatchToJson(false) : "{}";

  DynamicObject* payload = new DynamicObject();
  payload->setProperty("model", String(req.model));

  Array<var> messages;
  DynamicObject* sysMsg = new DynamicObject();
  sysMsg->setProperty("role", "system");
  sysMsg->setProperty("content", String(buildSystemPrompt()));
  messages.add(sysMsg);

  DynamicObject* userMsg = new DynamicObject();
  userMsg->setProperty("role", "user");
  userMsg->setProperty("content", "Current synth patch:\n" + String(currentPatchJson) +
                                  "\n\nUser request: " + String(req.userPrompt) +
                                  "\nReturn ONLY the modified or generated patch JSON.");
  messages.add(userMsg);

  payload->setProperty("messages", messages);
  payload->setProperty("temperature", 0.7);

  String jsonBody = JSON::toString(payload);

  URL url("https://openrouter.ai/api/v1/chat/completions");
  URL postUrl = url.withPOSTData(jsonBody);

  String extraHeaders = "Authorization: Bearer " + apiKey + "\n"
                        "Content-Type: application/json\n"
                        "HTTP-Referer: https://github.com/AA-EION/AetherHelm\n"
                        "X-Title: AetherHelm Synthesizer\n";

  int statusCode = 0;
  String responseText;

  {
#if defined(JUCE_MAJOR_VERSION) && (JUCE_MAJOR_VERSION >= 6)
    std::unique_ptr<InputStream> stream(postUrl.createInputStream(
      URL::InputStreamOptions(URL::ParameterHandling::inPostData)
        .withExtraHeaders(extraHeaders)
        .withConnectionTimeoutMs(30000)
        .withStatusCode(&statusCode)
    ));
#else
    std::unique_ptr<InputStream> stream(postUrl.createInputStream(
      true, nullptr, nullptr, extraHeaders, 30000, nullptr, &statusCode
    ));
#endif

    if (stream)
      responseText = stream->readEntireStreamAsString();
    else
      responseText = "";
  }

  if (statusCode != 200 || responseText.isEmpty()) {
    std::string errMsg = "HTTP Request failed (code: " + std::to_string(statusCode) + ")";
    if (responseText.isNotEmpty())
      errMsg += ": " + responseText.toStdString();

    if (req.callback) {
      MessageManager::callAsync([cb = req.callback, errMsg]() {
        cb(false, errMsg);
      });
    }
    return;
  }

  var responseVar;
  Result parseRes = JSON::parse(responseText, responseVar);
  if (parseRes.failed() || !responseVar.isObject()) {
    if (req.callback) {
      MessageManager::callAsync([cb = req.callback]() {
        cb(false, "Failed to parse OpenRouter response as JSON.");
      });
    }
    return;
  }

  String contentText;
  DynamicObject* respObj = responseVar.getDynamicObject();
  if (respObj->hasProperty("choices") && respObj->getProperty("choices").isArray()) {
    Array<var>* choices = respObj->getProperty("choices").getArray();
    if (choices && choices->size() > 0) {
      var firstChoice = (*choices)[0];
      if (firstChoice.isObject() && firstChoice.getDynamicObject()->hasProperty("message")) {
        DynamicObject* msg = firstChoice.getDynamicObject()->getProperty("message").getDynamicObject();
        if (msg && msg->hasProperty("content"))
          contentText = msg->getProperty("content").toString();
      }
    }
  }

  if (contentText.isEmpty()) {
    if (req.callback) {
      MessageManager::callAsync([cb = req.callback]() {
        cb(false, "No patch content returned by OpenRouter.");
      });
    }
    return;
  }

  std::string patchJson = extractJsonContent(contentText.toStdString());
  std::string injectError;
  bool success = req.synth ? req.synth->loadPatchFromJson(patchJson, &injectError) : false;

  if (req.callback) {
    MessageManager::callAsync([cb = req.callback, success, injectError]() {
      if (success)
        cb(true, "AI sound patch loaded successfully!");
      else
        cb(false, "Failed to apply AI patch: " + injectError);
    });
  }
}
