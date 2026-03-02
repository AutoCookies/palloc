# Build palloc-redirect.dll (and import library) for Windows.
# Used when PA_WIN_REDIRECT=ON. PALLOC_REDIRECT_SUFFIX must be set by the parent
# (e.g. "", "32", "-arm64", "-arm64ec").
cmake_minimum_required(VERSION 3.18)
if(NOT WIN32)
  return()
endif()

project(palloc-redirect NONE)
# Avoid adding to ALL_BUILD etc. if we are not the main project
if(CMAKE_PROJECT_NAME STREQUAL "libpalloc")
  set(REDIRECT_SUFFIX "${PALLOC_REDIRECT_SUFFIX}" CACHE INTERNAL "")
else()
  set(REDIRECT_SUFFIX "$ENV{PALLOC_REDIRECT_SUFFIX}" CACHE STRING "Redirect DLL name suffix")
  if(NOT REDIRECT_SUFFIX)
    set(REDIRECT_SUFFIX "")
  endif()
endif()

add_library(palloc-redirect SHARED palloc-redirect.c)
set_target_properties(palloc-redirect PROPERTIES
  OUTPUT_NAME "palloc-redirect${REDIRECT_SUFFIX}"
)
if(MSVC)
  target_compile_options(palloc-redirect PRIVATE /MD /W3)
  set_target_properties(palloc-redirect PROPERTIES
    ARCHIVE_OUTPUT_NAME "palloc-redirect${REDIRECT_SUFFIX}"
  )
elseif(MINGW)
  target_compile_options(palloc-redirect PRIVATE -O2 -Wall)
endif()

message(STATUS "palloc-redirect: building palloc-redirect${REDIRECT_SUFFIX}.dll")
