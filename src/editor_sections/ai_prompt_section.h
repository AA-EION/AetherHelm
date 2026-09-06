#pragma once

#include "JuceHeader.h"
#include "synth_section.h"

class AiPromptSection : public SynthSection {
public:
  AiPromptSection(String name);
  ~AiPromptSection() override;

  void paintBackground(Graphics& g) override;
  void resized() override;
  void buttonClicked(Button* button) override;

private:
  ScopedPointer<TextEditor> prompt_input_;
  ScopedPointer<TextButton> generate_button_;
  ScopedPointer<ComboBox> model_selector_;
  ScopedPointer<TextButton> config_key_button_;
  ScopedPointer<Label> status_label_;

  void triggerGeneration();
  void configureApiKey();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AiPromptSection)
};
