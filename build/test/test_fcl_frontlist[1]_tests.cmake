add_test([=[FCL_FRONT_LIST.front_list]=]  /home/user/FCLMua/build/test/test_fcl_frontlist [==[--gtest_filter=FCL_FRONT_LIST.front_list]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FCL_FRONT_LIST.front_list]=]  PROPERTIES WORKING_DIRECTORY /home/user/FCLMua/build/test SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  test_fcl_frontlist_TESTS FCL_FRONT_LIST.front_list)
