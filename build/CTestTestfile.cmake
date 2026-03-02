# CMake generated Testfile for 
# Source directory: /root/pomaieco/palloc
# Build directory: /root/pomaieco/palloc/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-api "/root/pomaieco/palloc/build/palloc-test-api")
set_tests_properties(test-api PROPERTIES  _BACKTRACE_TRIPLES "/root/pomaieco/palloc/CMakeLists.txt;754;add_test;/root/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-api-fill "/root/pomaieco/palloc/build/palloc-test-api-fill")
set_tests_properties(test-api-fill PROPERTIES  _BACKTRACE_TRIPLES "/root/pomaieco/palloc/CMakeLists.txt;754;add_test;/root/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-stress "/root/pomaieco/palloc/build/palloc-test-stress")
set_tests_properties(test-stress PROPERTIES  _BACKTRACE_TRIPLES "/root/pomaieco/palloc/CMakeLists.txt;754;add_test;/root/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-arena-pomai "/root/pomaieco/palloc/build/palloc-test-arena-pomai")
set_tests_properties(test-arena-pomai PROPERTIES  _BACKTRACE_TRIPLES "/root/pomaieco/palloc/CMakeLists.txt;754;add_test;/root/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-basic "/root/pomaieco/palloc/build/palloc-test-basic")
set_tests_properties(test-basic PROPERTIES  _BACKTRACE_TRIPLES "/root/pomaieco/palloc/CMakeLists.txt;754;add_test;/root/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-stress-dynamic "/usr/bin/cmake" "-E" "env" "PALLOC_VERBOSE=1" "LD_PRELOAD=/root/pomaieco/palloc/build/libpalloc.so.2.2" "/root/pomaieco/palloc/build/palloc-test-stress-dynamic")
set_tests_properties(test-stress-dynamic PROPERTIES  _BACKTRACE_TRIPLES "/root/pomaieco/palloc/CMakeLists.txt;774;add_test;/root/pomaieco/palloc/CMakeLists.txt;0;")
