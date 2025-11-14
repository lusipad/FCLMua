add_test([=[FCL_PROFILER.basic]=]  /home/user/FCLMua/build/test/test_fcl_profiler [==[--gtest_filter=FCL_PROFILER.basic]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FCL_PROFILER.basic]=]  PROPERTIES WORKING_DIRECTORY /home/user/FCLMua/build/test SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  test_fcl_profiler_TESTS FCL_PROFILER.basic)
