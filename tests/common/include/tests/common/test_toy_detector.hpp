/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2021-2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s)
#include "detray/test/types.hpp"
#include "detray/utils/consistency_checker.hpp"

// GTest include(s)
#include <gtest/gtest.h>

// System include(s)
#include <type_traits>

namespace detray {

/// Functor that returns the volume/accel links of a particluar mask
/// instance in the mask container of a detector
struct volume_link_getter {

    template <typename mask_group_t, typename mask_range_t>
    inline auto operator()(const mask_group_t& mask_group,
                           const mask_range_t& mask_range) {
        return mask_group[mask_range].volume_link();
    }
};

/// Call for grid types
template <typename acc_t>
inline void test_finder(const acc_t& finder, const dindex volume_index,
                        const darray<dindex, 2>& range) {

    std::vector<dindex> indices;

    // Check if the correct volume is linked to surface in grid
    // and record the surface indices
    for (const auto& sf : finder.all()) {
        EXPECT_EQ(sf.volume(), volume_index);
        indices.push_back(sf.index());
    }

    // Check if surface indices are consistent with given range of indices
    EXPECT_EQ(finder.size(), range[1] - range[0]);
    for (dindex idx : detray::views::iota(range)) {
        EXPECT_TRUE(std::find(indices.begin(), indices.end(), idx) !=
                    indices.end());
    }
}

template <typename bfield_t>
inline bool test_toy_detector(
    const detector<toy_metadata, bfield_t>& toy_det,
    const typename detector<toy_metadata, bfield_t>::name_map& names) {

    using detector_t = detector<toy_metadata, bfield_t>;
    using geo_obj_ids = typename detector_t::geo_obj_ids;
    using volume_t = typename detector_t::volume_type;
    using nav_link_t = typename detector_t::surface_type::navigation_link;
    using geo_context_t = typename detector_t::geometry_context;
    using mask_ids = typename detector_t::masks::id;
    using mask_link_t = typename detector_t::surface_type::mask_link;
    using material_ids = typename detector_t::materials::id;
    using material_link_t = typename detector_t::surface_type::material_link;
    using accel_ids = typename detector_t::accel::id;
    using accel_link_t = typename volume_t::accel_link_type::index_type;

    EXPECT_EQ(names.at(0u), "toy_detector");

    // Test general consistency
    detail::check_consistency(toy_det);

    geo_context_t ctx{};
    auto& volumes = toy_det.volumes();
    auto& surfaces = toy_det.surfaces();
    auto& accel = toy_det.accelerator_store();
    auto& transforms = toy_det.transform_store();
    auto& masks = toy_det.mask_store();
    auto& materials = toy_det.material_store();

    // Materials
    auto portal_mat = material_slab<scalar>(toy_det_config{}.mapped_material(),
                                            1.5f * unit<scalar>::mm);
    auto beampipe_mat =
        material_slab<scalar>(beryllium_tml<scalar>(), 0.8f * unit<scalar>::mm);
    auto pixel_mat =
        material_slab<scalar>(silicon_tml<scalar>(), 0.15f * unit<scalar>::mm);

    // Link to outer world (leaving detector)
    constexpr auto leaving_world{detail::invalid_value<nav_link_t>()};
    const bool has_grids =
        (accel.template size<accel_ids::e_cylinder2_grid>() != 0u) ||
        (accel.template size<accel_ids::e_disc_grid>() != 0u);
    const bool has_material =
        (materials.template size<material_ids::e_slab>() != 0);
    const bool has_material_maps =
        (materials.template size<material_ids::e_disc2_map>() != 0);

    // Check number of geomtery objects
    EXPECT_EQ(volumes.size(), 20u);
    EXPECT_EQ(toy_det.surfaces().size(), 3244u);
    EXPECT_EQ(transforms.size(ctx), 3264u);
    EXPECT_EQ(masks.template size<mask_ids::e_rectangle2>(), 2492u);
    EXPECT_EQ(masks.template size<mask_ids::e_trapezoid2>(), 648u);
    EXPECT_EQ(masks.template size<mask_ids::e_portal_cylinder2>(), 52u);
    EXPECT_EQ(masks.template size<mask_ids::e_portal_ring2>(), 52u);
    EXPECT_EQ(accel.template size<accel_ids::e_brute_force>(), 20u);
    if (has_grids) {
        EXPECT_EQ(accel.template size<accel_ids::e_cylinder2_grid>(), 4);
        EXPECT_EQ(accel.template size<accel_ids::e_disc_grid>(), 6);
    }
    if (has_material and !has_material_maps) {
        EXPECT_EQ(materials.template size<material_ids::e_slab>(), 3244u);
    } else if (has_material and has_material_maps) {
        EXPECT_EQ(materials.template size<material_ids::e_slab>(), 3141u);
        EXPECT_EQ(materials.template size<material_ids::e_cylinder2_map>(),
                  51u);
        EXPECT_EQ(materials.template size<material_ids::e_disc2_map>(), 52u);
    }

    /// Test the surface ranges in the volume
    auto check_sf_ranges = [](const typename detector_t::volume_type& vol,
                              dindex_range pt_range, dindex_range sf_range,
                              dindex_range psv_range) {
        EXPECT_EQ(vol.template sf_link<surface_id::e_portal>(), pt_range);
        EXPECT_EQ(vol.template sf_link<surface_id::e_sensitive>(), sf_range);
        EXPECT_EQ(vol.template sf_link<surface_id::e_passive>(), psv_range);
    };

    /// Test the links of a volume.
    ///
    /// @param vol_index volume the modules belong to
    /// @param sf_itr iterator into the surface container, start of the modules
    /// @param range index range of the modules in the surface container
    /// @param trf_index index of the transform (trf container) for the module
    /// @param mask_index type and index of module mask in respective mask cont
    /// @param volume_links links to next volume and next surfaces finder
    auto test_volume_links = [&](decltype(volumes.begin())& vol_itr,
                                 const dindex vol_index,
                                 const darray<dindex, 1>& range,
                                 const accel_link_t& /*accel_link*/) {
        EXPECT_EQ(vol_itr->index(), vol_index);
        EXPECT_EQ(vol_itr->template accel_link<geo_obj_ids::e_portal>().id(),
                  accel_ids::e_brute_force);
        EXPECT_EQ(vol_itr->template accel_link<geo_obj_ids::e_portal>().index(),
                  range[0]);
    };

    /// Test the links of portals (into the next volume or invalid if we leave
    /// the detector).
    ///
    /// @param vol_index volume the portals belong to
    /// @param sf_itr iterator into the surface container, start of the portals
    /// @param range index range of the portals in the surface container
    /// @param trf_index index of the transform (trf container) for the portal
    /// @param mask_index type and index of portal mask in respective mask cont
    /// @param volume_links links to next volume contained in the masks
    auto test_portal_links =
        [&](const dindex vol_index, decltype(surfaces.begin())&& sf_itr,
            const darray<dindex, 2>& range, dindex trf_index,
            mask_link_t&& mask_link, material_link_t&& material_index,
            const material_slab<scalar>& mat,
            const dvector<dindex>&& volume_links) {
            for (dindex pti = range[0]; pti < range[1]; ++pti) {
                EXPECT_EQ(sf_itr->volume(), vol_index);
                EXPECT_EQ(sf_itr->id(), surface_id::e_portal);
                EXPECT_EQ(sf_itr->index(), pti);
                // The volume index compensates for the number of volume
                // transforms in the transform store
                EXPECT_EQ(sf_itr->transform(), trf_index + vol_index + 1);
                EXPECT_EQ(sf_itr->mask(), mask_link);
                const auto volume_link =
                    masks.template visit<volume_link_getter>(sf_itr->mask());
                EXPECT_EQ(volume_link, volume_links[pti - range[0]]);
                if (has_material and !has_material_maps) {
                    EXPECT_EQ(sf_itr->material(), material_index);
                    EXPECT_EQ(
                        materials.template get<
                            material_ids::e_slab>()[sf_itr->material().index()],
                        mat);
                }

                ++sf_itr;
                ++trf_index;
                ++mask_link;
                ++material_index;
            }
        };

    /// Test the links of module surface (alway stay in their volume).
    ///
    /// @param vol_index volume the modules belong to
    /// @param sf_itr iterator into the surface container, start of the modules
    /// @param range index range of the modules in the surface container
    /// @param trf_index index of the transform (trf container) for the module
    /// @param mask_index type and index of module mask in respective mask cont
    /// @param volume_links links to next volume contained in the masks
    auto test_module_links =
        [&](const dindex vol_index, decltype(surfaces.begin())&& sf_itr,
            const darray<dindex, 2>& range, dindex trf_index,
            mask_link_t&& mask_index, material_link_t&& material_index,
            const material_slab<scalar>& mat,
            const dvector<dindex>&& volume_links) {
            for (dindex pti = range[0]; pti < range[1]; ++pti) {
                EXPECT_EQ(sf_itr->volume(), vol_index);
                EXPECT_FALSE(sf_itr->id() == surface_id::e_portal)
                    << sf_itr->barcode();
                EXPECT_EQ(sf_itr->index(), pti);
                // The volume index compensates for the number of volume
                // transforms in the transform store
                EXPECT_EQ(sf_itr->transform(), trf_index + vol_index + 1);
                EXPECT_EQ(sf_itr->mask(), mask_index);
                const auto volume_link =
                    masks.template visit<volume_link_getter>(sf_itr->mask());
                EXPECT_EQ(volume_link, volume_links[0]);
                if (has_material and !has_material_maps) {
                    EXPECT_EQ(sf_itr->material(), material_index);
                    EXPECT_EQ(
                        materials.template get<
                            material_ids::e_slab>()[sf_itr->material().index()],
                        mat)
                        << sf_itr->material();
                }

                ++sf_itr;
                ++trf_index;
                ++mask_index;
                ++material_index;
            }
        };

    /// Test the detectors acceleration data structures.
    ///
    /// @param vol_itr iterator over the volume descriptors
    /// @param accel_store the detectors acceleration data structure store
    /// @param pt_range index range of the portals in the surface lookup
    /// @param sf_range index range of the surfaces in the surface lookup
    auto test_accel =
        [has_grids](
            decltype(volumes.begin())& vol_itr,
            const typename detector_t::accelerator_container& accel_store,
            const darray<dindex, 2>& pt_range,
            const darray<dindex, 2>& sf_range = {0u, 0u}) {
            // Link to the acceleration data structures the volume holds
            const auto& link = vol_itr->accel_link();

            // Test the portal search
            const auto& bf_finder =
                accel_store
                    .template get<accel_ids::e_brute_force>()[link[0].index()];

            // This means no grids, all surfaces are in the brute force method
            if (not has_grids) {
                const auto full_range = darray<dindex, 2>{
                    pt_range[0], std::max(pt_range[1], sf_range[1])};
                test_finder(bf_finder, vol_itr->index(), full_range);
            } else {
                test_finder(bf_finder, vol_itr->index(), pt_range);

                // Test the module search if grids were filled
                if (not link[1].is_invalid()) {
                    if (link[1].id() == accel_ids::e_cylinder2_grid) {
                        const auto& cyl_grid = accel_store.template get<
                            accel_ids::e_cylinder2_grid>()[link[1].index()];
                        test_finder(cyl_grid, vol_itr->index(), sf_range);
                    } else {
                        const auto& disc_grid = accel_store.template get<
                            accel_ids::e_disc_grid>()[link[1].index()];
                        test_finder(disc_grid, vol_itr->index(), sf_range);
                    }
                }
            }
        };

    //
    // beampipe
    //

    // Check volume
    auto vol_itr = volumes.begin();
    EXPECT_EQ(names.at(vol_itr->index() + 1), "beampipe_0");
    darray<dindex, 1> index = {0u};
    accel_link_t accel_link{accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {0u, 15u}, {}, {15u, 16u});

    // Test the links in the volumes
    test_volume_links(vol_itr, 0u, index, accel_link);

    // Check links of portals
    // cylinder portals
    darray<dindex, 2> range = {0u, 7u};
    test_portal_links(vol_itr->index(), surfaces.begin(), range, range[0],
                      {mask_ids::e_portal_cylinder2, 0u},
                      {material_ids::e_slab, 0u}, portal_mat,
                      {1u, 2u, 3u, 4u, 5u, 6u, 7u});
    range = {7u, 13u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 7u},
                      {material_ids::e_slab, 7u}, portal_mat,
                      {14u, 15u, 16u, 17u, 18u, 19u});

