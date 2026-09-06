class FullInterface {
  public:
    virtual ~FullInterface() = default;
};

#include "synth_gui_interface.h"

SynthGuiInterface::SynthGuiInterface(SynthBase* synth, bool) : synth_(synth) { }
SynthGuiInterface::~SynthGuiInterface() = default;
void SynthGuiInterface::updateFullGui() { }
void SynthGuiInterface::updateGuiControl(const std::string&, mopo::mopo_float) { }
mopo::mopo_float SynthGuiInterface::getControlValue(const std::string&) { return 0.0f; }
void SynthGuiInterface::setFocus() { }
void SynthGuiInterface::notifyChange() { }
void SynthGuiInterface::notifyFresh() { }
void SynthGuiInterface::externalPatchLoaded(File) { }
void SynthGuiInterface::setGuiSize(int, int) { }
