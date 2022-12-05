set(
  sources
  FileStorage.cpp
  HttpStorage.cpp
  RemoteStorage.cpp
)

if(ARCHIVE_STORAGE_BACKEND)
  list(APPEND sources ArchiveStorage.cpp)
endif()
if(REDIS_STORAGE_BACKEND)
  list(APPEND sources RedisStorage.cpp)
endif()

file(GLOB headers *.hpp)
list(APPEND sources ${headers})

target_sources(ccache_framework PRIVATE ${sources})