    // disc portals
    range = {13u, 15u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 0u},
                      {material_ids::e_slab, 13u}, portal_mat,
                      {leaving_world, leaving_world});

    // Check links of beampipe itself
    range = {15u, 16u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_cylinder2, 13u},
                      {material_ids::e_slab, 15u}, beampipe_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {0u, 16u});

    //
    // neg endcap (layer 3)
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "endcap_1");
    range = {16u, 128u};
    index = {1u};
    accel_link = {accel_ids::e_disc_grid, 0u};
    check_sf_ranges(*vol_itr, {16u, 20u}, {20u, 128u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 1u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {16u, 18u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 14u},
                      {material_ids::e_slab, 16u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {18u, 20u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 2u},
                      {material_ids::e_slab, 18u}, portal_mat,
                      {leaving_world, 2u});

    // Check the trapezoid modules
    range = {20u, 128u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_trapezoid2, 0u},
                      {material_ids::e_slab, 20u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {16u, 20u}, range);

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "gap_2");
    range = {128u, 132u};
    index = {2u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {128u, 132u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 2u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {128u, 130u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 16u},
                      {material_ids::e_slab, 128u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {130u, 132u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 4u},
                      {material_ids::e_slab, 130u}, portal_mat, {1u, 3u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {128u, 132u});

    //
    // neg endcap (layer 2)
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "endcap_3");
    range = {132u, 244u};
    index = {3u};
    accel_link = {accel_ids::e_disc_grid, 1u};
    check_sf_ranges(*vol_itr, {132u, 136u}, {136u, 244u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 3u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {132u, 134u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 18u},
                      {material_ids::e_slab, 132u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {134u, 136u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 6u},
                      {material_ids::e_slab, 134u}, portal_mat, {2u, 4u});

    // Check the trapezoid modules
    range = {136u, 244u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_trapezoid2, 108u},
                      {material_ids::e_slab, 136u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {132u, 136u}, range);

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "gap_4");
    range = {244u, 248u};
    index = {4u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {244u, 248u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 4u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {244u, 246u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 20u},
                      {material_ids::e_slab, 244u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {246u, 248u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 8u},
                      {material_ids::e_slab, 246u}, portal_mat, {3u, 5u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {244u, 248u});

    //
    // neg endcap (layer 1)
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "endcap_5");
    range = {248u, 360u};
    index = {5u};
    accel_link = {accel_ids::e_disc_grid, 2u};
    check_sf_ranges(*vol_itr, {248u, 252u}, {252u, 360u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 5u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {248u, 250u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 22u},
                      {material_ids::e_slab, 248u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {250u, 252u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 10u},
                      {material_ids::e_slab, 250u}, portal_mat, {4u, 6u});

    // Check the trapezoid modules
    range = {252u, 360u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_trapezoid2, 216u},
                      {material_ids::e_slab, 252u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {248u, 252u}, range);

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "connector_gap_6");
    range = {360u, 370u};
    index = {6u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {360u, 370u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 6u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {360u, 362u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 24u},
                      {material_ids::e_slab, 360u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {362u, 370u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 12u},
                      {material_ids::e_slab, 362u}, portal_mat,
                      {5u, 7u, 8u, 9u, 10u, 11u, 12u, 13u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {360u, 370u});

    //
    // barrel
    //

    //
    // first layer
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "barrel_7");
    range = {370u, 598u};
    index = {7u};
    accel_link = {accel_ids::e_cylinder2_grid, 0u};
    check_sf_ranges(*vol_itr, {370u, 374u}, {374u, 598u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 7u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {370u, 372u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 26u},
                      {material_ids::e_slab, 370u}, portal_mat, {0u, 8u});

    // disc portals
    range = {372u, 374u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 20u},
                      {material_ids::e_slab, 372u}, portal_mat, {6u, 14u});

    // Check links of modules
    range = {374u, 598u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_rectangle2, 0u},
                      {material_ids::e_slab, 374u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {370u, 374u}, range);

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "gap_8");
    range = {598u, 602u};
    index = {8u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {598u, 602u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 8u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {598u, 600u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 28u},
                      {material_ids::e_slab, 598u}, portal_mat, {7u, 9u});
    // disc portals
    range = {600u, 602u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 22u},
                      {material_ids::e_slab, 600u}, portal_mat, {6u, 14u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {598u, 602u});

    //
    // second layer
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "barrel_9");
    range = {602u, 1054u};
    index = {9u};
    accel_link = {accel_ids::e_cylinder2_grid, 1u};
    check_sf_ranges(*vol_itr, {602u, 606u}, {606u, 1054u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 9u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {602u, 604u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 30u},
                      {material_ids::e_slab, 602u}, portal_mat, {8u, 10u});

    // disc portals
    range = {604u, 606u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 24u},
                      {material_ids::e_slab, 604u}, portal_mat, {6u, 14u});

    // Check links of modules
    range = {606u, 1054u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_rectangle2, 224u},
                      {material_ids::e_slab, 606u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {602u, 606u}, range);

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "gap_10");
    range = {1054u, 1058u};
    index = {10u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {1054u, 1058u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 10u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {1054u, 1056u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 32u},
                      {material_ids::e_slab, 1054u}, portal_mat, {9u, 11u});
    // disc portals
    range = {1056u, 1058u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 26u},
                      {material_ids::e_slab, 1056u}, portal_mat, {6u, 14u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {1054u, 1058u});

    //
    // third layer
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "barrel_11");
    range = {1058u, 1790u};
    index = {11u};
    accel_link = {accel_ids::e_cylinder2_grid, 2u};
    check_sf_ranges(*vol_itr, {1058u, 1062u}, {1062u, 1790u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 11u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {1058u, 1060u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 34u},
                      {material_ids::e_slab, 1058u}, portal_mat, {10u, 12u});

    // disc portals
    range = {1060u, 1062u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 28u},
                      {material_ids::e_slab, 1060u}, portal_mat, {6u, 14u});

    // Check links of modules
    range = {1062u, 1790u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_rectangle2, 672u},
                      {material_ids::e_slab, 1062u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {1058u, 1062u}, range);

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "gap_12");
    range = {1790u, 1794u};
    index = {12u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {1790u, 1794u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 12u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {1790u, 1792u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 36u},
                      {material_ids::e_slab, 1790u}, portal_mat, {11u, 13u});
    // disc portals
    range = {1792u, 1794u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 30u},
                      {material_ids::e_slab, 1792u}, portal_mat, {6u, 14u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {1790u, 1794u});

    //
    // fourth layer
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "barrel_13");
    range = {1794u, 2890u};
    index = {13u};
    accel_link = {accel_ids::e_cylinder2_grid, 3u};
    check_sf_ranges(*vol_itr, {1794u, 1798u}, {1798u, 2890u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 13u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {1794u, 1796u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 38u},
                      {material_ids::e_slab, 1794u}, portal_mat,
                      {12u, leaving_world});

    // disc portals
    range = {1796u, 1798u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 32u},
                      {material_ids::e_slab, 1796u}, portal_mat, {6u, 14u});

    // Check links of modules
    range = {1798u, 2890u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_rectangle2, 1400u},
                      {material_ids::e_slab, 1798u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {1794u, 1798u}, range);

    //
    // positive endcap
    //

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "connector_gap_14");
    range = {2890u, 2900u};
    index = {14u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {2890u, 2900u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 14u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {2890u, 2892u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 40u},
                      {material_ids::e_slab, 2890u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {2892u, 2900u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 34u},
                      {material_ids::e_slab, 2892u}, portal_mat,
                      {15u, 7u, 8u, 9u, 10u, 11u, 12u, 13u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {2890u, 2900u});

    //
    // pos endcap (layer 1)
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "endcap_15");
    range = {2900u, 3012u};
    index = {15u};
    accel_link = {accel_ids::e_disc_grid, 3u};
    check_sf_ranges(*vol_itr, {2900u, 2904u}, {2904u, 3012u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 15u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {2900u, 2902u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 42u},
                      {material_ids::e_slab, 2900u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {2902u, 2904u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 42u},
                      {material_ids::e_slab, 2902u}, portal_mat, {14u, 16u});

    // Check the trapezoid modules
    range = {2904u, 3012u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_trapezoid2, 324u},
                      {material_ids::e_slab, 2904u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {2900u, 2904u}, range);

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "gap_16");
    range = {3012u, 3016u};
    index = {16u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {3012u, 3016u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 16u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {3012u, 3014u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 44u},
                      {material_ids::e_slab, 3012u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {3014u, 3016u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 44u},
                      {material_ids::e_slab, 3014u}, portal_mat, {15u, 17u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {3012u, 3016u});

    //
    // pos endcap (layer 2)
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "endcap_17");
    range = {3016u, 3128u};
    index = {17u};
    accel_link = {accel_ids::e_disc_grid, 4u};
    check_sf_ranges(*vol_itr, {3016u, 3020u}, {3020u, 3128u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 17u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {3016u, 3018u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 46u},
                      {material_ids::e_slab, 3016u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {3018u, 3020u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 46u},
                      {material_ids::e_slab, 3018u}, portal_mat, {16u, 18u});

    // Check the trapezoid modules
    range = {3020u, 3128u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_trapezoid2, 432u},
                      {material_ids::e_slab, 3020u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {3016u, 3020u}, range);

    //
    // gap
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "gap_18");
    range = {3128u, 3132u};
    index = {18u};
    accel_link = {accel_ids::e_brute_force, 0u};
    check_sf_ranges(*vol_itr, {3128u, 3132u}, {}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 18u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {3128u, 3130u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 48u},
                      {material_ids::e_slab, 3128u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {3130u, 3132u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 48u},
                      {material_ids::e_slab, 3130u}, portal_mat, {17u, 19u});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {3128u, 3132u});

    //
    // pos endcap (layer 3)
    //

    // Check volume
    ++vol_itr;
    EXPECT_EQ(names.at(vol_itr->index() + 1), "endcap_19");
    range = {3132u, 3244u};
    index = {19u};
    accel_link = {accel_ids::e_disc_grid, 5u};
    check_sf_ranges(*vol_itr, {3132u, 3136u}, {3136u, 3244u}, {});

    // Test the links in the volumes
    test_volume_links(vol_itr, 19u, index, accel_link);

    // Check links of portals
    // cylinder portals
    range = {3132u, 3134u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_cylinder2, 50u},
                      {material_ids::e_slab, 3132u}, portal_mat,
                      {0u, leaving_world});
    // disc portals
    range = {3134u, 3136u};
    test_portal_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_portal_ring2, 50u},
                      {material_ids::e_slab, 3134u}, portal_mat,
                      {18u, leaving_world});

    // Check the trapezoid modules
    range = {3136u, 3244u};
    test_module_links(vol_itr->index(),
                      surfaces.begin() + static_cast<std::ptrdiff_t>(range[0]),
                      range, range[0], {mask_ids::e_trapezoid2, 540u},
                      {material_ids::e_slab, 3136u}, pixel_mat,
                      {vol_itr->index()});

    // Check link of surfaces in surface finder
    test_accel(vol_itr, accel, {3132u, 3136u}, range);

    return true;
}

}  // namespace detray
