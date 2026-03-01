#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "palloc" for configuration "Release"
set_property(TARGET palloc APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(palloc PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libpalloc.so.2.2"
  IMPORTED_SONAME_RELEASE "libpalloc.so.2"
  )

list(APPEND _cmake_import_check_targets palloc )
list(APPEND _cmake_import_check_files_for_palloc "${_IMPORT_PREFIX}/lib/libpalloc.so.2.2" )

# Import target "palloc-static" for configuration "Release"
set_property(TARGET palloc-static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(palloc-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/palloc-2.2/libpalloc.a"
  )

list(APPEND _cmake_import_check_targets palloc-static )
list(APPEND _cmake_import_check_files_for_palloc-static "${_IMPORT_PREFIX}/lib/palloc-2.2/libpalloc.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
