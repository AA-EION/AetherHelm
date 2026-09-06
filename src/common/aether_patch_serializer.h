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
  static bool loadPatchFromJson(SynthBase* synth, const std::string& jsonString, std::string* error = nullptr);

  static var stateToVar(SynthBase* synth, bool hierarchical = false);
  static bool varToState(SynthBase* synth, const var& parsedState, std::string* error = nullptr);

  static std::string getParameterDocumentationJson();

private:
  static void parseHierarchicalSection(SynthBase* synth, const var& sectionVar, const std::string& prefix = "");
  static void applyControl(SynthBase* synth, const std::string& name, mopo::mopo_float value);
  static void applyModulation(SynthBase* synth, const std::string& source, const std::string& dest, mopo::mopo_float amount);
};
