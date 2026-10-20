cmake_minimum_required(VERSION 3.22)
project(NullGlitch VERSION 1.2.4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# macOS defaults. A -D on the command line wins, so one build can be Intel,
# Apple Silicon, or both. Other platforms ignore these.
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    endif()
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NullGlitch
    COMPANY_NAME "Logic Beach"
    BUNDLE_ID xyz.logicbeach.clicks
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE LgBc
    PLUGIN_CODE LbCk
    FORMATS VST3
    PRODUCT_NAME "NullAtomo"
    VST3_CATEGORIES Instrument Synth)

juce_add_binary_data(NullGlitchData
    SOURCES
        Resources/Chicago12.otf
        Resources/starfield.png
        Resources/nullatomo.png
        Resources/samrobot.jpg
        Resources/ui/backgroundmain.png
        Resources/ui/backgroundoutput.png
        Resources/ui/bar.png
        Resources/ui/faderbar.png
        Resources/ui/faderknob.png
        Resources/ui/knob1.png
        Resources/ui/knob2.png
        Resources/ui/knob3.png
        Resources/ui/knob4.png
        Resources/ui/knob5.png
        Resources/ui/knob6.png
        Resources/ui/knobbig.png
        Resources/ui/knobface.png
        Resources/ui/knobtape.png
        Resources/ui/switch.png
        Resources/ui/tape.png
        Resources/ui/wavenoise.png
        Resources/ui/wavesaw.png
        Resources/ui/wavesine.png
        Resources/ui/wavesquare.png
        Resources/ui/wavetriangle.png)

set(SAM_SRC
    third_party/SAM/src/sam.c
    third_party/SAM/src/render.c
    third_party/SAM/src/reciter.c
    third_party/SAM/src/debug.c)

target_sources(NullGlitch
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/SamVoice.cpp
        Source/ESpeakVoice.cpp
        ${SAM_SRC})

