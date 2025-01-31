/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2020-2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Project include(s)
#include "detray/masks/masks.hpp"
#include "detray/masks/unmasked.hpp"
#include "detray/test/types.hpp"

// GTest include
#include <gtest/gtest.h>

using namespace detray;
using point3_t = test::point3;

constexpr scalar tol{1e-7f};

/// This tests the basic functionality of an unmasked plane
GTEST_TEST(detray_masks, unmasked) {
    point3_t p2 = {0.5f, -9.f, 0.f};

    mask<unmasked> u{};

    ASSERT_TRUE(u.is_inside(p2, 0.f) == intersection::status::e_inside);

    // Check bounding box
    constexpr scalar envelope{0.01f};
    const auto loc_bounds = u.local_min_bounds(envelope);
    ASSERT_TRUE(detail::is_invalid_value(loc_bounds[cuboid3D<>::e_min_x]));
    ASSERT_TRUE(detail::is_invalid_value(loc_bounds[cuboid3D<>::e_min_y]));
    ASSERT_TRUE(detail::is_invalid_value(loc_bounds[cuboid3D<>::e_min_z]));
    ASSERT_TRUE(detail::is_invalid_value(loc_bounds[cuboid3D<>::e_max_x]));
    ASSERT_TRUE(detail::is_invalid_value(loc_bounds[cuboid3D<>::e_max_y]));
    ASSERT_TRUE(detail::is_invalid_value(loc_bounds[cuboid3D<>::e_max_z]));

    const auto centroid = u.centroid();
    ASSERT_NEAR(centroid[0], 0.f, tol);
    ASSERT_NEAR(centroid[1], 0.f, tol);
    ASSERT_NEAR(centroid[2], 0.f, tol);
}
