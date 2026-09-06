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

private:
  static void parseHierarchicalSection(SynthBase* synth, const var& sectionVar,
                                       const std::string& prefix = "", int* updatedCount = nullptr);
  static void applyControl(SynthBase* synth, const std::string& name,
                           mopo::mopo_float value, int* updatedCount = nullptr);
  static void applyModulation(SynthBase* synth, const std::string& source,
                              const std::string& dest, mopo::mopo_float amount, int* updatedCount = nullptr);
};
