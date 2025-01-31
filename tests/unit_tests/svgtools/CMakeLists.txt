# Detray library, part of the ACTS project (R&D line)
#
# (c) 2021-2023 CERN for the benefit of the ACTS project
#
# Mozilla Public License Version 2.0

# Set up the core tests.

include_directories( SYSTEM $<TARGET_PROPERTY:actsvg::meta,INTERFACE_INCLUDE_DIRECTORIES> )
include_directories( SYSTEM $<TARGET_PROPERTY:actsvg::core,INTERFACE_INCLUDE_DIRECTORIES> )

detray_add_test( svgtools_masks
   "masks.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_surfaces
   "surfaces.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_volumes
   "volumes.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_detectors
   "detectors.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_groups
   "groups.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_intersections
   "intersections.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_trajectories
   "trajectories.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_grids
   "grids.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_land_marks
   "landmarks.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools)

detray_add_test( svgtools_web
   "web.cpp"
   LINK_LIBRARIES GTest::gtest_main detray_tests_common detray::core_array actsvg::core actsvg::meta detray::svgtools actsvg::web)
