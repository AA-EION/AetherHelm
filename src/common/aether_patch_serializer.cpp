#include "aether_patch_serializer.h"
#include "synth_base.h"
#include "synth_gui_interface.h"
#include <algorithm>

void AetherPatchSerializer::applyControl(SynthBase* synth, const std::string& name, mopo::mopo_float value) {
  if (!mopo::Parameters::isParameter(name))
    return;

  mopo::ValueDetails details = mopo::Parameters::getDetails(name);
  mopo::mopo_float minVal = static_cast<mopo::mopo_float>(details.min);
  mopo::mopo_float maxVal = static_cast<mopo::mopo_float>(details.max);
  mopo::mopo_float clamped = std::clamp<mopo::mopo_float>(value, minVal, maxVal);
  synth->valueChangedInternal(name, clamped);
}

void AetherPatchSerializer::applyModulation(SynthBase* synth, const std::string& source,
                                           const std::string& dest, mopo::mopo_float amount) {
  if (source.empty() || dest.empty())
    return;
  if (!mopo::Parameters::isParameter(dest))
    return;

  mopo::mopo_float clamped = std::clamp<mopo::mopo_float>(amount, -1.0f, 1.0f);
  synth->changeModulationAmount(source, dest, clamped);
}

void AetherPatchSerializer::parseHierarchicalSection(SynthBase* synth, const var& sectionVar, const std::string& prefix) {
  if (!sectionVar.isObject())
    return;

  DynamicObject* obj = sectionVar.getDynamicObject();
  NamedValueSet props = obj->getProperties();

  for (int i = 0; i < props.size(); ++i) {
    Identifier id = props.getName(i);
    var val = props.getValueAt(i);
    std::string key = id.toString().toStdString();

    if (val.isObject()) {
      std::string newPrefix = prefix.empty() ? key : prefix + "_" + key;
      parseHierarchicalSection(synth, val, newPrefix);
    } else if (val.isDouble() || val.isInt() || val.isInt64() || val.isBool()) {
      mopo::mopo_float numVal = static_cast<mopo::mopo_float>(val);
      std::string paramName = prefix.empty() ? key : prefix + "_" + key;

      // Handle common schema alias mappings
      if (paramName == "filter_env_depth") paramName = "fil_env_depth";
      else if (paramName == "filter_attack") paramName = "fil_attack";
      else if (paramName == "filter_decay") paramName = "fil_decay";
      else if (paramName == "filter_sustain") paramName = "fil_sustain";
      else if (paramName == "filter_release") paramName = "fil_release";

      applyControl(synth, paramName, numVal);
    }
  }
}

std::string AetherPatchSerializer::exportPatchToJson(SynthBase* synth, bool hierarchical) {
  var state = stateToVar(synth, hierarchical);
  return JSON::toString(state, true).toStdString();
}

var AetherPatchSerializer::stateToVar(SynthBase* synth, bool hierarchical) {
  DynamicObject* root = new DynamicObject();
  root->setProperty("synth_name", "AetherHelm");
  root->setProperty("synth_version", ProjectInfo::versionString);
  root->setProperty("patch_name", synth->getPatchName());
  root->setProperty("author", synth->getAuthor());
  root->setProperty("folder_name", synth->getFolderName());

  mopo::control_map controls = synth->getControls();

  if (!hierarchical) {
    DynamicObject* settings = new DynamicObject();
    for (const auto& pair : controls) {
      if (pair.second)
        settings->setProperty(String(pair.first), pair.second->value());
    }
    root->setProperty("settings", settings);
  } else {
    // Hierarchical layout
    DynamicObject* oscObj = new DynamicObject();
    DynamicObject* filObj = new DynamicObject();
    DynamicObject* envObj = new DynamicObject();
    DynamicObject* modObj = new DynamicObject();
    DynamicObject* fxObj = new DynamicObject();
    DynamicObject* globalObj = new DynamicObject();

    for (const auto& pair : controls) {
      if (!pair.second) continue;
      String key(pair.first);
      mopo::mopo_float v = pair.second->value();

      if (key.startsWith("osc_") || key.startsWith("sub_") || key == "cross_modulation" || key.startsWith("noise_"))
        oscObj->setProperty(key, v);
      else if (key.startsWith("fil_") || key.startsWith("filter_") || key.startsWith("formant_") || key == "cutoff" || key == "resonance" || key == "keytrack")
        filObj->setProperty(key, v);
      else if (key.startsWith("amp_") || key.startsWith("mod_"))
        envObj->setProperty(key, v);
      else if (key.startsWith("mono_lfo_") || key.startsWith("poly_lfo_") || key.startsWith("step_"))
        modObj->setProperty(key, v);
      else if (key.startsWith("distortion_") || key.startsWith("delay_") || key.startsWith("reverb_") || key.startsWith("stutter_"))
        fxObj->setProperty(key, v);
      else
        globalObj->setProperty(key, v);
    }

    root->setProperty("oscillators", oscObj);
    root->setProperty("filter", filObj);
    root->setProperty("envelopes", envObj);
    root->setProperty("modulators", modObj);
    root->setProperty("effects", fxObj);
    root->setProperty("global", globalObj);
  }

  // Modulations
  std::set<mopo::ModulationConnection*> modulations = synth->getModulationConnections();
  Array<var> modArray;
  for (mopo::ModulationConnection* connection : modulations) {
    if (!connection || connection->amount.value() == 0.0f)
      continue;
    DynamicObject* mod = new DynamicObject();
    mod->setProperty("source", String(connection->source));
    mod->setProperty("destination", String(connection->destination));
    mod->setProperty("amount", connection->amount.value());
    modArray.add(mod);
  }
  root->setProperty("modulations", modArray);

  return root;
}

