#include "aether_patch_serializer.h"
#include "synth_base.h"
#include "synth_gui_interface.h"
#include <algorithm>
#include <cmath>

const std::vector<std::string>& AetherPatchSerializer::getAvailableModulationSources() {
  static const std::vector<std::string> sources = {
    "mono_lfo_1",
    "mono_lfo_2",
    "poly_lfo",
    "mod_envelope",
    "fil_envelope",
    "amp_envelope",
    "step_sequencer",
    "velocity",
    "note",
    "mod_wheel",
    "aftertouch",
    "pitch_wheel",
    "random"
  };
  return sources;
}

bool AetherPatchSerializer::isValidModulationSource(const std::string& source) {
  std::string resolved = resolveModulationSourceName(source);
  const auto& sources = getAvailableModulationSources();
  return std::find(sources.begin(), sources.end(), resolved) != sources.end();
}

std::string AetherPatchSerializer::resolveModulationSourceName(const std::string& source) {
  std::string s = source;
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);

  if (s == "mono_lfo_1" || s == "mono_lfo1" || s == "lfo_1" || s == "lfo1" || s == "monolfo1")
    return "mono_lfo_1";
  if (s == "mono_lfo_2" || s == "mono_lfo2" || s == "lfo_2" || s == "lfo2" || s == "monolfo2")
    return "mono_lfo_2";
  if (s == "poly_lfo" || s == "polylfo" || s == "poly_lfo_1" || s == "lfo")
    return "poly_lfo";
  if (s == "mod_envelope" || s == "mod_env" || s == "mod_envelope_amp" || s == "modenv")
    return "mod_envelope";
  if (s == "fil_envelope" || s == "fil_env" || s == "filter_envelope" || s == "filter_env" || s == "fil_envelope_amp")
    return "fil_envelope";
  if (s == "amp_envelope" || s == "amp_env" || s == "amplitude_envelope" || s == "amp_envelope_amp")
    return "amp_envelope";
  if (s == "step_sequencer" || s == "step" || s == "sequencer" || s == "step_seq" || s == "arp")
    return "step_sequencer";
  if (s == "velocity" || s == "vel")
    return "velocity";
  if (s == "note" || s == "key" || s == "keytrack" || s == "pitch")
    return "note";
  if (s == "mod_wheel" || s == "modwheel" || s == "wheel" || s == "mod")
    return "mod_wheel";
  if (s == "pitch_wheel" || s == "pitchwheel" || s == "pitchbend")
    return "pitch_wheel";
  if (s == "aftertouch" || s == "touch" || s == "pressure")
    return "aftertouch";
  if (s == "random" || s == "rand" || s == "noise_mod")
    return "random";

  return source;
}

