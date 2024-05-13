if(aerospike_FOUND)
  return()
endif()

set(aerospike_FOUND FALSE)

  if(NOT aerospike_FOUND)
    find_library(AEROSPIKE_LIBRARY aerospike)
    find_path(AEROSPIKE_INCLUDE_DIR aerospike/aerospike.h)
    if(AEROSPIKE_LIBRARY AND AEROSPIKE_INCLUDE_DIR)
      message(STATUS "Using aerospike from ${AEROSPIKE_LIBRARY}")
      set(aerospike_FOUND TRUE)
    endif()
  endif()

  if(aerospike_FOUND)
    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)

    mark_as_advanced(AEROSPIKE_INCLUDE_DIR AEROSPIKE_LIBRARY)
    add_library(AEROSPIKE::AEROSPIKE UNKNOWN IMPORTED)
    set_target_properties(
      AEROSPIKE::AEROSPIKE
      PROPERTIES
      IMPORTED_LOCATION "${AEROSPIKE_LIBRARY}"
      INTERFACE_COMPILE_OPTIONS "${AEROSPIKE_CFLAGS_OTHER}"
      INTERFACE_INCLUDE_DIRECTORIES "${AEROSPIKE_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto;ZLIB::ZLIB;${CMAKE_DL_LIBS}"
    )
    if(WIN32 AND STATIC_LINK)
      target_link_libraries(AEROSPIKE::AEROSPIKE INTERFACE ws2_32)
    endif()
    set(aerospike_FOUND TRUE)
    set(target AEROSPIKE::AEROSPIKE)
  else()
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
      aerospike
      REQUIRED_VARS AEROSPIKE_LIBRARY AEROSPIKE_INCLUDE_DIR
    )
  endif()

include(FeatureSummary)
set_package_properties(
  aerospike
  PROPERTIES
  URL "https://github.com/aerospike/aerospike-client-c"
  DESCRIPTION "C interface for interacting with the Aerospike Database"
)
