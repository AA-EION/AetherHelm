# CMake generated Testfile for 
# Source directory: /helm
# Build directory: /helm/build-mingw
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[AetherHelmTestSuite]=] "/helm/build-mingw/aetherhelm-tests_artefacts/Release/aetherhelm-tests.exe")
set_tests_properties([=[AetherHelmTestSuite]=] PROPERTIES  _BACKTRACE_TRIPLES "/helm/CMakeLists.txt;391;add_test;/helm/CMakeLists.txt;0;")
subdirs("_deps/juce-build")
