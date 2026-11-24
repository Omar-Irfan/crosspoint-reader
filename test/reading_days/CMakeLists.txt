add_executable(ReadingDaysTest
  ReadingDaysTest.cpp
)

target_include_directories(ReadingDaysTest PRIVATE
  ${REPO_ROOT}/src/util
)

target_link_libraries(ReadingDaysTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReadingDaysTest)
