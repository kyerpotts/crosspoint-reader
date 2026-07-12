add_executable(FontRoleTest
  FontRoleTest.cpp
)


target_link_libraries(FontRoleTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(FontRoleTest)
