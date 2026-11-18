add_executable(ArabicStemTest
  ArabicStemTest.cpp
  ${REPO_ROOT}/lib/ArabicStem/ArabicStem.cpp
)

target_include_directories(ArabicStemTest PRIVATE
  ${REPO_ROOT}/lib/ArabicStem
)

target_link_libraries(ArabicStemTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ArabicStemTest)
