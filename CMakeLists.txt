cmake_minimum_required(VERSION 3.21)
project(Intona LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Pdf PdfWidgets)
qt_standard_project_setup()

set(INTONA_SOURCES
  src/main.cpp
  src/MainWindow.h
  src/MainWindow.cpp
  src/StyleUtils.h
  src/StyleUtils.cpp
  src/Config.hpp
  src/CentsUtilities.hpp
  src/StringUtilities.hpp
  src/Chords.h
  src/Chords.cpp
  src/ChordRecognizer.h
  src/ChordRecognizer.cpp
  src/TuningCenterFinder.h
  src/TuningCenterFinder.cpp
  src/About.h

  ../Common/src/ui/widgets/MidiChannelSelector.h
  ../Common/src/ui/widgets/MidiChannelSelector.cpp
  ../Common/src/ui/widgets/ManualWidget.h
  ../Common/src/ui/widgets/ManualWidget.cpp

  ../Common/src/midi/MidiMessage.h

  ../Common/src/midi/IMidiOut.h
  ../Common/src/midi/MidiOutFactory.h
  ../Common/src/midi/MidiOutFactory.cpp

  ../Common/src/midi/IMidiIn.h
  ../Common/src/midi/MidiInFactory.h
  ../Common/src/midi/MidiInFactory.cpp

  ../Common/src/midi/MidiMonoIn.h
  src/midi/MidiMonoIn.cpp

  src/ui/tabs/MidiSettingsTab.h
  src/ui/tabs/MidiSettingsTab.cpp

  src/ui/tabs/SurfaceTab.h
  src/ui/tabs/SurfaceTab.cpp

  src/ui/widgets/NtetCircleWidget.h
  src/ui/widgets/NtetCircleWidget.cpp

  src/ui/widgets/ConfigPresetListWidget.h
  src/ui/widgets/ConfigPresetListWidget.cpp
)

if(WIN32)
  list(APPEND INTONA_SOURCES
    ../Common/src/midi/MidiOut_WinMM.h
    ../Common/src/midi/MidiOut_WinMM.cpp
    ../Common/src/midi/MidiIn_WinMM.h
    ../Common/src/midi/MidiIn_WinMM.cpp
  )
else()
  list(APPEND INTONA_SOURCES
    ../Common/src/midi/MidiOut_Stub.h
    ../Common/src/midi/MidiOut_Stub.cpp
    ../Common/src/midi/MidiIn_Stub.h
    ../Common/src/midi/MidiIn_Stub.cpp
  )
endif()

if(ANDROID)
  qt_add_executable(Intona
    MANUAL_FINALIZATION
    ${INTONA_SOURCES}
  )
else()
  add_executable(Intona
    ${INTONA_SOURCES}
  )
endif()

set_source_files_properties(
  src/resources/qdarkstyle/darkstyle.qss
  PROPERTIES QT_RESOURCE_ALIAS "qdarkstyle/darkstyle.qss"
)

set_source_files_properties(
  src/resources/images/keyboard.png
  PROPERTIES QT_RESOURCE_ALIAS "images/keyboard.png"
)

set_source_files_properties(
  src/resources/manual/IntonaUserManual.pdf
  PROPERTIES QT_RESOURCE_ALIAS "manual/IntonaUserManual.pdf"
)

qt_add_resources(Intona IntonaResources
  PREFIX "/"
  FILES
    src/resources/qdarkstyle/darkstyle.qss
    src/resources/images/keyboard.png
    src/resources/manual/IntonaUserManual.pdf
)

target_link_libraries(Intona PRIVATE Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets)

if(WIN32)
  target_link_libraries(Intona PRIVATE winmm)
endif()

qt_finalize_executable(Intona)
