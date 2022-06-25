cmake_minimum_required (VERSION 3.13)

target_sources(${TARGET} PRIVATE
    "DynArray.h"
    "vecmat.cpp" "vecmat.h"
    "TagDict.h"
    "IndexedStringList.h"
    "util.cpp" "util.h"
    "GSFileParser.cpp" "GSFileParser.h")