std::string AetherPatchSerializer::resolveParameterName(const std::string& key, const std::string& prefix) {
  if (mopo::Parameters::isParameter(key))
    return key;

  std::string combined = prefix.empty() ? key : prefix + "_" + key;
  if (mopo::Parameters::isParameter(combined))
    return combined;

  // Filter aliases
  if (combined == "filter_env_depth" || combined == "env_depth" || combined == "filter_envelope_depth")
    return "fil_env_depth";
  if (combined == "filter_attack") return "fil_attack";
  if (combined == "filter_decay") return "fil_decay";
  if (combined == "filter_sustain") return "fil_sustain";
  if (combined == "filter_release") return "fil_release";
  if (combined == "filter_cutoff") return "cutoff";
  if (combined == "filter_resonance") return "resonance";
  if (combined == "filter_keytrack" || combined == "filter_key_track") return "keytrack";
  if (combined == "filter_saturation") return "filter_drive";

  // Oscillator aliases
  if (combined == "osc1_volume") return "osc_1_volume";
  if (combined == "osc2_volume") return "osc_2_volume";
  if (combined == "osc1_waveform") return "osc_1_waveform";
  if (combined == "osc2_waveform") return "osc_2_waveform";
  if (combined == "osc1_transpose") return "osc_1_transpose";
  if (combined == "osc2_transpose") return "osc_2_transpose";
  if (combined == "osc1_tune") return "osc_1_tune";
  if (combined == "osc2_tune") return "osc_2_tune";
  if (combined == "osc1_unison_voices") return "osc_1_unison_voices";
  if (combined == "osc2_unison_voices") return "osc_2_unison_voices";
  if (combined == "osc1_unison_detune") return "osc_1_unison_detune";
  if (combined == "osc2_unison_detune") return "osc_2_unison_detune";
  if (combined == "sub_osc_volume" || combined == "sub_level") return "sub_volume";
  if (combined == "sub_osc_waveform") return "sub_waveform";
  if (combined == "sub_osc_octave") return "sub_octave";
  if (combined == "noise_level") return "noise_volume";

  // Envelopes aliases
  if (combined == "amp_env_attack") return "amp_attack";
  if (combined == "amp_env_decay") return "amp_decay";
  if (combined == "amp_env_sustain") return "amp_sustain";
  if (combined == "amp_env_release") return "amp_release";
  if (combined == "mod_env_attack") return "mod_attack";
  if (combined == "mod_env_decay") return "mod_decay";
  if (combined == "mod_env_sustain") return "mod_sustain";
  if (combined == "mod_env_release") return "mod_release";

  // Effects aliases
  if (combined == "distortion_gain" || combined == "distortion_amount") return "distortion_drive";
  if (combined == "delay_mix") return "delay_dry_wet";
  if (combined == "delay_time") return "delay_tempo";
  if (combined == "reverb_mix") return "reverb_dry_wet";
  if (combined == "reverb_decay" || combined == "reverb_size" || combined == "reverb_room_size") return "reverb_feedback";

  // Global aliases
  if (combined == "master_volume" || combined == "main_volume") return "volume";
  if (combined == "glide" || combined == "glide_time") return "portamento";
  if (combined == "poly" || combined == "voices") return "polyphony";

  // Prefix handling for hierarchical structures
  if (prefix == "filter") {
    if (key == "cutoff") return "cutoff";
    if (key == "resonance") return "resonance";
    if (key == "keytrack") return "keytrack";
    std::string filPrefixed = "filter_" + key;
    if (mopo::Parameters::isParameter(filPrefixed)) return filPrefixed;
    std::string shortPrefixed = "fil_" + key;
    if (mopo::Parameters::isParameter(shortPrefixed)) return shortPrefixed;
  } else if (prefix == "envelopes" || prefix == "amp" || prefix == "amplitude") {
    std::string ampPrefixed = "amp_" + key;
    if (mopo::Parameters::isParameter(ampPrefixed)) return ampPrefixed;
  } else if (prefix == "mod" || prefix == "mod_envelope") {
    std::string modPrefixed = "mod_" + key;
    if (mopo::Parameters::isParameter(modPrefixed)) return modPrefixed;
  } else if (prefix == "reverb") {
    if (key == "decay" || key == "size") return "reverb_feedback";
    if (key == "mix") return "reverb_dry_wet";
    std::string revPrefixed = "reverb_" + key;
    if (mopo::Parameters::isParameter(revPrefixed)) return revPrefixed;
  } else if (prefix == "delay") {
    if (key == "mix") return "delay_dry_wet";
    if (key == "time") return "delay_tempo";
    std::string delPrefixed = "delay_" + key;
    if (mopo::Parameters::isParameter(delPrefixed)) return delPrefixed;
  } else if (prefix == "distortion") {
    if (key == "drive" || key == "gain") return "distortion_drive";
    if (key == "mix") return "distortion_mix";
    std::string distPrefixed = "distortion_" + key;
    if (mopo::Parameters::isParameter(distPrefixed)) return distPrefixed;
  }

  return combined;
}

