/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2023-2024 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s)
#include "detray/definitions/containers.hpp"
#include "detray/definitions/qualifiers.hpp"
#include "detray/utils/ranges.hpp"

// System include(s)
#include <utility>

namespace detray::n_axis::detail {

template <typename G, typename I>
struct bin_iterator;

/// @returns the local bin indexer for the given @param search_window.
/// (cartesian product of the bin index ranges on the respective axes)
template <std::size_t... I>
DETRAY_HOST_DEVICE inline auto get_bin_indexer(
    const n_axis::multi_bin_range<sizeof...(I)> &search_window,
    std::index_sequence<I...>) {

    return detray::views::cartesian_product{
        detray::views::iota{detray::detail::get<I>(search_window)}...};
}

/// @brief Range adaptor that fetches grid bins according to a search window.
template <typename grid_t>
struct bin_view : public detray::ranges::view_interface<bin_view<grid_t>> {

    /// Cartesian product view over the local bin index sequences
    using bin_indexer_t = decltype(
        get_bin_indexer(std::declval<n_axis::multi_bin_range<grid_t::Dim>>(),
                        std::declval<std::make_index_sequence<grid_t::Dim>>()));

    using iterator_t =
        bin_iterator<grid_t, detray::ranges::iterator_t<bin_indexer_t>>;
    using value_t = typename std::iterator_traits<iterator_t>::value_type;

    /// Default constructor
    constexpr bin_view() = default;

    /// Construct from a @param search_window of local bin index ranges and an
    /// underlying @param grid
    DETRAY_HOST_DEVICE constexpr explicit bin_view(
        const grid_t &grid, n_axis::multi_bin_range<grid_t::Dim> &search_window)
        : m_grid{grid},
          m_bin_indexer{get_bin_indexer(
              search_window,
              std::make_integer_sequence<std::size_t, grid_t::Dim>{})} {}

    /// Copy constructor
    DETRAY_HOST_DEVICE
    constexpr bin_view(const bin_view &other)
        : m_grid{other.m_grid}, m_bin_indexer{other.m_bin_indexer} {}

    /// Default destructor
    ~bin_view() = default;

    /// Copy assignment operator
    DETRAY_HOST_DEVICE
    bin_view &operator=(const bin_view &other) {
        m_grid = other.m_grid;
        m_bin_indexer = other.m_bin_indexer;
        return *this;
    }

    /// @returns start position: first local bin index
    DETRAY_HOST_DEVICE
    constexpr auto begin() const -> iterator_t {
        return {m_grid, detray::ranges::begin(m_bin_indexer)};
    }

    /// @returns sentinel of the range: last local bin index
    DETRAY_HOST_DEVICE
    constexpr auto end() const -> iterator_t {
        return {m_grid, detray::ranges::end(m_bin_indexer)};
    }

    /// @returns number of all bins in the search area
    DETRAY_HOST_DEVICE
    constexpr auto size() const noexcept -> std::size_t {
        return m_bin_indexer.size();
    }

    private:
    /// The underlying grid that holds the bins
    const grid_t &m_grid;
    /// How to index the bins in the search window (produces local indices)
    bin_indexer_t m_bin_indexer;
};

/// @brief Iterate through the bin search area.
template <typename grid_t, typename bin_indexer_t>
struct bin_iterator {

    using difference_type = std::ptrdiff_t;
    using value_type = typename grid_t::bin_type;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = detray::ranges::bidirectional_iterator_tag;

    /// Default constructor required by LegacyIterator trait
    constexpr bin_iterator() = default;

    /// Construct from a bin indexing prescription @param bin_indexer and a
    /// @param grid
    DETRAY_HOST_DEVICE
    constexpr bin_iterator(const grid_t &grid, bin_indexer_t &&bin_indexer)
        : m_grid(grid), m_bin_indexer(std::move(bin_indexer)) {}

    /// @returns true if it points to the same local bin.
    DETRAY_HOST_DEVICE constexpr bool operator==(
        const bin_iterator &rhs) const {
        return (m_bin_indexer == rhs.m_bin_indexer);
    }

    /// @returns false if it points to the same local bin.
    DETRAY_HOST_DEVICE constexpr bool operator!=(
        const bin_iterator &rhs) const {
        return (m_bin_indexer != rhs.m_bin_indexer);
    }

    /// Increment to find next local bin index.
    DETRAY_HOST_DEVICE auto operator++() -> bin_iterator & {
        ++m_bin_indexer;
        return *this;
    }

    /// Decrement to find previous local bin index.
    DETRAY_HOST_DEVICE auto operator--() -> bin_iterator & {
        --m_bin_indexer;
        return *this;
    }

    /// @returns the bin that corresponds to the current local bin index - const
    DETRAY_HOST_DEVICE
    constexpr decltype(auto) operator*() const {
        // Get the correct local bin index
        typename grid_t::loc_bin_index lbin{};
        map_circular(*m_bin_indexer, lbin,
                     std::make_integer_sequence<std::size_t, grid_t::Dim>{});
        // Fetch the bin
        return m_grid.bin(lbin);
    }

    private:
    /// The iota range that is generated for circular axes does not map to their
    /// local bin index range, yet
    template <typename... Idx_t, std::size_t... I>
    DETRAY_HOST_DEVICE constexpr void map_circular(
        std::tuple<Idx_t...> index_tuple, typename grid_t::loc_bin_index &lbin,
        std::index_sequence<I...>) const {
        // Run the mapping for every axis in the grid
        (map_circular(m_grid.template get_axis<I>(), index_tuple, lbin), ...);
    }

    /// Map the local bin index for the phi axis to a periodic range and fill
    /// the local bin @param lbin
    template <typename axis_t, typename... Idx_t>
    DETRAY_HOST_DEVICE constexpr void map_circular(
        const axis_t &ax, std::tuple<Idx_t...> index_tuple,
        typename grid_t::loc_bin_index &lbin) const {

        constexpr auto loc_idx{
            static_cast<std::size_t>(axis_t::bounds_type::label)};

        if constexpr (axis_t::bounds_type::type == n_axis::bounds::e_circular) {
            lbin[loc_idx] = static_cast<dindex>(n_axis::circular<>{}.wrap(
                std::get<loc_idx>(index_tuple), ax.nbins()));
        } else {
            // All other axes start with a range that is already mapped
            lbin[loc_idx] = static_cast<dindex>(std::get<loc_idx>(index_tuple));
        }
    }

    /// Grid
    const grid_t &m_grid;
    /// Bin indexing (cartesian product over local bin index ranges)
    bin_indexer_t m_bin_indexer;
};

}  // namespace detray::n_axis::detail
