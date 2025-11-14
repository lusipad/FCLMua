add_test([=[HalfspaceConvexPrimitiveTest.CollisionTests]=]  /home/user/FCLMua/build/test/narrowphase/detail/primitive_shape_algorithm/test_half_space_convex [==[--gtest_filter=HalfspaceConvexPrimitiveTest.CollisionTests]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[HalfspaceConvexPrimitiveTest.CollisionTests]=]  PROPERTIES WORKING_DIRECTORY /home/user/FCLMua/build/test/narrowphase/detail/primitive_shape_algorithm SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  test_half_space_convex_TESTS HalfspaceConvexPrimitiveTest.CollisionTests)
