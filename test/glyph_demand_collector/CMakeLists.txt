add_executable(GlyphDemandCollectorTest
  GlyphDemandCollectorTest.cpp
  ${REPO_ROOT}/lib/GfxRenderer/GlyphDemandCollector.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(GlyphDemandCollectorTest PRIVATE
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(GlyphDemandCollectorTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(GlyphDemandCollectorTest)
