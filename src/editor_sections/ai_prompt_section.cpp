#include "ai_prompt_section.h"
#include "colors.h"
#include "fonts.h"
#include "openrouter_client.h"
#include "synth_gui_interface.h"

AiPromptSection::AiPromptSection(String name) : SynthSection(name) {
  prompt_input_ = new TextEditor("PromptInput");
  prompt_input_->setMultiLine(false);
  prompt_input_->setReturnKeyStartsNewLine(false);
  prompt_input_->setTextToShowWhenEmpty(TRANS("Describe sound (e.g. Cyberpunk bass with detuned saws and notch filter)..."),
                                        Colour(0xff888888));
  prompt_input_->setColour(TextEditor::backgroundColourId, Colour(0xff181b20));
  prompt_input_->setColour(TextEditor::textColourId, Colour(0xffffffff));
  prompt_input_->setColour(TextEditor::outlineColourId, Colour(0xff333842));
  addAndMakeVisible(prompt_input_);

  model_selector_ = new ComboBox("ModelSelector");
  std::vector<std::string> models = OpenRouterClient::getAvailableModels();
  for (size_t i = 0; i < models.size(); ++i)
    model_selector_->addItem(String(models[i]), static_cast<int>(i + 1));
  model_selector_->setSelectedId(1);
  model_selector_->setColour(ComboBox::backgroundColourId, Colour(0xff22262e));
  model_selector_->setColour(ComboBox::textColourId, Colour(0xffe0e0e0));
  addAndMakeVisible(model_selector_);

  generate_button_ = new TextButton("GenerateButton");
  generate_button_->setButtonText(TRANS("GENERATE"));
  generate_button_->setColour(TextButton::buttonColourId, Colour(0xff2196f3));
  generate_button_->setColour(TextButton::textColourOffId, Colour(0xffffffff));
  generate_button_->addListener(this);
  addAndMakeVisible(generate_button_);

  config_key_button_ = new TextButton("KeyButton");
  config_key_button_->setButtonText(TRANS("KEY"));
  config_key_button_->setColour(TextButton::buttonColourId, Colour(0xff2a2e38));
  config_key_button_->setColour(TextButton::textColourOffId, Colour(0xffcccccc));
  config_key_button_->addListener(this);
  addAndMakeVisible(config_key_button_);

  close_button_ = new TextButton("CloseButton");
  close_button_->setButtonText(TRANS("X"));
  close_button_->setColour(TextButton::buttonColourId, Colours::transparentBlack);
  close_button_->setColour(TextButton::textColourOffId, Colour(0xff888888));
  close_button_->addListener(this);
  addAndMakeVisible(close_button_);

  status_label_ = new Label("StatusLabel", TRANS("Ready"));
  status_label_->setFont(Fonts::instance()->proportional_light().withPointHeight(11.0f));
  status_label_->setColour(Label::textColourId, Colour(0xff888888));
  addAndMakeVisible(status_label_);
}

AiPromptSection::~AiPromptSection() {}

void AiPromptSection::paintBackground(Graphics& g) {
  paintContainer(g);
  g.setColour(Colour(0xf0181b20));
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
  g.setColour(Colour(0xff333842));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
}

void AiPromptSection::resized() {
  SynthSection::resized();
  int pad = 4;
  int x = pad;
  int y = 4;
  int w = getWidth() - 2 * x;
  int h = getHeight() - 2 * pad;

  int btnW = 85;
  int keyW = 45;
  int modelW = 160;
  int closeW = 22;
  int gap = 6;

  int inputW = w - btnW - keyW - modelW - closeW - (gap * 4);
  if (inputW < 120) {
    modelW = 120;
    inputW = w - btnW - keyW - modelW - closeW - (gap * 4);
  }

  int compH = std::min(24, h);

  prompt_input_->setBounds(x, y, inputW, compH);
  model_selector_->setBounds(x + inputW + gap, y, modelW, compH);
  generate_button_->setBounds(x + inputW + gap + modelW + gap, y, btnW, compH);
  config_key_button_->setBounds(x + inputW + gap + modelW + gap + btnW + gap, y, keyW, compH);
  close_button_->setBounds(x + inputW + gap + modelW + gap + btnW + gap + keyW + gap, y, closeW, compH);

  if (h >= 24)
    status_label_->setBounds(x, y + compH + 2, w - closeW - gap, std::max(14, h - compH - 2));
  else
    status_label_->setBounds(0, 0, 0, 0);
}

void AiPromptSection::buttonClicked(Button* button) {
  if (button == generate_button_)
    triggerGeneration();
  else if (button == config_key_button_)
    configureApiKey();
  else if (button == close_button_)
    setVisible(false);
}

void AiPromptSection::triggerGeneration() {
  String promptText = prompt_input_->getText().trim();
  if (promptText.isEmpty()) {
    status_label_->setText(TRANS("Please enter a sound description first."), NotificationType::dontSendNotification);
    return;
  }

  SynthGuiInterface* gui = findParentComponentOfClass<SynthGuiInterface>();
  SynthBase* synth = gui ? gui->getSynth() : nullptr;
  if (!synth) {
    status_label_->setText(TRANS("Synthesizer instance not found."), NotificationType::dontSendNotification);
    return;
  }

  std::string model = model_selector_->getText().toStdString();
  status_label_->setText(TRANS("Generating sound via OpenRouter..."), NotificationType::dontSendNotification);
  generate_button_->setEnabled(false);

  OpenRouterClient::instance()->requestPatchGeneration(
    synth,
    promptText.toStdString(),
    model,
    [this](bool success, const std::string& message) {
      generate_button_->setEnabled(true);
      status_label_->setText(String(message), NotificationType::dontSendNotification);
    }
  );
}

void AiPromptSection::configureApiKey() {
  AlertWindow w(TRANS("OpenRouter API Key"),
                TRANS("Enter your OpenRouter API key for in-app AI sound generation:"),
                AlertWindow::QuestionIcon);

  w.addTextEditor("apiKey", OpenRouterClient::getApiKey(), TRANS("API Key:"));
  w.getTextEditor("apiKey")->setPasswordCharacter('*');
  w.addButton(TRANS("Save"), 1, KeyPress(KeyPress::returnKey));
  w.addButton(TRANS("Cancel"), 0, KeyPress(KeyPress::escapeKey));

  if (w.runModalLoop() == 1) {
    String newKey = w.getTextEditorContents("apiKey").trim();
    OpenRouterClient::setApiKey(newKey);
    status_label_->setText(TRANS("API Key saved."), NotificationType::dontSendNotification);
  }
}