bool AetherPatchSerializer::loadPatchFromJson(SynthBase* synth, const std::string& jsonString, std::string* error) {
  var parsedState;
  Result res = JSON::parse(String(jsonString), parsedState);
  if (res.failed()) {
    if (error) *error = "JSON parse error: " + res.getErrorMessage().toStdString();
    return false;
  }

  return varToState(synth, parsedState, error);
}

bool AetherPatchSerializer::varToState(SynthBase* synth, const var& parsedState, std::string* error) {
  if (!parsedState.isObject()) {
    if (error) *error = "JSON root must be an object";
    return false;
  }

  DynamicObject* root = parsedState.getDynamicObject();
  NamedValueSet props = root->getProperties();

  if (props.contains("patch_name"))
    synth->setPatchName(props["patch_name"].toString());
  if (props.contains("author"))
    synth->setAuthor(props["author"].toString());
  if (props.contains("folder_name"))
    synth->setFolderName(props["folder_name"].toString());

  // Check for flat 'settings' object (standard Helm format)
  if (props.contains("settings") && props["settings"].isObject()) {
    DynamicObject* settings = props["settings"].getDynamicObject();
    NamedValueSet settingProps = settings->getProperties();
    for (int i = 0; i < settingProps.size(); ++i) {
      std::string key = settingProps.getName(i).toString().toStdString();
      if (key == "modulations") continue;
      var val = settingProps.getValueAt(i);
      if (val.isDouble() || val.isInt() || val.isInt64())
        applyControl(synth, key, static_cast<mopo::mopo_float>(val));
    }
  }

  // Check for hierarchical sections
  const char* sections[] = { "oscillators", "filter", "envelopes", "modulators", "effects", "global" };
  for (const char* sectionName : sections) {
    if (props.contains(sectionName))
      parseHierarchicalSection(synth, props[sectionName], "");
  }

  // Check for direct properties at root level
  for (int i = 0; i < props.size(); ++i) {
    std::string key = props.getName(i).toString().toStdString();
    if (mopo::Parameters::isParameter(key)) {
      var val = props.getValueAt(i);
      if (val.isDouble() || val.isInt() || val.isInt64())
        applyControl(synth, key, static_cast<mopo::mopo_float>(val));
    }
  }

  // Modulations
  const Array<var>* modulations = nullptr;
  if (props.contains("modulations") && props["modulations"].isArray())
    modulations = props["modulations"].getArray();
  else if (props.contains("settings") && props["settings"].isObject()) {
    DynamicObject* settings = props["settings"].getDynamicObject();
    if (settings->hasProperty("modulations") && settings->getProperty("modulations").isArray())
      modulations = settings->getProperty("modulations").getArray();
  }

  if (modulations) {
    synth->clearModulations();
    for (const var& modVar : *modulations) {
      if (!modVar.isObject()) continue;
      DynamicObject* mod = modVar.getDynamicObject();
      std::string source = mod->getProperty("source").toString().toStdString();
      std::string dest = mod->getProperty("destination").toString().toStdString();
      mopo::mopo_float amount = static_cast<mopo::mopo_float>(mod->getProperty("amount"));
      applyModulation(synth, source, dest, amount);
    }
  }

  return true;
}

std::string AetherPatchSerializer::getParameterDocumentationJson() {
  DynamicObject* doc = new DynamicObject();
  std::map<std::string, mopo::ValueDetails> allDetails = mopo::Parameters::lookup_.getAllDetails();

  for (const auto& pair : allDetails) {
    DynamicObject* item = new DynamicObject();
    item->setProperty("min", pair.second.min);
    item->setProperty("max", pair.second.max);
    item->setProperty("default", pair.second.default_value);
    item->setProperty("units", String(pair.second.display_units));
    item->setProperty("description", String(pair.second.display_name));
    doc->setProperty(String(pair.first), item);
  }

  return JSON::toString(doc, false).toStdString();
}