std::string AetherPatchSerializer::getParameterCategory(const std::string& paramId) {
  std::string id = paramId;
  if (id.rfind("osc_1", 0) == 0) return "Oscillator 1";
  if (id.rfind("osc_2", 0) == 0) return "Oscillator 2";
  if (id.rfind("sub_", 0) == 0) return "Sub Oscillator";
  if (id.rfind("noise_", 0) == 0) return "Noise";
  if (id == "cross_modulation") return "Oscillators";

  if (id.rfind("fil_", 0) == 0 || id.rfind("filter_", 0) == 0 ||
      id.rfind("formant_", 0) == 0 || id == "cutoff" || id == "resonance" || id == "keytrack")
    return "Filter";

  if (id.rfind("amp_", 0) == 0) return "Amp Envelope";
  if (id.rfind("mod_", 0) == 0) return "Mod Envelope";

  if (id.rfind("mono_lfo_1", 0) == 0) return "Mono LFO 1";
  if (id.rfind("mono_lfo_2", 0) == 0) return "Mono LFO 2";
  if (id.rfind("poly_lfo", 0) == 0) return "Poly LFO";
  if (id.rfind("step_", 0) == 0 || id == "num_steps") return "Step Sequencer";
  if (id.rfind("arp_", 0) == 0) return "Arpeggiator";

  if (id.rfind("distortion_", 0) == 0) return "Distortion";
  if (id.rfind("delay_", 0) == 0) return "Delay";
  if (id.rfind("reverb_", 0) == 0) return "Reverb";
  if (id.rfind("stutter_", 0) == 0) return "Stutter";

  if (id == "volume" || id == "polyphony" || id == "legato" ||
      id == "portamento" || id == "beats_per_minute" || id.rfind("pitch_bend", 0) == 0)
    return "Global";

  return "General";
}

