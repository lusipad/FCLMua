add_test([=[FCL_GENERAL.general]=]  /home/user/FCLMua/build/test/test_fcl_general [==[--gtest_filter=FCL_GENERAL.general]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FCL_GENERAL.general]=]  PROPERTIES WORKING_DIRECTORY /home/user/FCLMua/build/test SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  test_fcl_general_TESTS FCL_GENERAL.general)
