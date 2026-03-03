# CMake generated Testfile for 
# Source directory: /home/autocookie/pomaieco/palloc
# Build directory: /home/autocookie/pomaieco/palloc/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-api "/home/autocookie/pomaieco/palloc/build/palloc-test-api")
set_tests_properties(test-api PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;770;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-api-fill "/home/autocookie/pomaieco/palloc/build/palloc-test-api-fill")
set_tests_properties(test-api-fill PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;770;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-stress "/home/autocookie/pomaieco/palloc/build/palloc-test-stress")
set_tests_properties(test-stress PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;770;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-arena-pomai "/home/autocookie/pomaieco/palloc/build/palloc-test-arena-pomai")
set_tests_properties(test-arena-pomai PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;770;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-basic "/home/autocookie/pomaieco/palloc/build/palloc-test-basic")
set_tests_properties(test-basic PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;770;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-stress-dynamic "/usr/bin/cmake" "-E" "env" "PALLOC_VERBOSE=1" "LD_PRELOAD=/home/autocookie/pomaieco/palloc/build/libpalloc.so.2.2" "/home/autocookie/pomaieco/palloc/build/palloc-test-stress-dynamic")
set_tests_properties(test-stress-dynamic PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;790;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
