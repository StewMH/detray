/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2020-2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Project include(s)
#include "detray/geometry/detail/surface_descriptor.hpp"
#include "detray/intersection/detail/trajectories.hpp"
#include "detray/intersection/intersection.hpp"
#include "detray/intersection/plane_intersector.hpp"
#include "detray/masks/masks.hpp"
#include "detray/masks/unmasked.hpp"
#include "detray/test/types.hpp"

// GTest include(s)
#include <gtest/gtest.h>

// System include(s)
#include <cmath>
#include <limits>

using namespace detray;

// Three-dimensional definitions
using vector3 = test::vector3;
using point3 = test::point3;
using transform3 = test::transform3;
using intersection_t = intersection2D<surface_descriptor<>, transform3>;

constexpr scalar tol{std::numeric_limits<scalar>::epsilon()};

// This defines the local frame test suite
GTEST_TEST(detray_intersection, translated_plane_ray) {
    // Create a shifted plane
    const transform3 shifted(vector3{3.f, 2.f, 10.f});

    // Test ray
    const point3 pos{2.f, 1.f, 0.f};
    const vector3 mom{0.f, 0.f, 1.f};
    const detail::ray<transform3> r(pos, 0.f, mom, 0.f);

    // The same test but bound to local frame
    plane_intersector<intersection_t> pi;
    mask<unmasked> unmasked_bound{};
    const auto hit_bound =
        pi(r, surface_descriptor<>{}, unmasked_bound, shifted);

    ASSERT_TRUE(hit_bound.status == intersection::status::e_inside);
    // Global intersection information - unchanged
    const auto global0 =
        unmasked_bound.to_global_frame(shifted, hit_bound.local);
    ASSERT_NEAR(global0[0], 2.f, tol);
    ASSERT_NEAR(global0[1], 1.f, tol);
    ASSERT_NEAR(global0[2], 10.f, tol);
    // Local intersection information
    ASSERT_NEAR(hit_bound.local[0], -1.f, tol);
    ASSERT_NEAR(hit_bound.local[1], -1.f, tol);
    // Incidence angle
    ASSERT_NEAR(hit_bound.cos_incidence_angle, 1.f, tol);

    // The same test but bound to local frame & masked - inside
    mask<rectangle2D<>> rect_for_inside{0u, 3.f, 3.f};
    const auto hit_bound_inside =
        pi(r, surface_descriptor<>{}, rect_for_inside, shifted);
    ASSERT_TRUE(hit_bound_inside.status == intersection::status::e_inside);
    // Global intersection information - unchanged
    const auto global1 =
        rect_for_inside.to_global_frame(shifted, hit_bound_inside.local);
    ASSERT_NEAR(global1[0], 2.f, tol);
    ASSERT_NEAR(global1[1], 1.f, tol);
    ASSERT_NEAR(global1[2], 10.f, tol);
    // Local intersection infoimation - unchanged
    ASSERT_NEAR(hit_bound_inside.local[0], -1.f, tol);
    ASSERT_NEAR(hit_bound_inside.local[1], -1.f, tol);

    // The same test but bound to local frame & masked - outside
    mask<rectangle2D<>> rect_for_outside{0u, 0.5f, 3.5f};
    const auto hit_bound_outside =
        pi(r, surface_descriptor<>{}, rect_for_outside, shifted);
    ASSERT_TRUE(hit_bound_outside.status == intersection::status::e_outside);
    // Global intersection information - not written out anymore
    const auto global2 =
        rect_for_outside.to_global_frame(shifted, hit_bound_outside.local);
    ASSERT_NEAR(global2[0], 2.f, tol);
    ASSERT_NEAR(global2[1], 1.f, tol);
    ASSERT_NEAR(global2[2], 10.f, tol);
    // Local intersection infoimation - unchanged
    ASSERT_NEAR(hit_bound_outside.local[0], -1.f, tol);
    ASSERT_NEAR(hit_bound_outside.local[1], -1.f, tol);
}

// This defines the local frame test suite
GTEST_TEST(detray_intersection, plane_incidence_angle) {
    // tf3 with rotated axis
    const vector3 x{1.f, 0.f, -1.f};
    const vector3 z{1.f, 0.f, 1.f};
    const vector3 t{0.f, 0.f, 0.f};

    const transform3 rotated{t, vector::normalize(z), vector::normalize(x)};

    plane_intersector<intersection_t> pi;

    // Test ray
    const point3 pos{-1.f, 0.f, 0.f};
    const vector3 mom{1.f, 0.f, 0.f};
    const detail::ray<transform3> r(pos, 0.f, mom, 0.f);

    // The same test but bound to local frame & masked - inside
    mask<rectangle2D<>> rect{0u, 3.f, 3.f};

    const auto is = pi(r, surface_descriptor<>{}, rect, rotated);

    ASSERT_NEAR(is.cos_incidence_angle, std::cos(constant<scalar>::pi_4), tol);
}