std::string AetherPatchSerializer::getParameterDescription(const std::string& paramId) {
  static const std::map<std::string, std::string> descriptions = {
    { "cutoff", "Filter cutoff frequency expressed as MIDI note pitch (28.0 = E0 ~41 Hz to 127.0 = G9 ~12.5 kHz, 60 = Middle C)." },
    { "resonance", "Filter resonance sharpness / Q-factor (0.0 = unresonant to 1.0 = self-oscillating peak)." },
    { "filter_style", "Filter slope roll-off style (0 = 12dB/octave, 1 = 24dB/octave lowpass, 2 = Shelf)." },
    { "filter_blend", "Filter continuous mode blend (0.0 = low-pass, 1.0 = band-pass, 2.0 = high-pass)." },
    { "filter_drive", "Pre-filter saturation drive in dB (-12.0 to +20.0 dB)." },
    { "filter_on", "Filter circuit toggle (0 = bypass, 1 = active)." },
    { "fil_env_depth", "Bipolar depth of filter envelope modulation on cutoff (-128.0 to +128.0 semitones)." },
    { "fil_attack", "Filter envelope attack time in seconds (0.0 to 4.0s)." },
    { "fil_decay", "Filter envelope decay time in seconds (0.0 to 4.0s)." },
    { "fil_sustain", "Filter envelope sustain level (0.0 to 1.0)." },
    { "fil_release", "Filter envelope release time in seconds (0.0 to 4.0s)." },
    { "amp_attack", "Amplitude envelope attack time in seconds (0.0 to 4.0s)." },
    { "amp_decay", "Amplitude envelope decay time in seconds (0.0 to 4.0s)." },
    { "amp_sustain", "Amplitude envelope sustain level (0.0 to 1.0)." },
    { "amp_release", "Amplitude envelope release time in seconds (0.0 to 4.0s)." },
    { "mod_attack", "Modulation envelope attack time in seconds (0.0 to 4.0s)." },
    { "mod_decay", "Modulation envelope decay time in seconds (0.0 to 4.0s)." },
    { "mod_sustain", "Modulation envelope sustain level (0.0 to 1.0)." },
    { "mod_release", "Modulation envelope release time in seconds (0.0 to 4.0s)." },
    { "osc_1_waveform", "Oscillator 1 waveform shape index (0=Sine, 1=Triangle, 2=Square, 3=Saw, 4=Rev Saw, 5=3-Step, 6=4-Step, 7=8-Step, 8=3-Pyramid, 9=5-Pyramid, 10=9-Pyramid, 11=Sine-Power)." },
    { "osc_1_volume", "Oscillator 1 volume level (0.0 to 1.0)." },
    { "osc_1_transpose", "Oscillator 1 pitch transposition in semitones (-48.0 to +48.0 semitones)." },
    { "osc_1_tune", "Oscillator 1 fine pitch detuning in cents (-100.0 to +100.0 cents)." },
    { "osc_1_unison_voices", "Oscillator 1 unison voice stack count (1 to 15 voices)." },
    { "osc_1_unison_detune", "Oscillator 1 stereo unison detune spread (0.0 to 100.0 cents)." },
    { "osc_2_waveform", "Oscillator 2 waveform shape index (0=Sine, 1=Triangle, 2=Square, 3=Saw, etc.)." },
    { "osc_2_volume", "Oscillator 2 volume level (0.0 to 1.0)." },
    { "osc_2_transpose", "Oscillator 2 pitch transposition in semitones (-48.0 to +48.0 semitones)." },
    { "osc_2_tune", "Oscillator 2 fine pitch detuning in cents (-100.0 to +100.0 cents)." },
    { "osc_2_unison_voices", "Oscillator 2 unison voice stack count (1 to 15 voices)." },
    { "osc_2_unison_detune", "Oscillator 2 stereo unison detune spread (0.0 to 100.0 cents)." },
    { "cross_modulation", "Frequency/phase cross-modulation amount between Oscillator 1 and 2 (0.0 to 1.0)." },
    { "sub_volume", "Sub-oscillator output volume (0.0 to 1.0)." },
    { "sub_waveform", "Sub-oscillator waveform (0=Sine, 1=Triangle, 2=Square)." },
    { "sub_octave", "Sub-oscillator octave offset (-1 or -2 octaves below Osc 1)." },
    { "noise_volume", "Noise generator output volume (0.0 to 1.0)." },
    { "distortion_on", "Distortion effect toggle (0 = off, 1 = on)." },
    { "distortion_drive", "Distortion overdrive gain in dB (-30.0 to +30.0 dB)." },
    { "distortion_mix", "Distortion dry/wet mix balance (0.0 = dry, 1.0 = wet)." },
    { "distortion_type", "Distortion saturation mode (0=Soft Clip, 1=Hard Clip, 2=Linear Fold, 3=Sine Fold)." },
    { "delay_on", "Stereo ping-pong delay toggle (0 = off, 1 = on)." },
    { "delay_dry_wet", "Delay wet mix level (0.0 = dry, 1.0 = wet)." },
    { "delay_feedback", "Delay repeats feedback amount (0.0 to 1.0)." },
    { "delay_sync", "Delay tempo synchronization mode (0=Free running, 1=Host tempo sync)." },
    { "delay_tempo", "Delay synchronized tempo division index (0 to 11)." },
    { "reverb_on", "Stereo algorithmic reverb toggle (0 = off, 1 = on)." },
    { "reverb_dry_wet", "Reverb wet mix level (0.0 = dry, 1.0 = wet)." },
    { "reverb_feedback", "Reverb room size / decay time feedback (0.0 to 1.0)." },
    { "reverb_damping", "Reverb high-frequency absorption damping (0.0 to 1.0)." },
    { "volume", "Master synth output volume gain (0.0 to 1.4143 / +3dB)." },
    { "polyphony", "Maximum simultaneous active voices limit (1 to 32 voices)." },
    { "portamento", "Portamento note pitch glide rate (0.0 to 1.0)." },
    { "legato", "Legato mode switch (0 = polyphonic retrigger, 1 = legato mono glide)." },
    { "keytrack", "Filter cutoff keyboard tracking percentage (-1.0 to +1.0 / -100% to +100%)." }
  };

  auto it = descriptions.find(paramId);
  if (it != descriptions.end())
    return it->second;

  if (mopo::Parameters::isParameter(paramId)) {
    mopo::ValueDetails vd = mopo::Parameters::getDetails(paramId);
    std::string unitStr = vd.display_units.empty() ? "" : (" " + vd.display_units);
    return vd.display_name + " [Range: " + std::to_string(vd.min) + " to " + std::to_string(vd.max) + unitStr + "]";
  }

  return paramId;
}

