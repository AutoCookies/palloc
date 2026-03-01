vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO microsoft/palloc
  HEAD_REF master

  # The "REF" can be a commit hash, branch name (dev3), or a version (v3.2.7).
  REF "v${VERSION}"
  # REF e2db21e9ba9fb9172b7b0aa0fe9b8742525e8774

  # The sha512 is the hash of the tar.gz bundle.
  # (To get the sha512, run `vcpkg install "palloc[override]" --overlay-ports=<dir of this file>` and copy the sha from the error message.)
  SHA512 5830ceb1bf0d02f50fe586caaad87624ba8eba1bb66e68e8201894221cf6f51854f5a9667fc98358c3b430dae6f9bf529bfcb74d42debe6f40a487265053371c
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
  FEATURES
    c           PA_NO_USE_CXX
    guarded     PA_GUARDED
    secure      PA_SECURE
    override    PA_OVERRIDE
    optarch     PA_OPT_ARCH
    nooptarch   PA_NO_OPT_ARCH
    optsimd     PA_OPT_SIMD
    xmalloc     PA_XMALLOC
    asm         PA_SEE_ASM
)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" PA_BUILD_STATIC)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" PA_BUILD_SHARED)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    -DPA_USE_CXX=ON
    -DPA_BUILD_TESTS=OFF
    -DPA_BUILD_OBJECT=ON
    -DPA_BUILD_STATIC=${PA_BUILD_STATIC}
    -DPA_BUILD_SHARED=${PA_BUILD_SHARED}
    -DPA_INSTALL_TOPLEVEL=ON
    ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()

file(COPY
  "${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake"
  "${CMAKE_CURRENT_LIST_DIR}/usage"
  DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/palloc)

if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
  # todo: why is this needed?
  vcpkg_replace_string(
    "${CURRENT_PACKAGES_DIR}/include/palloc.h"
    "!defined(PA_SHARED_LIB)"
    "0 // !defined(PA_SHARED_LIB)"
  )
endif()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_fixup_pkgconfig()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
