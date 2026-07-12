add_executable(FontRoleResolverTest
  FontRoleResolverTest.cpp
)

target_link_libraries(FontRoleResolverTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(FontRoleResolverTest)
