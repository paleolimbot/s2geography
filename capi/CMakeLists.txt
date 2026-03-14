add_library(
  s2geography_c
  src/s2geography_c.cc)

target_include_directories(
  s2geography_c
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_link_libraries(s2geography_c PRIVATE s2geography)

set_target_properties(s2geography_c PROPERTIES
  C_STANDARD 11
  CXX_STANDARD 17
  POSITION_INDEPENDENT_CODE ON)

if(S2GEOGRAPHY_BUILD_TESTS)
  add_executable(s2geography_c_test test/s2geography_c_test.cc)
  target_link_libraries(s2geography_c_test s2geography_c GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(s2geography_c_test)

  # Pure C test — compiled as C to verify the API is callable from C
  add_executable(s2geography_c_api_test test/s2geography_c_api_test.c)
  set_target_properties(s2geography_c_api_test PROPERTIES
    C_STANDARD 11
    LINKER_LANGUAGE C)
  target_link_libraries(s2geography_c_api_test s2geography_c)
  add_test(NAME s2geography_c_api_test COMMAND s2geography_c_api_test)
endif()
