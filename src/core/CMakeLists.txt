set(
  sources
  CacheEntry.cpp
  FileRecompressor.cpp
  FileRestorage.cpp
  Manifest.cpp
  MsvcShowIncludesOutput.cpp
  Result.cpp
  ResultExtractor.cpp
  ResultFiles.cpp
  ResultInspector.cpp
  ResultRetriever.cpp
  Statistics.cpp
  StatisticsCounters.cpp
  StatsLog.cpp
  mainoptions.cpp
  types.cpp
)

file(GLOB headers *.hpp)
list(APPEND sources ${headers})

target_sources(ccache_framework PRIVATE ${sources})
