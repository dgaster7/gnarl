file(GLOB_RECURSE GNARL_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(gnarl_app INTERFACE)
target_sources(gnarl_app INTERFACE ${GNARL_APP_SOURCES})
target_include_directories(gnarl_app INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gnarl_app INTERFACE gnarl_dsp monocypher
  juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography
  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
target_compile_definitions(gnarl_app INTERFACE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1 JUCE_MODAL_LOOPS_PERMITTED=0)
