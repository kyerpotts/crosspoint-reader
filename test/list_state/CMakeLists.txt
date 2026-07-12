add_executable(ListStateTest
  ListStateTest.cpp
)

target_link_libraries(ListStateTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ListStateTest)
