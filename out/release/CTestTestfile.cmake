# CMake generated Testfile for 
# Source directory: /home/autocookie/pomaieco/palloc
# Build directory: /home/autocookie/pomaieco/palloc/out/release
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-api "/home/autocookie/pomaieco/palloc/out/release/palloc-test-api")
set_tests_properties(test-api PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;739;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-api-fill "/home/autocookie/pomaieco/palloc/out/release/palloc-test-api-fill")
set_tests_properties(test-api-fill PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;739;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-stress "/home/autocookie/pomaieco/palloc/out/release/palloc-test-stress")
set_tests_properties(test-stress PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;739;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
add_test(test-stress-dynamic "/usr/bin/cmake" "-E" "env" "PALLOC_VERBOSE=1" "LD_PRELOAD=/home/autocookie/pomaieco/palloc/out/release/libpalloc.so.2.2" "/home/autocookie/pomaieco/palloc/out/release/palloc-test-stress-dynamic")
set_tests_properties(test-stress-dynamic PROPERTIES  _BACKTRACE_TRIPLES "/home/autocookie/pomaieco/palloc/CMakeLists.txt;759;add_test;/home/autocookie/pomaieco/palloc/CMakeLists.txt;0;")