void AetherPatchSerializer::applyControl(SynthBase* synth, const std::string& name,
                                        mopo::mopo_float value, int* updatedCount) {
  std::string resolvedName = resolveParameterName(name);
  if (!mopo::Parameters::isParameter(resolvedName))
    return;

  mopo::ValueDetails details = mopo::Parameters::getDetails(resolvedName);
  if (!std::isfinite(value))
    value = static_cast<mopo::mopo_float>(details.default_value);

  mopo::mopo_float minVal = static_cast<mopo::mopo_float>(details.min);
  mopo::mopo_float maxVal = static_cast<mopo::mopo_float>(details.max);
  mopo::mopo_float clamped = std::clamp<mopo::mopo_float>(value, minVal, maxVal);

  mopo::control_map controls = synth->getControls();
  auto it = controls.find(resolvedName);
  if (it != controls.end() && it->second)
    it->second->set(clamped);

  synth->valueChangedInternal(resolvedName, clamped);
  if (updatedCount)
    (*updatedCount)++;
}

void AetherPatchSerializer::applyModulation(SynthBase* synth, const std::string& source,
                                           const std::string& dest, mopo::mopo_float amount,
                                           int* updatedCount) {
  if (source.empty() || dest.empty())
    return;

  std::string resolvedSource = resolveModulationSourceName(source);
  std::string resolvedDest = resolveParameterName(dest);

  if (!isValidModulationSource(resolvedSource) && synth->getModSource(resolvedSource) == nullptr)
    return;
  if (!mopo::Parameters::isParameter(resolvedDest))
    return;
  if (!std::isfinite(amount))
    amount = 0.0f;

  mopo::mopo_float clamped = std::clamp<mopo::mopo_float>(amount, -1.0f, 1.0f);
  synth->changeModulationAmount(resolvedSource, resolvedDest, clamped);
  if (updatedCount)
    (*updatedCount)++;
}

void AetherPatchSerializer::parseHierarchicalSection(SynthBase* synth, const var& sectionVar,
                                                    const std::string& prefix, int* updatedCount) {
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
      parseHierarchicalSection(synth, val, newPrefix, updatedCount);
    } else if (val.isDouble() || val.isInt() || val.isInt64() || val.isBool()) {
      mopo::mopo_float numVal = static_cast<mopo::mopo_float>(val);
      std::string paramName = resolveParameterName(key, prefix);
      applyControl(synth, paramName, numVal, updatedCount);
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

bool AetherPatchSerializer::loadPatchFromJson(SynthBase* synth, const std::string& jsonString,
                                             std::string* error, int* updatedCount) {
  var parsedState;
  Result res = JSON::parse(String(jsonString), parsedState);
  if (res.failed()) {
    if (error) *error = "JSON parse error: " + res.getErrorMessage().toStdString();
    return false;
  }

  return varToState(synth, parsedState, error, updatedCount);
}

bool AetherPatchSerializer::varToState(SynthBase* synth, const var& parsedState,
                                      std::string* error, int* updatedCount) {
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
      if (val.isDouble() || val.isInt() || val.isInt64() || val.isBool())
        applyControl(synth, key, static_cast<mopo::mopo_float>(val), updatedCount);
    }
  }

  // Check for 'parameters' or 'patch_or_params' object
  if (props.contains("parameters") && props["parameters"].isObject()) {
    DynamicObject* paramsObj = props["parameters"].getDynamicObject();
    NamedValueSet paramProps = paramsObj->getProperties();
    for (int i = 0; i < paramProps.size(); ++i) {
      std::string key = paramProps.getName(i).toString().toStdString();
      if (key == "modulations") continue;
      var val = paramProps.getValueAt(i);
      if (val.isDouble() || val.isInt() || val.isInt64() || val.isBool())
        applyControl(synth, key, static_cast<mopo::mopo_float>(val), updatedCount);
      else if (val.isObject())
        parseHierarchicalSection(synth, val, key, updatedCount);
    }
  }

  if (props.contains("patch_or_params") && props["patch_or_params"].isObject()) {
    DynamicObject* popObj = props["patch_or_params"].getDynamicObject();
    NamedValueSet popProps = popObj->getProperties();
    for (int i = 0; i < popProps.size(); ++i) {
      std::string key = popProps.getName(i).toString().toStdString();
      if (key == "modulations") continue;
      var val = popProps.getValueAt(i);
      if (val.isDouble() || val.isInt() || val.isInt64() || val.isBool())
        applyControl(synth, key, static_cast<mopo::mopo_float>(val), updatedCount);
      else if (val.isObject())
        parseHierarchicalSection(synth, val, key, updatedCount);
    }
  }

  // Check for hierarchical sections
  const char* sections[] = { "oscillators", "filter", "envelopes", "modulators", "effects", "global" };
  for (const char* sectionName : sections) {
    if (props.contains(sectionName))
      parseHierarchicalSection(synth, props[sectionName], "", updatedCount);
  }

  // Check for direct properties at root level
  for (int i = 0; i < props.size(); ++i) {
    std::string key = props.getName(i).toString().toStdString();
    if (key == "settings" || key == "parameters" || key == "patch_or_params" ||
        key == "modulations" || key == "patch_name" || key == "author" ||
        key == "folder_name" || key == "synth_name" || key == "synth_version")
      continue;

    std::string resolved = resolveParameterName(key);
    if (mopo::Parameters::isParameter(resolved)) {
      var val = props.getValueAt(i);
      if (val.isDouble() || val.isInt() || val.isInt64() || val.isBool())
        applyControl(synth, resolved, static_cast<mopo::mopo_float>(val), updatedCount);
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
      applyModulation(synth, source, dest, amount, updatedCount);
    }
  }

  return true;
}

