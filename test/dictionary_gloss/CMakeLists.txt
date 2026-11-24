add_executable(DictionaryGlossTest
  DictionaryGlossTest.cpp
  ${REPO_ROOT}/src/util/DictionaryGloss.cpp
  ${REPO_ROOT}/src/util/HtmlToPlainText.cpp
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
)

target_include_directories(DictionaryGlossTest PRIVATE
  ${REPO_ROOT}/src/util
  ${REPO_ROOT}/lib/Epub
)

target_link_libraries(DictionaryGlossTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(DictionaryGlossTest)
