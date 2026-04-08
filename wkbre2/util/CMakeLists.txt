add_library(util)

target_sources(
  util
  PRIVATE GSFileParser.cpp vecmat.cpp util.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
      include/util/util.h
      include/util/vecmat.h
      include/util/BinaryReader.h
      include/util/DynArray.h
      include/util/GSFileParser.h
      include/util/IndexedStringList.h
      include/util/StriCompare.h
      include/util/TagDict.h
)
