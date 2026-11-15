set(GNARL_COMMON_PLUGIN_ARGS
  COMPANY_NAME "GNARL Audio"
  COMPANY_WEBSITE "https://github.com/dgaster7/gnarl"
  PLUGIN_MANUFACTURER_CODE Gnrl
  VERSION ${GNARL_VERSION}
  COPY_PLUGIN_AFTER_BUILD ${GNARL_COPY_PLUGINS}
  VST3_AUTO_MANIFEST FALSE
  HARDENED_RUNTIME_ENABLED FALSE)

juce_add_plugin(GNARL
  ${GNARL_COMMON_PLUGIN_ARGS}
  PRODUCT_NAME "GNARL"
  PLUGIN_NAME "GNARL"
  DESCRIPTION "Bass design synthesizer"
  PLUGIN_CODE Gsyn
  BUNDLE_ID audio.gnarl.synth
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  VST3_CATEGORIES Instrument Synth
  FORMATS VST3 AU Standalone)
target_sources(GNARL PRIVATE synth/SynthEntry.cpp)
target_link_libraries(GNARL PRIVATE gnarl_app)

juce_add_plugin(GNARL_FX
  ${GNARL_COMMON_PLUGIN_ARGS}
  PRODUCT_NAME "GNARL FX"
  PLUGIN_NAME "GNARL FX"
  DESCRIPTION "Bass design effect"
  PLUGIN_CODE Gfxp
  BUNDLE_ID audio.gnarl.fx
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  AU_MAIN_TYPE kAudioUnitType_MusicEffect
  VST3_CATEGORIES Fx Distortion
  FORMATS VST3 AU)
target_sources(GNARL_FX PRIVATE fx/FxEntry.cpp)
target_link_libraries(GNARL_FX PRIVATE gnarl_app)

# Factory bank embedded in both plugins (flattened as <Plugin>__<Category>__<Name>.gnarl)
file(GLOB_RECURSE GNARL_FACTORY_PRESETS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/content/presets/*.gnarl)
set(GNARL_FACTORY_FLAT)
foreach(f ${GNARL_FACTORY_PRESETS})
  file(RELATIVE_PATH rel ${CMAKE_SOURCE_DIR}/content/presets ${f})
  string(REPLACE "/" "__" flat "${rel}")
  set(out ${CMAKE_BINARY_DIR}/factory_flat/${flat})
  configure_file(${f} ${out} COPYONLY)
  list(APPEND GNARL_FACTORY_FLAT ${out})
endforeach()
if(GNARL_FACTORY_FLAT)
  juce_add_binary_data(GnarlFactoryData HEADER_NAME GnarlFactoryData.h NAMESPACE GnarlFactoryData SOURCES ${GNARL_FACTORY_FLAT})
  target_link_libraries(GNARL PRIVATE GnarlFactoryData)
  target_link_libraries(GNARL_FX PRIVATE GnarlFactoryData)
endif()
