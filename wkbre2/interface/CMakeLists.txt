cmake_minimum_required (VERSION 3.13)

target_sources(${TARGET} PRIVATE
    "ServerDebugger.cpp" "ServerDebugger.h"
    "ClientDebugger.cpp" "ClientDebugger.h"
    "ClientInterface.cpp" "ClientInterface.h"
    "GameSetDebugger.h" "GameSetDebugger.cpp"
    "QuickStartMenu.h" "QuickStartMenu.cpp"
)
