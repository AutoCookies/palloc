# CMake generated Testfile for 
# Source directory: /root/palloc
# Build directory: /root/palloc/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-api "/root/palloc/build/palloc-test-api")
set_tests_properties(test-api PROPERTIES  _BACKTRACE_TRIPLES "/root/palloc/CMakeLists.txt;875;add_test;/root/palloc/CMakeLists.txt;0;")
add_test(test-api-fill "/root/palloc/build/palloc-test-api-fill")
set_tests_properties(test-api-fill PROPERTIES  _BACKTRACE_TRIPLES "/root/palloc/CMakeLists.txt;875;add_test;/root/palloc/CMakeLists.txt;0;")
add_test(test-stress "/root/palloc/build/palloc-test-stress")
set_tests_properties(test-stress PROPERTIES  _BACKTRACE_TRIPLES "/root/palloc/CMakeLists.txt;875;add_test;/root/palloc/CMakeLists.txt;0;")
add_test(test-arena-pomai "/root/palloc/build/palloc-test-arena-pomai")
set_tests_properties(test-arena-pomai PROPERTIES  _BACKTRACE_TRIPLES "/root/palloc/CMakeLists.txt;875;add_test;/root/palloc/CMakeLists.txt;0;")
add_test(test-basic "/root/palloc/build/palloc-test-basic")
set_tests_properties(test-basic PROPERTIES  _BACKTRACE_TRIPLES "/root/palloc/CMakeLists.txt;875;add_test;/root/palloc/CMakeLists.txt;0;")
add_test(test-stress-dynamic "/usr/bin/cmake" "-E" "env" "PALLOC_VERBOSE=1" "LD_PRELOAD=/root/palloc/build/libpalloc.so.2.2" "/root/palloc/build/palloc-test-stress-dynamic")
set_tests_properties(test-stress-dynamic PROPERTIES  _BACKTRACE_TRIPLES "/root/palloc/CMakeLists.txt;895;add_test;/root/palloc/CMakeLists.txt;0;")