std::string AetherPatchSerializer::getParameterDocumentationJson() {
  DynamicObject* doc = new DynamicObject();
  std::map<std::string, mopo::ValueDetails> allDetails = mopo::Parameters::lookup_.getAllDetails();

  for (const auto& pair : allDetails) {
    DynamicObject* item = new DynamicObject();
    item->setProperty("name", String(pair.second.display_name));
    item->setProperty("category", String(getParameterCategory(pair.first)));
    item->setProperty("min", pair.second.min);
    item->setProperty("max", pair.second.max);
    item->setProperty("default", pair.second.default_value);
    item->setProperty("units", String(pair.second.display_units));
    item->setProperty("description", String(getParameterDescription(pair.first)));
    doc->setProperty(String(pair.first), item);
  }

  return JSON::toString(doc, false).toStdString();
}

std::string AetherPatchSerializer::extractJsonFromText(const std::string& raw) {
  std::string s = raw;
  size_t fenceStart = s.find("```");
  if (fenceStart != std::string::npos) {
    size_t lineEnd = s.find('\n', fenceStart);
    if (lineEnd != std::string::npos) {
      size_t contentStart = lineEnd + 1;
      size_t fenceEnd = s.find("```", contentStart);
      if (fenceEnd != std::string::npos) {
        std::string fenced = s.substr(contentStart, fenceEnd - contentStart);
        size_t firstBrace = fenced.find('{');
        size_t lastBrace = fenced.rfind('}');
        if (firstBrace != std::string::npos && lastBrace != std::string::npos && lastBrace > firstBrace) {
          return fenced.substr(firstBrace, lastBrace - firstBrace + 1);
        }
      }
    }
  }

  size_t firstBrace = s.find('{');
  if (firstBrace == std::string::npos)
    return raw;

  int depth = 0;
  bool inQuotes = false;
  bool escaped = false;
  size_t endBrace = std::string::npos;

  for (size_t i = firstBrace; i < s.length(); ++i) {
    char c = s[i];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      inQuotes = !inQuotes;
      continue;
    }
    if (!inQuotes) {
      if (c == '{') {
        depth++;
      } else if (c == '}') {
        depth--;
        if (depth == 0) {
          endBrace = i;
          break;
        }
      }
    }
  }

  if (endBrace != std::string::npos)
    return s.substr(firstBrace, endBrace - firstBrace + 1);

  size_t lastBrace = s.rfind('}');
  if (lastBrace != std::string::npos && lastBrace > firstBrace)
    return s.substr(firstBrace, lastBrace - firstBrace + 1);

  return raw;
}
