cmake_minimum_required(VERSION 3.22)

project(AetherHelm VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/src/common/juce_compat.h>)
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/common/juce_compat.h>)
endif()

# Option to fetch modern JUCE from GitHub or use local copy
option(AETHERHELM_FETCH_JUCE "Fetch JUCE from GitHub" ON)

if(AETHERHELM_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
    )
    FetchContent_MakeAvailable(JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

# Common include directories
set(AETHERHELM_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/JuceLibraryCode
    ${CMAKE_CURRENT_SOURCE_DIR}/concurrentqueue
    ${CMAKE_CURRENT_SOURCE_DIR}/mopo/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor_components
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor_sections
    ${CMAKE_CURRENT_SOURCE_DIR}/src/look_and_feel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/standalone
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mcp
)

# Binary resources (fonts and images)
juce_add_binary_data(AetherHelm_BinaryData
    SOURCES
        fonts/DroidSansMono.ttf
        fonts/Roboto-Light.ttf
        fonts/Roboto-Regular.ttf
        fonts/Roboto-Thin.ttf
        images/helm_debian_icon.xpm
        images/helm_icon_16_1x.png
        images/helm_icon_16_2x.png
        images/helm_icon_32_1x.png
        images/helm_icon_32_2x.png
        images/helm_icon_128_1x.png
        images/helm_icon_128_2x.png
        images/helm_icon_256_1x.png
        images/helm_icon_256_2x.png
        images/helm_icon_512_1x.png
        images/helm_icon_512_2x.png
        images/modulation_selected_active_1x.png
        images/modulation_selected_active_2x.png
        images/modulation_selected_inactive_1x.png
        images/modulation_selected_inactive_2x.png
        images/modulation_unselected_active_1x.png
        images/modulation_unselected_active_2x.png
        images/modulation_unselected_inactive_1x.png
        images/modulation_unselected_inactive_2x.png
)

# DSP Library (mopo) sources
set(MOPO_SOURCES
    mopo/src/alias.cpp
    mopo/src/arpeggiator.cpp
    mopo/src/biquad_filter.cpp
    mopo/src/bit_crush.cpp
    mopo/src/bypass_router.cpp
    mopo/src/delay.cpp
    mopo/src/distortion.cpp
    mopo/src/envelope.cpp
    mopo/src/feedback.cpp
    mopo/src/formant_manager.cpp
    mopo/src/ladder_filter.cpp
    mopo/src/linear_slope.cpp
    mopo/src/magnitude_lookup.cpp
    mopo/src/memory.cpp
    mopo/src/midi_lookup.cpp
    mopo/src/mono_panner.cpp
    mopo/src/operators.cpp
    mopo/src/oscillator.cpp
    mopo/src/portamento_slope.cpp
    mopo/src/processor.cpp
    mopo/src/processor_router.cpp
    mopo/src/resonance_lookup.cpp
    mopo/src/reverb.cpp
    mopo/src/reverb_all_pass.cpp
    mopo/src/reverb_comb.cpp
    mopo/src/sample_decay_lookup.cpp
    mopo/src/simple_delay.cpp
    mopo/src/smooth_filter.cpp
    mopo/src/smooth_value.cpp
    mopo/src/state_variable_filter.cpp
    mopo/src/step_generator.cpp
    mopo/src/stutter.cpp
    mopo/src/trigger_operators.cpp
    mopo/src/value.cpp
    mopo/src/voice_handler.cpp
)

# Synthesis engine sources
set(SYNTHESIS_SOURCES
    src/synthesis/dc_filter.cpp
    src/synthesis/detune_lookup.cpp
    src/synthesis/fixed_point_oscillator.cpp
    src/synthesis/fixed_point_wave.cpp
    src/synthesis/gate.cpp
    src/synthesis/helm_engine.cpp
    src/synthesis/helm_lfo.cpp
    src/synthesis/helm_module.cpp
    src/synthesis/helm_oscillators.cpp
    src/synthesis/helm_voice_handler.cpp
    src/synthesis/noise_oscillator.cpp
    src/synthesis/peak_meter.cpp
    src/synthesis/resonance_cancel.cpp
    src/synthesis/trigger_random.cpp
    src/synthesis/value_switch.cpp
)

# Common headless utility sources
set(SHARED_COMMON_SOURCES
    src/common/helm_common.cpp
    src/common/load_save.cpp
    src/common/midi_manager.cpp
    src/common/startup.cpp
    src/common/synth_base.cpp
    src/common/aether_patch_serializer.cpp
    src/common/openrouter_client.cpp
)

# Headless targets use the stub for GUI callbacks
set(HEADLESS_COMMON_SOURCES
    ${SHARED_COMMON_SOURCES}
    src/common/synth_gui_stub.cpp
)

# GUI common sources
set(GUI_COMMON_SOURCES
    src/common/border_bounds_constrainer.cpp
    src/common/file_list_box_model.cpp
    src/common/synth_gui_interface.cpp
)

set(COMMON_SOURCES
    ${SHARED_COMMON_SOURCES}
    ${GUI_COMMON_SOURCES}
)

# Look and feel UI sources
set(LOOK_AND_FEEL_SOURCES
    src/look_and_feel/browser_look_and_feel.cpp
    src/look_and_feel/colors.cpp
    src/look_and_feel/default_look_and_feel.cpp
    src/look_and_feel/fonts.cpp
    src/look_and_feel/modulation_look_and_feel.cpp
    src/look_and_feel/shaders.cpp
    src/look_and_feel/text_look_and_feel.cpp
)

# Editor components sources
set(EDITOR_COMPONENTS_SOURCES
    src/editor_components/bpm_slider.cpp
    src/editor_components/filter_response.cpp
    src/editor_components/filter_selector.cpp
    src/editor_components/formant_response.cpp
    src/editor_components/global_tool_tip.cpp
    src/editor_components/graphical_step_sequencer.cpp
    src/editor_components/midi_keyboard.cpp
    src/editor_components/modulation_button.cpp
    src/editor_components/modulation_highlight.cpp
    src/editor_components/modulation_meter.cpp
    src/editor_components/modulation_slider.cpp
    src/editor_components/open_gl_background.cpp
    src/editor_components/open_gl_component.cpp
    src/editor_components/open_gl_envelope.cpp
    src/editor_components/open_gl_modulation_meter.cpp
    src/editor_components/open_gl_oscilloscope.cpp
    src/editor_components/open_gl_peak_meter.cpp
    src/editor_components/open_gl_wave_viewer.cpp
    src/editor_components/oscilloscope.cpp
    src/editor_components/patch_selector.cpp
    src/editor_components/retrigger_selector.cpp
    src/editor_components/synth_button.cpp
    src/editor_components/synth_slider.cpp
    src/editor_components/tempo_selector.cpp
    src/editor_components/text_selector.cpp
    src/editor_components/text_slider.cpp
    src/editor_components/wave_selector.cpp
    src/editor_components/wave_viewer.cpp
    src/editor_components/xy_pad.cpp
)

# Editor sections sources
set(EDITOR_SECTIONS_SOURCES
    src/editor_sections/about_section.cpp
    src/editor_sections/ai_prompt_section.cpp
    src/editor_sections/arp_section.cpp
    src/editor_sections/bpm_section.cpp
    src/editor_sections/contribute_section.cpp
    src/editor_sections/delay_section.cpp
    src/editor_sections/delete_section.cpp
    src/editor_sections/distortion_section.cpp
    src/editor_sections/dynamic_section.cpp
    src/editor_sections/envelope_section.cpp
    src/editor_sections/extra_mod_section.cpp
    src/editor_sections/feedback_section.cpp
    src/editor_sections/filter_section.cpp
    src/editor_sections/formant_section.cpp
    src/editor_sections/formant_tuner.cpp
    src/editor_sections/full_interface.cpp
    src/editor_sections/lfo_section.cpp
    src/editor_sections/mixer_section.cpp
    src/editor_sections/modulation_manager.cpp
    src/editor_sections/noise_section.cpp
    src/editor_sections/open_gl_modulation_manager.cpp
    src/editor_sections/oscillator_section.cpp
    src/editor_sections/patch_browser.cpp
    src/editor_sections/reverb_section.cpp
    src/editor_sections/save_section.cpp
    src/editor_sections/step_sequencer_section.cpp
    src/editor_sections/stutter_section.cpp
    src/editor_sections/sub_section.cpp
    src/editor_sections/synth_section.cpp
    src/editor_sections/synthesis_interface.cpp
    src/editor_sections/update_check_section.cpp
    src/editor_sections/voice_section.cpp
    src/editor_sections/volume_section.cpp
)

# Plugin target formats
set(AETHERHELM_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND AETHERHELM_FORMATS AU)
endif()

# JUCE 7.0.12's VST3 moduleinfo tool (juce_vst3_helper) does not run correctly
# on Windows ARM64, so build Standalone-only there.
if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
    list(REMOVE_ITEM AETHERHELM_FORMATS VST3)
endif()

# ==============================================================================
# Plugin Target: AetherHelm
# ==============================================================================
juce_add_plugin(AetherHelm
    VERSION "1.0.0"
    PLUGIN_MANUFACTURER_CODE "AeEl"
    PLUGIN_CODE "AeHm"
    FORMATS ${AETHERHELM_FORMATS}
    PRODUCT_NAME "AetherHelm"
    COMPANY_NAME "AA-EION"
    BUNDLE_ID "com.aeeion.aetherhelm"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
)

target_sources(AetherHelm
    PRIVATE
        ${MOPO_SOURCES}
        ${SYNTHESIS_SOURCES}
        ${COMMON_SOURCES}
        ${LOOK_AND_FEEL_SOURCES}
        ${EDITOR_COMPONENTS_SOURCES}
        ${EDITOR_SECTIONS_SOURCES}
        src/plugin/helm_editor.cpp
        src/plugin/helm_plugin.cpp
)

target_include_directories(AetherHelm
    PRIVATE
        ${AETHERHELM_INCLUDE_DIRS}
)

target_compile_definitions(AetherHelm
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(AetherHelm
    PRIVATE
        AetherHelm_BinaryData
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_opengl
)

juce_generate_juce_header(AetherHelm)

# ==============================================================================
# MCP Server Target: aetherhelm-mcp
# ==============================================================================
juce_add_console_app(aetherhelm-mcp
    VERSION "1.0.0"
    PRODUCT_NAME "aetherhelm-mcp"
)

target_sources(aetherhelm-mcp
    PRIVATE
        src/mcp/main.cpp
        src/mcp/aetherhelm_mcp_server.cpp
        ${MOPO_SOURCES}
        ${SYNTHESIS_SOURCES}
        ${HEADLESS_COMMON_SOURCES}
)

target_include_directories(aetherhelm-mcp
    PRIVATE
        ${AETHERHELM_INCLUDE_DIRS}
)

target_compile_definitions(aetherhelm-mcp
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(aetherhelm-mcp
    PRIVATE
        AetherHelm_BinaryData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_gui_basics
)

juce_generate_juce_header(aetherhelm-mcp)

# ==============================================================================
# Automated Tests: aetherhelm-tests
# ==============================================================================
enable_testing()

juce_add_console_app(aetherhelm-tests
    VERSION "1.0.0"
    PRODUCT_NAME "aetherhelm-tests"
)

target_sources(aetherhelm-tests
    PRIVATE
        tests/aetherhelm_tests.cpp
        src/mcp/aetherhelm_mcp_server.cpp
        ${MOPO_SOURCES}
        ${SYNTHESIS_SOURCES}
        ${HEADLESS_COMMON_SOURCES}
)

target_include_directories(aetherhelm-tests
    PRIVATE
        ${AETHERHELM_INCLUDE_DIRS}
)

target_compile_definitions(aetherhelm-tests
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(aetherhelm-tests
    PRIVATE
        AetherHelm_BinaryData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_gui_basics
)

juce_generate_juce_header(aetherhelm-tests)

add_test(NAME AetherHelmTestSuite COMMAND aetherhelm-tests)
