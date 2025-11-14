add_test([=[ConfigurationFailureTest.ConfirmFormatting]=]  /home/user/FCLMua/build/test/narrowphase/detail/test_failed_at_this_configuration [==[--gtest_filter=ConfigurationFailureTest.ConfirmFormatting]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ConfigurationFailureTest.ConfirmFormatting]=]  PROPERTIES WORKING_DIRECTORY /home/user/FCLMua/build/test/narrowphase/detail SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  test_failed_at_this_configuration_TESTS ConfigurationFailureTest.ConfirmFormatting)
