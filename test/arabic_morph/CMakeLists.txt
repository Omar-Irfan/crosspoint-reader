add_executable(ArabicMorphTest
  ArabicMorphTest.cpp
  ${REPO_ROOT}/lib/ArabicMorph/ArabicMorph.cpp
)

target_include_directories(ArabicMorphTest PRIVATE
  ${REPO_ROOT}/lib/ArabicMorph
)

target_link_libraries(ArabicMorphTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ArabicMorphTest)
