add_executable(SdCardFontSlotsTest
  SdCardFontSlotsTest.cpp
  ${REPO_ROOT}/lib/EpdFont/SdCardFontManager.cpp
)
target_include_directories(SdCardFontSlotsTest BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/fakes
  ${REPO_ROOT}/lib/EpdFont
)
target_link_libraries(SdCardFontSlotsTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(SdCardFontSlotsTest)