# eSpeak NG 1.52.0, GPL-3.0. The speech data ships in the VST3 Resources folder.
FetchContent_Declare(
    espeakng
    GIT_REPOSITORY https://github.com/espeak-ng/espeak-ng.git
    GIT_TAG 1.52.0
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(espeakng)
if(NOT espeakng_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(espeakng)
endif()

set(ESPEAK_CONFIG_DIR ${CMAKE_BINARY_DIR}/espeak-config)
file(MAKE_DIRECTORY ${ESPEAK_CONFIG_DIR})
if(WIN32)
    set(ESPEAK_MKSTEMP 0)
else()
    set(ESPEAK_MKSTEMP 1)
endif()
file(WRITE ${ESPEAK_CONFIG_DIR}/config.h
"#define HAVE_MKSTEMP ${ESPEAK_MKSTEMP}
#define USE_ASYNC 0
#define USE_KLATT 1
#define USE_LIBPCAUDIO 0
#define USE_LIBSONIC 0
#define USE_MBROLA 0
#define USE_SPEECHPLAYER 0
#define PACKAGE_VERSION \"1.52.0\"
")

set(ESPEAK_SRC_DIR ${espeakng_SOURCE_DIR}/src/libespeak-ng)
set(ESPEAK_SOURCES
    ${ESPEAK_SRC_DIR}/common.c
    ${ESPEAK_SRC_DIR}/mnemonics.c
    ${ESPEAK_SRC_DIR}/error.c
    ${ESPEAK_SRC_DIR}/ieee80.c
    ${ESPEAK_SRC_DIR}/compiledata.c
    ${ESPEAK_SRC_DIR}/compiledict.c
    ${ESPEAK_SRC_DIR}/dictionary.c
    ${ESPEAK_SRC_DIR}/encoding.c
    ${ESPEAK_SRC_DIR}/intonation.c
    ${ESPEAK_SRC_DIR}/langopts.c
    ${ESPEAK_SRC_DIR}/numbers.c
    ${ESPEAK_SRC_DIR}/phoneme.c
    ${ESPEAK_SRC_DIR}/phonemelist.c
    ${ESPEAK_SRC_DIR}/readclause.c
    ${ESPEAK_SRC_DIR}/setlengths.c
    ${ESPEAK_SRC_DIR}/soundicon.c
    ${ESPEAK_SRC_DIR}/spect.c
    ${ESPEAK_SRC_DIR}/ssml.c
    ${ESPEAK_SRC_DIR}/synthdata.c
    ${ESPEAK_SRC_DIR}/synthesize.c
    ${ESPEAK_SRC_DIR}/tr_languages.c
    ${ESPEAK_SRC_DIR}/translate.c
    ${ESPEAK_SRC_DIR}/translateword.c
    ${ESPEAK_SRC_DIR}/voices.c
    ${ESPEAK_SRC_DIR}/wavegen.c
    ${ESPEAK_SRC_DIR}/speech.c
    ${ESPEAK_SRC_DIR}/espeak_api.c
    ${ESPEAK_SRC_DIR}/klatt.c
    ${espeakng_SOURCE_DIR}/src/ucd-tools/src/case.c
    ${espeakng_SOURCE_DIR}/src/ucd-tools/src/categories.c
    ${espeakng_SOURCE_DIR}/src/ucd-tools/src/ctype.c
    ${espeakng_SOURCE_DIR}/src/ucd-tools/src/proplist.c
    ${espeakng_SOURCE_DIR}/src/ucd-tools/src/scripts.c
    ${espeakng_SOURCE_DIR}/src/ucd-tools/src/tostring.c)

add_library(espeak_static STATIC ${ESPEAK_SOURCES})
if(MSVC)
    set_source_files_properties(${ESPEAK_SOURCES} PROPERTIES COMPILE_FLAGS "/w")
else()
    set_source_files_properties(${ESPEAK_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(espeak_static PUBLIC m)
endif()
target_include_directories(espeak_static BEFORE PRIVATE
    ${ESPEAK_CONFIG_DIR}
    ${espeakng_SOURCE_DIR}/src/include
    ${espeakng_SOURCE_DIR}/src/include/compat
    ${ESPEAK_SRC_DIR}
    ${espeakng_SOURCE_DIR}/src/ucd-tools/src/include)
target_compile_definitions(espeak_static PUBLIC
    "PATH_ESPEAK_DATA=\".\""
    "LIBESPEAK_NG_EXPORT=1")
set_target_properties(espeak_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(espeak_data_bin ${espeakng_SOURCE_DIR}/src/espeak-ng.c)
target_link_libraries(espeak_data_bin PRIVATE espeak_static)
target_include_directories(espeak_data_bin BEFORE PRIVATE
    ${ESPEAK_CONFIG_DIR}
    ${espeakng_SOURCE_DIR}/src/include)
if(WIN32)
    # The dictionary tool is a small POSIX program. MSVC needs eSpeak's
    # getopt, plus stand-ins for fcntl.h and S_ISFIFO.
    target_sources(espeak_data_bin PRIVATE ${espeakng_SOURCE_DIR}/src/compat/getopt.c)
    target_include_directories(espeak_data_bin BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/Source/msvc)
    target_compile_options(espeak_data_bin PRIVATE /FI espeak_win.h /w)
endif()

set(ESPEAK_DATA_OUT ${CMAKE_BINARY_DIR}/espeak-data-out)
set(ESPEAK_DATA_DIR ${ESPEAK_DATA_OUT}/espeak-ng-data)
add_custom_command(
    OUTPUT
        ${ESPEAK_DATA_DIR}/intonations
        ${ESPEAK_DATA_DIR}/phondata
        ${ESPEAK_DATA_DIR}/en_dict
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${ESPEAK_DATA_OUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ESPEAK_DATA_DIR}/voices
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${espeakng_SOURCE_DIR}/espeak-ng-data/lang ${ESPEAK_DATA_DIR}/lang
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${espeakng_SOURCE_DIR}/espeak-ng-data/voices/!v ${ESPEAK_DATA_DIR}/voices/!v
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${espeakng_SOURCE_DIR}/phsource ${ESPEAK_DATA_OUT}/phsource
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${espeakng_SOURCE_DIR}/dictsource ${ESPEAK_DATA_OUT}/dictsource
    COMMAND ${CMAKE_COMMAND} -E env ESPEAK_DATA_PATH=${ESPEAK_DATA_OUT}
        ${CMAKE_COMMAND} -E chdir ${ESPEAK_DATA_OUT}/phsource
        $<TARGET_FILE:espeak_data_bin> --compile-intonations
    COMMAND ${CMAKE_COMMAND} -E env ESPEAK_DATA_PATH=${ESPEAK_DATA_OUT}
        ${CMAKE_COMMAND} -E chdir ${ESPEAK_DATA_OUT}/phsource
        $<TARGET_FILE:espeak_data_bin> --compile-phonemes
    COMMAND ${CMAKE_COMMAND} -E env ESPEAK_DATA_PATH=${ESPEAK_DATA_OUT}
        ${CMAKE_COMMAND} -E chdir ${ESPEAK_DATA_OUT}/dictsource
        $<TARGET_FILE:espeak_data_bin> --compile=en
    DEPENDS espeak_data_bin
    COMMENT "Compile English eSpeak data"
    VERBATIM)
add_custom_target(espeak_data DEPENDS
    ${ESPEAK_DATA_DIR}/intonations
    ${ESPEAK_DATA_DIR}/phondata
    ${ESPEAK_DATA_DIR}/en_dict)
add_dependencies(NullGlitch_VST3 espeak_data)

if(MSVC)
    set_source_files_properties(${SAM_SRC} PROPERTIES COMPILE_FLAGS "/w")
else()
    set_source_files_properties(${SAM_SRC} PROPERTIES COMPILE_FLAGS "-w -fcommon")
endif()

target_include_directories(NullGlitch PRIVATE
    third_party/SAM/src
    ${espeakng_SOURCE_DIR}/src/include)

target_compile_definitions(NullGlitch
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(NullGlitch
    PRIVATE
        NullGlitchData
        espeak_static
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_custom_command(TARGET NullGlitch_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ESPEAK_DATA_DIR}"
        "$<TARGET_FILE_DIR:NullGlitch_VST3>/../Resources/espeak-ng-data")

juce_generate_juce_header(NullGlitch)
