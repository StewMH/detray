/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Project include(s)
#include "detray/definitions/units.hpp"
#include "detray/masks/masks.hpp"
#include "detray/masks/unbounded.hpp"
#include "detray/test/types.hpp"

// GTest include(s)
#include <gtest/gtest.h>

// System include(s)
#include <cassert>
#include <type_traits>

using namespace detray;
using point3_t = test::point3;
using transform3_t = test::transform3;

constexpr scalar tol{1e-7f};

/// This tests the basic functionality of an unbounded rectangle shape
GTEST_TEST(detray_masks, unbounded) {

    using shape_t = rectangle2D<>;
    using unbounded_t = unbounded<shape_t>;

    constexpr scalar h{20.f * unit<scalar>::mm};

    mask<unbounded_t> u{0u, h, h};

    // Test local typedefs
    static_assert(std::is_same_v<unbounded_t::shape, shape_t>,
                  "incorrect shape");
    static_assert(std::is_same_v<unbounded_t::boundaries, shape_t::boundaries>,
                  "incorrect boundaries");
    static_assert(
        std::is_same_v<unbounded_t::template local_frame_type<transform3_t>,
                       shape_t::template local_frame_type<transform3_t>>,
        "incorrect local frame");
    static_assert(
        std::is_same_v<unbounded_t::template intersector_type<transform3_t>,
                       shape_t::template intersector_type<transform3_t>>,
        "incorrect intersector");

    // Test static members
    EXPECT_TRUE(unbounded_t::name == "unbounded rectangle2D");

    // Test boundary check
    typename mask<unbounded_t>::point3_t p2 = {0.5f, -9.f, 0.f};
    ASSERT_TRUE(u.is_inside(p2, 0.f) == intersection::status::e_inside);

    // Check bounding box
    constexpr scalar envelope{0.01f};
    const auto loc_bounds = u.local_min_bounds(envelope);
    ASSERT_NEAR(loc_bounds[cuboid3D<>::e_min_x], -(h + envelope), tol);
    ASSERT_NEAR(loc_bounds[cuboid3D<>::e_min_y], -(h + envelope), tol);
    ASSERT_NEAR(loc_bounds[cuboid3D<>::e_min_z], -envelope, tol);
    ASSERT_NEAR(loc_bounds[cuboid3D<>::e_max_x], (h + envelope), tol);
    ASSERT_NEAR(loc_bounds[cuboid3D<>::e_max_y], (h + envelope), tol);
    ASSERT_NEAR(loc_bounds[cuboid3D<>::e_max_z], envelope, tol);

    const auto centroid = u.centroid();
    ASSERT_NEAR(centroid[0], 0.f, tol);
    ASSERT_NEAR(centroid[1], 0.f, tol);
    ASSERT_NEAR(centroid[2], 0.f, tol);
}
