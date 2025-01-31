/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2022-2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#include <gtest/gtest.h>

#include "detray/definitions/units.hpp"
#include "detray/intersection/detail/trajectories.hpp"
#include "detray/test/types.hpp"
#include "detray/tracks/tracks.hpp"

// System include(s)
#include <cmath>

using namespace detray;

constexpr const scalar tol{1e-5f};

// This tests the base functionality of the Helix Gun
GTEST_TEST(detray_intersection, helix_trajectory) {

    using vector3 = test::vector3;
    using point3 = test::point3;
    using transform3_type = test::transform3;

    const point3 pos{0.f, 0.f, 0.f};
    const scalar time{0.f};
    const vector3 mom{1.f, 0.f, 1.f * unit<scalar>::GeV};
    const scalar q{-1.f * unit<scalar>::e};

    // vertex
    free_track_parameters<transform3_type> vertex(pos, time, mom, q);

    // magnetic field
    const vector3 B{0.f, 0.f, 1.f * unit<scalar>::T};

    const scalar p_mag{getter::norm(mom)};
    const scalar B_mag{getter::norm(B)};
    const scalar pz_along{vector::dot(mom, vector::normalize(B))};
    const scalar pt{std::sqrt(p_mag * p_mag - pz_along * pz_along)};

    // helix trajectory
    detail::helix helix_traj(vertex, &B);
    EXPECT_NEAR(helix_traj.time(), 0.f, tol);
    EXPECT_NEAR(helix_traj.qop(), -constant<scalar>::inv_sqrt2, tol);

    // radius of helix
    scalar R{helix_traj.radius()};
    EXPECT_NEAR(R, pt / B_mag, tol);

    // Path length for one loop
    scalar S = 2.f * p_mag / B_mag * constant<scalar>::pi;

    // After half turn
    point3 half_loop_pos = helix_traj(S / 2.f);
    EXPECT_NEAR(half_loop_pos[0], 0.f, R * tol);
    EXPECT_NEAR(half_loop_pos[1], 2.f * R, R * tol);
    EXPECT_NEAR(half_loop_pos[2], pz_along / B_mag * constant<scalar>::pi,
                R * tol);

    point3 half_loop_dir = helix_traj.dir(S / 2.f);
    EXPECT_NEAR(half_loop_dir[0], -vertex.dir()[0], R * tol);
    EXPECT_NEAR(half_loop_dir[1], -vertex.dir()[1], R * tol);
    EXPECT_NEAR(half_loop_dir[2], vertex.dir()[2], R * tol);

    // After half turn in the opposite direction
    half_loop_pos = helix_traj(-S / 2.f);
    EXPECT_NEAR(half_loop_pos[0], 0.f, R * tol);
    EXPECT_NEAR(half_loop_pos[1], 2.f * R, R * tol);
    EXPECT_NEAR(half_loop_pos[2], -pz_along / B_mag * constant<scalar>::pi,
                R * tol);

    half_loop_dir = helix_traj.dir(-S / 2.f);
    EXPECT_NEAR(half_loop_dir[0], -vertex.dir()[0], R * tol);
    EXPECT_NEAR(half_loop_dir[1], -vertex.dir()[1], R * tol);
    EXPECT_NEAR(half_loop_dir[2], vertex.dir()[2], R * tol);

    // After one full turn
    point3 one_loop_pos = helix_traj(S);
    EXPECT_NEAR(one_loop_pos[0], 0.f, R * tol);
    EXPECT_NEAR(one_loop_pos[1], 0.f, R * tol);
    EXPECT_NEAR(one_loop_pos[2], 2.f * pz_along / B_mag * constant<scalar>::pi,
                R * tol);

    point3 one_loop_dir = helix_traj.dir(S);
    EXPECT_NEAR(one_loop_dir[0], vertex.dir()[0], R * tol);
    EXPECT_NEAR(one_loop_dir[1], vertex.dir()[1], R * tol);
    EXPECT_NEAR(one_loop_dir[2], vertex.dir()[2], R * tol);

    // After one full turn in the opposite direction
    one_loop_pos = helix_traj(-S);
    EXPECT_NEAR(one_loop_pos[0], 0.f, R * tol);
    EXPECT_NEAR(one_loop_pos[1], 0.f, R * tol);
    EXPECT_NEAR(one_loop_pos[2], -2.f * pz_along / B_mag * constant<scalar>::pi,
                R * tol);

    one_loop_dir = helix_traj.dir(-S);
    EXPECT_NEAR(one_loop_dir[0], vertex.dir()[0], R * tol);
    EXPECT_NEAR(one_loop_dir[1], vertex.dir()[1], R * tol);
    EXPECT_NEAR(one_loop_dir[2], vertex.dir()[2], R * tol);

    /*********************************
     * Same test with oppsite charge
     *********************************/

    free_track_parameters<transform3_type> vertex2(pos, time, mom, -q);

    // helix trajectory
    detail::helix helix_traj2(vertex2, &B);

    EXPECT_NEAR(R, helix_traj2.radius(), tol);

    // After half turn
    half_loop_pos = helix_traj2(S / 2.f);
    EXPECT_NEAR(half_loop_pos[0], 0.f, R * tol);
    EXPECT_NEAR(half_loop_pos[1], -2.f * R, R * tol);
    EXPECT_NEAR(half_loop_pos[2], pz_along / B_mag * constant<scalar>::pi,
                R * tol);

    half_loop_dir = helix_traj2.dir(S / 2.f);
    EXPECT_NEAR(half_loop_dir[0], -vertex2.dir()[0], R * tol);
    EXPECT_NEAR(half_loop_dir[1], -vertex2.dir()[1], R * tol);
    EXPECT_NEAR(half_loop_dir[2], vertex2.dir()[2], R * tol);

    // After half turn in the opposite direction
    half_loop_pos = helix_traj2(-S / 2.f);
    EXPECT_NEAR(half_loop_pos[0], 0.f, R * tol);
    EXPECT_NEAR(half_loop_pos[1], -2.f * R, R * tol);
    EXPECT_NEAR(half_loop_pos[2], -pz_along / B_mag * constant<scalar>::pi,
                R * tol);

    half_loop_dir = helix_traj.dir(-S / 2.f);
    EXPECT_NEAR(half_loop_dir[0], -vertex.dir()[0], R * tol);
    EXPECT_NEAR(half_loop_dir[1], -vertex.dir()[1], R * tol);
    EXPECT_NEAR(half_loop_dir[2], vertex.dir()[2], R * tol);

    // After one full turn
    one_loop_pos = helix_traj2(S);
    EXPECT_NEAR(one_loop_pos[0], 0.f, R * tol);
    EXPECT_NEAR(one_loop_pos[1], 0.f, R * tol);
    EXPECT_NEAR(one_loop_pos[2], 2.f * pz_along / B_mag * constant<scalar>::pi,
                R * tol);

    one_loop_dir = helix_traj2.dir(S);
    EXPECT_NEAR(one_loop_dir[0], vertex2.dir()[0], R * tol);
    EXPECT_NEAR(one_loop_dir[1], vertex2.dir()[1], R * tol);
    EXPECT_NEAR(one_loop_dir[2], vertex2.dir()[2], R * tol);

    // After one full turn in the opposite direction
    one_loop_pos = helix_traj2(-S);
    EXPECT_NEAR(one_loop_pos[0], 0.f, R * tol);
    EXPECT_NEAR(one_loop_pos[1], 0.f, R * tol);
    EXPECT_NEAR(one_loop_pos[2], -2.f * pz_along / B_mag * constant<scalar>::pi,
                R * tol);

    one_loop_dir = helix_traj2.dir(-S);
    EXPECT_NEAR(one_loop_dir[0], vertex2.dir()[0], R * tol);
    EXPECT_NEAR(one_loop_dir[1], vertex2.dir()[1], R * tol);
    EXPECT_NEAR(one_loop_dir[2], vertex2.dir()[2], R * tol);
}

GTEST_TEST(detray_intersection, helix_trajectory_small_pT) {

    using vector3 = test::vector3;
    using point3 = test::point3;
    using transform3_type = test::transform3;

    const point3 pos{0.f, 0.f, 0.f};
    const scalar time{0.f};
    const vector3 mom{0.f, tol, 1.f * unit<scalar>::GeV};
    const scalar q{-1. * unit<scalar>::e};

    // vertex
    free_track_parameters<transform3_type> vertex(pos, time, mom, q);

    // magnetic field
    const vector3 B{0.f, 0.f, 1.f * unit<scalar>::T};

    // helix trajectory
    detail::helix helix_traj(vertex, &B);
    EXPECT_NEAR(helix_traj.time(), 0.f, tol);
    EXPECT_NEAR(helix_traj.qop(), -1.f, tol);

    // After 10 mm
    const scalar path_length{10.f * unit<scalar>::mm};
    const point3 helix_pos = helix_traj(path_length);
    const point3 true_pos = pos + path_length * vector::normalize(mom);

    EXPECT_NEAR(true_pos[0], helix_pos[0], tol);
    EXPECT_NEAR(true_pos[1], helix_pos[1], tol);
    EXPECT_NEAR(true_pos[2], helix_pos[2], tol);
}
