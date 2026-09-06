#pragma once

#include <string>
#include <vector>
#include <map>
#include "JuceHeader.h"
#include "helm_common.h"

class SynthBase;

class AetherPatchSerializer {
public:
  static std::string exportPatchToJson(SynthBase* synth, bool hierarchical = false);
  static bool loadPatchFromJson(SynthBase* synth, const std::string& jsonString,
                                std::string* error = nullptr, int* updatedCount = nullptr);

  static var stateToVar(SynthBase* synth, bool hierarchical = false);
  static bool varToState(SynthBase* synth, const var& parsedState,
                         std::string* error = nullptr, int* updatedCount = nullptr);

  static std::string getParameterDocumentationJson();
  static std::string extractJsonFromText(const std::string& raw);

  // Parameter and Modulation resolution helpers
  static std::string resolveParameterName(const std::string& key, const std::string& prefix = "");
  static std::string resolveModulationSourceName(const std::string& source);
  static bool isValidModulationSource(const std::string& source);
  static const std::vector<std::string>& getAvailableModulationSources();
  static std::string getParameterCategory(const std::string& parameterId);
  static std::string getParameterDescription(const std::string& parameterId);

  static void applyControl(SynthBase* synth, const std::string& name,
                           mopo::mopo_float value, int* updatedCount = nullptr);
  static void applyModulation(SynthBase* synth, const std::string& source,
                              const std::string& dest, mopo::mopo_float amount, int* updatedCount = nullptr);

private:
  static void parseHierarchicalSection(SynthBase* synth, const var& sectionVar,
                                       const std::string& prefix = "", int* updatedCount = nullptr);
};
