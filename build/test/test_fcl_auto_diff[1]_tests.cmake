add_test([=[FCL_AUTO_DIFF.basic]=]  /home/user/FCLMua/build/test/test_fcl_auto_diff [==[--gtest_filter=FCL_AUTO_DIFF.basic]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FCL_AUTO_DIFF.basic]=]  PROPERTIES WORKING_DIRECTORY /home/user/FCLMua/build/test SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  test_fcl_auto_diff_TESTS FCL_AUTO_DIFF.basic)
