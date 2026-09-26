cmake_minimum_required(VERSION 3.21)
project(Intona LANGUAGES CXX)

# Ninja must parse the same /showIncludes language on every build, including
# builds started outside Visual Studio. Otherwise header edits can silently
# leave incompatible object files in the executable.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
  set(CMAKE_CL_SHOWINCLUDES_PREFIX "Note: including file: ")
  list(PREPEND CMAKE_CXX_COMPILER_LAUNCHER
    "${CMAKE_COMMAND}" "-E" "env" "VSLANG=1033")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
  Core
  Gui
  Svg
  Qml
  Quick
  QuickControls2
)

qt_standard_project_setup()

add_subdirectory(
  "${CMAKE_CURRENT_SOURCE_DIR}/external/Common"
  "${CMAKE_CURRENT_BINARY_DIR}/external/Common"
)

if(QT_KNOWN_POLICY_QTP0002)
  qt_policy(SET QTP0002 NEW)
endif()

if(QT_KNOWN_POLICY_QTP0005)
  qt_policy(SET QTP0005 NEW)
endif()

set(INTONA_SOURCES
  src/main.cpp
  src/Config.hpp
  src/CentsUtilities.hpp
  src/StringUtilities.hpp
  src/tuning/NoteNaming.h
  src/tuning/KeyboardMapping.h
  src/tuning/RelativeKeyboard.h
  src/tuning/RelativeKeyboard.cpp
  src/tuning/ScaleTriadAdapting.h
  src/tuning/ScaleTriadAdapting.cpp
  src/About.h
  
  src/tuning/TuningTypes.h
  src/tuning/TuningAlgorithms.h
  src/tuning/TuningAlgorithms.cpp
  src/tuning/TuningController.h
  src/tuning/TuningController.cpp
  src/tuning/TuningViewModel.h
  src/tuning/TuningViewModel.cpp
  src/tuning/TuningMidiOutput.h
  src/tuning/TuningMidiOutput.cpp

  src/midi/IntonaMidiConfiguration.h
  src/midi/MidiController.h
  src/midi/MidiController.cpp
  src/midi/MidiEventRecorder.h
  src/midi/MidiEventRecorder.cpp
  src/midi/MidiViewModel.h
  src/midi/MidiViewModel.cpp
)

if(ANDROID)
  set(QT_ANDROID_COMPILE_SDK_VERSION 34)
  set(QT_ANDROID_MIN_SDK_VERSION 23)
  set(QT_ANDROID_TARGET_SDK_VERSION 34)

  qt_add_executable(Intona
    MANUAL_FINALIZATION
    ${INTONA_SOURCES}
  )
else()
  add_executable(Intona
    ${INTONA_SOURCES}
  )
endif()

if(WIN32)
  target_sources(Intona PRIVATE
    src/resources/windows/Intona.rc
  )
endif()

qt_add_qml_module(Intona
  URI Intona
  VERSION 1.0
  IMPORTS
    TARGET NaadaLabUi
  RESOURCE_PREFIX /qt/qml
  NO_RESOURCE_TARGET_PATH
  QML_FILES
    src/qml/Main.qml
    src/qml/navigation/NavigationShell.qml
    src/qml/controls/MidiChannelGrid.qml
	src/qml/controls/TuningCircle.qml
    src/qml/controls/TuningKeyboard.qml
    src/qml/pages/MidiSetupPage.qml
    src/qml/pages/SettingsPage.qml
    src/qml/pages/TuningSurfacePage.qml
    src/qml/pages/UserManualPage.qml
    src/qml/pages/AboutPage.qml
)

set_source_files_properties(
  src/resources/images/keyboard.png
  PROPERTIES QT_RESOURCE_ALIAS "images/keyboard.png"
)

set_source_files_properties(
  src/resources/icons/Intona.svg
  PROPERTIES QT_RESOURCE_ALIAS "icons/Intona.svg"
)

set_source_files_properties(
  src/resources/manual/IntonaUserManual.html
  PROPERTIES QT_RESOURCE_ALIAS "manual/IntonaUserManual.html"
)

