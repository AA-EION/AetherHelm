#pragma once

#include "synth_base.h"

class HeadlessSynth : public SynthBase {
public:
  HeadlessSynth() {
    engine_.setSampleRate(44100);
    engine_.setBufferSize(256);
    loadInitPatch();
  }

  void flushQueues() {
    processControlChanges();
    processModulationChanges();
  }

  void renderBlock() {
    flushQueues();
    engine_.process();
  }

  const CriticalSection& getCriticalSection() override { return lock_; }
  SynthGuiInterface* getGuiInterface() override { return nullptr; }

private:
  CriticalSection lock_;
};