file(GLOB MANUAL_IMAGE_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/manual/images/*.png"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/manual/images/*.jpg"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/manual/images/*.jpeg"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/manual/images/*.svg"
)

foreach(manual_image ${MANUAL_IMAGE_FILES})
  get_filename_component(manual_image_name "${manual_image}" NAME)

  set_source_files_properties(
    "${manual_image}"
    PROPERTIES
      QT_RESOURCE_ALIAS "manual/images/${manual_image_name}"
  )
endforeach()

qt_add_resources(Intona IntonaResources
  PREFIX "/"
  FILES
    src/resources/icons/Intona.svg
    src/resources/images/keyboard.png
    src/resources/manual/IntonaUserManual.html
    ${MANUAL_IMAGE_FILES}
)

target_link_libraries(Intona PRIVATE
  NaadaLab::Midi
  NaadaLab::Ui
  NaadaLab::Manual
  Qt6::Svg
  Qt6::Qml
  Qt6::Quick
  Qt6::QuickControls2
)

if(ANDROID)
  naadalab_configure_android_midi_package(
    Intona
    "${CMAKE_CURRENT_SOURCE_DIR}/android"
  )

  set_property(TARGET Intona PROPERTY
    QT_ANDROID_VERSION_CODE 2
  )

  set_property(TARGET Intona PROPERTY
    QT_ANDROID_VERSION_NAME "1.0"
  )
endif()

qt_finalize_executable(Intona)

option(INTONA_BUILD_TESTS "Build note naming regression tests" OFF)
if(INTONA_BUILD_TESTS)
  enable_testing()
  set(INTONA_TEST_SOURCES ${INTONA_SOURCES})
  list(REMOVE_ITEM INTONA_TEST_SOURCES src/main.cpp)
  add_executable(IntonaNamingTests tests/NoteNamingTests.cpp tests/KeyboardMappingTests.cpp tests/RelativeKeyboardTests.cpp ${INTONA_TEST_SOURCES})
  target_include_directories(IntonaNamingTests PRIVATE src)
  target_link_libraries(IntonaNamingTests PRIVATE NaadaLab::Midi Qt6::Core)
  add_test(NAME IntonaNoteNaming COMMAND IntonaNamingTests)
  add_executable(IntonaScaleTriadTests tests/ScaleTriadTests.cpp ${INTONA_TEST_SOURCES})
  target_include_directories(IntonaScaleTriadTests PRIVATE src)
  target_compile_definitions(IntonaScaleTriadTests PRIVATE INTONA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(IntonaScaleTriadTests PRIVATE NaadaLab::Midi Qt6::Core)
  add_test(NAME IntonaScaleTriads COMMAND IntonaScaleTriadTests)
  add_executable(IntonaMidiRecordingTests tests/MidiRecordingTests.cpp
    src/midi/MidiEventRecorder.h src/midi/MidiEventRecorder.cpp)
  target_include_directories(IntonaMidiRecordingTests PRIVATE src)
  target_link_libraries(IntonaMidiRecordingTests PRIVATE Qt6::Core)
  add_test(NAME IntonaMidiRecording COMMAND IntonaMidiRecordingTests)
  add_executable(IntonaMidiStartupTests tests/MidiStartupTests.cpp
    src/midi/MidiController.h src/midi/MidiController.cpp)
  target_include_directories(IntonaMidiStartupTests PRIVATE src)
  target_link_libraries(IntonaMidiStartupTests PRIVATE NaadaLab::Midi Qt6::Core)
  add_test(NAME IntonaMidiStartup COMMAND IntonaMidiStartupTests)
  add_executable(IntonaSettingsPageTests tests/SettingsPageTests.cpp
    src/midi/MidiEventRecorder.h src/midi/MidiEventRecorder.cpp)
  target_include_directories(IntonaSettingsPageTests PRIVATE src)
  target_compile_definitions(IntonaSettingsPageTests PRIVATE
    INTONA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(IntonaSettingsPageTests PRIVATE NaadaLab::Ui Qt6::QuickControls2)
  add_test(NAME IntonaSettingsPage COMMAND IntonaSettingsPageTests)
  add_test(NAME IntonaStartup COMMAND Intona --startup-check -platform offscreen)
  set_tests_properties(IntonaStartup PROPERTIES TIMEOUT 20)
  # A deployed application folder may contain only the Windows platform plugin.
  # Tests also need the SDK's offscreen plugin and complete QML module set.
  if(TARGET Qt6::qmake AND TARGET Qt6::QOffscreenIntegrationPlugin)
    get_target_property(INTONA_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    execute_process(COMMAND "${INTONA_QMAKE_EXECUTABLE}" -query QT_INSTALL_QML
      OUTPUT_VARIABLE INTONA_QT_QML_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
      COMMAND_ERROR_IS_FATAL ANY)
    set_tests_properties(IntonaSettingsPage IntonaStartup PROPERTIES ENVIRONMENT
      "QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>;QML_IMPORT_PATH=${INTONA_QT_QML_DIR};QT_FORCE_STDERR_LOGGING=1")
  endif()
endif()
