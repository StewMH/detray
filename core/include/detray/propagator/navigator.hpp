/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2021-2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s)
#include "detray/core/detector.hpp"
#include "detray/definitions/containers.hpp"
#include "detray/definitions/detail/algorithms.hpp"
#include "detray/definitions/indexing.hpp"
#include "detray/definitions/qualifiers.hpp"
#include "detray/definitions/units.hpp"
#include "detray/geometry/barcode.hpp"
#include "detray/intersection/detail/trajectories.hpp"
#include "detray/intersection/intersection.hpp"
#include "detray/intersection/intersection_kernel.hpp"
#include "detray/utils/ranges.hpp"

// vecmem include(s)
#include <vecmem/containers/data/jagged_vector_buffer.hpp>
#include <vecmem/memory/memory_resource.hpp>

namespace detray {

namespace navigation {

/// @enum NavigationDirection
/// The navigation direction is always with
/// respect to a given momentum or direction
enum class direction : int { e_backward = -1, e_forward = 1 };

/// Navigation status flags
enum class status {
    e_abort = -3,          ///< error ocurred, propagation will be aborted
    e_on_target = -2,      ///< navigation exited successfully
    e_unknown = -1,        ///< unknown state/not initialized
    e_towards_object = 0,  ///< move towards next object
    e_on_module = 1,       ///< reached module surface
    e_on_portal = 2,       ///< reached portal surface
};

/// Navigation trust levels determine how the candidates chache is updated
enum class trust_level {
    e_no_trust = 0,  ///< re-initialize the volume (i.e. run local navigation)
    e_fair = 1,      ///< update the distance & order of the candidates
    e_high = 3,  ///< update the distance to the next candidate (current target)
    e_full = 4   ///< don't update anything
};

/// Navigation configuration
struct config {
    /// Tolerance on the masks 'is_inside' check
    scalar mask_tolerance{15.f * unit<scalar>::um};
    /// Maximal absolute path distance for a track to be considered 'on surface'
    scalar on_surface_tolerance{1.f * unit<scalar>::um};
    /// How far behind the track position to look for candidates
    scalar overstep_tolerance{-100.f * unit<scalar>::um};
    /// Search window size for grid based acceleration structures
    std::array<dindex, 2> search_window = {0u, 0u};
};

/// A void inpector that does nothing.
///
/// Inspectors can be plugged in to understand the current navigation state.
struct void_inspector {
    template <typename state_t>
    DETRAY_HOST_DEVICE void operator()(const state_t & /*ignored*/,
                                       const char * /*ignored*/) {}
};

}  // namespace navigation

/// @brief The geometry navigation class.
///
/// The navigator is initialized around a detector object, but is itself
/// agnostic to the detectors's object/primitive types.
/// Within a detector volume, the navigatior will perform a local navigation
/// based on the geometry accelerator structure that is provided by the volume.
/// Once the local navigation is resolved, it moves to the next volume by a
/// portal.
/// To this end, it requires a link to the [next] navigation volume in every
/// candidate that is computed by intersection from the detector objects:
/// A module surface must link back to its mothervolume, while a portal surface
/// links to the next volume in the direction of the track.
///
/// This navigator applies a trust level based update of its candidate
/// (intersection) cache, which is kept in the naviagtor's state. The trust
/// level, and with it the appropriate update policy, must be set by an actor,
/// otherwise no update will be performed.
///
/// The navigation state is set up by an init() call and then follows a
/// sequence of
/// - step()       (stepper)
/// - update()     (navigator)
/// - run_actors() (actor chain)
/// calls, which are handeled by the propagator class.
///
/// The navigation heartbeat indicates, that the navigation is still running
/// and in a valid state.
///
/// @tparam detector_t the detector to navigate
/// @tparam inspector_t is a validation inspector that can record information
///         about the navigation state at different points of the nav. flow.
template <
    typename detector_t, typename inspector_t = navigation::void_inspector,
    typename intersection_t = intersection2D<typename detector_t::surface_type,
                                             typename detector_t::transform3>>
class navigator {

    public:
    using inspector_type = inspector_t;
    using detector_type = detector_t;
    using scalar_type = typename detector_t::scalar_type;
    using volume_type = typename detector_t::volume_type;
    template <typename T>
    using vector_type = typename detector_t::template vector_type<T>;
    using intersection_type = intersection_t;
    using nav_link_type = typename detector_t::surface_type::navigation_link;

    private:
    /// A functor that fills the navigation candidates vector by intersecting
    /// the surfaces in the volume neighborhood
    struct candidate_search {

        /// Test the volume links
        template <typename track_t>
        DETRAY_HOST_DEVICE void operator()(
            const typename detector_type::surface_type &sf_descr,
            const detector_type &det, const track_t &track,
            vector_type<intersection_type> &candidates,
            const scalar_type mask_tol, const scalar_type overstep_tol) const {

            const auto sf = surface{det, sf_descr};

            sf.template visit_mask<intersection_initialize>(
                candidates, detail::ray(track), sf_descr, det.transform_store(),
                sf.is_portal() ? 0.f : mask_tol, overstep_tol);
        }
    };

    public:
    /// @brief A navigation state object used to cache the information of the
    /// current navigation stream.
    ///
    /// The state is passed between navigation calls and is accessible to the
    /// actors in the propagation, for which it defines the public interface
    /// towards the navigation. The navigator is responsible for updating the
    /// elements  in the state's cache with every navigation call, establishing
    /// 'full trust' again.
    class state : public detray::ranges::view_interface<state> {
        friend class navigator;
        // Allow the filling/updating of candidates
        friend struct intersection_initialize;
        friend struct intersection_update;

        using candidate_itr_t =
            typename vector_type<intersection_type>::iterator;
        using const_candidate_itr_t =
            typename vector_type<intersection_type>::const_iterator;

        public:
        using detector_type = navigator::detector_type;

        /// Default constructor
        state() = default;

        state(const detector_type &det) : m_detector(&det) {}

        /// Constructor with memory resource
        DETRAY_HOST
        state(const detector_type &det, vecmem::memory_resource &resource)
            : m_detector(&det), m_candidates(&resource) {}

        /// Constructor from candidates vector_view
        DETRAY_HOST_DEVICE state(const detector_type &det,
                                 vector_type<intersection_type> candidates)
            : m_detector(&det), m_candidates(candidates) {}

        /// @return start position of valid candidate range.
        DETRAY_HOST_DEVICE
        constexpr auto begin() -> candidate_itr_t { return m_next; }

        /// @return start position of the valid candidate range - const
        DETRAY_HOST_DEVICE
        constexpr auto begin() const -> const_candidate_itr_t { return m_next; }

        /// @return sentinel of the valid candidate range.
        DETRAY_HOST_DEVICE
        constexpr auto end() -> candidate_itr_t { return m_last; }

        /// @return sentinel of the valid candidate range.
        DETRAY_HOST_DEVICE
        constexpr auto end() const -> const_candidate_itr_t { return m_last; }

        /// @returns a pointer of detector
        DETRAY_HOST_DEVICE
        auto detector() const { return m_detector; }

        /// Scalar representation of the navigation state,
        /// @returns distance to next
        DETRAY_HOST_DEVICE
        scalar_type operator()() const { return m_next->path; }

        /// @returns currently cached candidates - const
        DETRAY_HOST_DEVICE
        inline auto candidates() const
            -> const vector_type<intersection_type> & {
            return m_candidates;
        }

        /// @returns numer of currently cached (reachable) candidates - const
        DETRAY_HOST_DEVICE
        inline auto n_candidates() const ->
            typename std::iterator_traits<candidate_itr_t>::difference_type {
            return std::distance(m_next, m_last);
        }

        /// @returns current/previous object that was reached
        DETRAY_HOST_DEVICE
        inline auto current() const -> const_candidate_itr_t {
            return m_next - 1;
        }

        /// @returns next object that we want to reach (current target) - const
        DETRAY_HOST_DEVICE
        inline auto next() const -> const const_candidate_itr_t & {
            return m_next;
        }

        /// @returns last valid candidate (by position in the cache) - const
        DETRAY_HOST_DEVICE
        inline auto last() const -> const const_candidate_itr_t & {
            return m_last;
        }

        /// @returns the navigation inspector
        DETRAY_HOST
        inline auto &inspector() { return m_inspector; }

        /// @returns current volume (index) - const
        DETRAY_HOST_DEVICE
        inline auto volume() const -> nav_link_type { return m_volume_index; }

        /// Set start/new volume
        DETRAY_HOST_DEVICE
        inline void set_volume(dindex v) {
            m_volume_index = static_cast<nav_link_type>(v);
        }

        /// @returns barcode of the detector surface the navigator is on
        /// (invalid when not on surface) - const
        DETRAY_HOST_DEVICE
        inline auto barcode() const -> geometry::barcode {
            return current()->sf_desc.barcode();
        }

        /// @returns the next surface the navigator intends to reach
        DETRAY_HOST_DEVICE
        inline auto next_surface() const {
            return surface<detector_type>{*m_detector,
                                          m_next->sf_desc.barcode()};
        }

        /// @returns current detector surface the navigator is on
        /// (cannot be used when not on surface) - const
        DETRAY_HOST_DEVICE
        inline auto get_surface() const {
            assert(is_on_module() or is_on_portal());
            return surface<detector_type>{*m_detector, barcode()};
        }

        /// @returns current navigation status - const
        DETRAY_HOST_DEVICE
        inline auto status() const -> navigation::status { return m_status; }

        /// @returns current navigation direction - const
        DETRAY_HOST_DEVICE
        inline auto direction() const -> navigation::direction {
            return m_direction;
        }

        /// Set direction
        DETRAY_HOST_DEVICE
        inline void set_direction(const navigation::direction dir) {
            m_direction = dir;
        }

        /// @returns navigation trust level - const
        DETRAY_HOST_DEVICE
        inline auto trust_level() const -> navigation::trust_level {
            return m_trust_level;
        }

        /// Update navigation trust level to no trust
        DETRAY_HOST_DEVICE
        inline void set_no_trust() {
            m_trust_level = navigation::trust_level::e_no_trust;
        }

        /// Update navigation trust level to full trust
        DETRAY_HOST_DEVICE
        inline void set_full_trust() {
            m_trust_level = m_trust_level <= navigation::trust_level::e_full
                                ? m_trust_level
                                : navigation::trust_level::e_full;
        }

        /// Update navigation trust level to high trust
        DETRAY_HOST_DEVICE
        inline void set_high_trust() {
            m_trust_level = m_trust_level <= navigation::trust_level::e_high
                                ? m_trust_level
                                : navigation::trust_level::e_high;
        }

        /// Update navigation trust level to fair trust
        DETRAY_HOST_DEVICE
        inline void set_fair_trust() {
            m_trust_level = m_trust_level <= navigation::trust_level::e_fair
                                ? m_trust_level
                                : navigation::trust_level::e_fair;
        }

        /// Helper method to check the track has reached a module surface
        DETRAY_HOST_DEVICE
        inline auto is_on_module() const -> bool {
            return m_status == navigation::status::e_on_module;
        }

        /// Helper method to check the track has reached a sensitive surface
        DETRAY_HOST_DEVICE
        inline auto is_on_sensitive() const -> bool {
            return (m_status == navigation::status::e_on_module) &&
                   (barcode().id() == surface_id::e_sensitive);
        }

        /// Helper method to check the track has reached a portal surface
        DETRAY_HOST_DEVICE
        inline auto is_on_portal() const -> bool {
            return m_status == navigation::status::e_on_portal;
        }

        /// Helper method to check the track has encountered material
        DETRAY_HOST_DEVICE
        inline auto encountered_material() const -> bool {
            return (is_on_module() or is_on_portal()) and
                   (current()->sf_desc.material().id() !=
                    detector_t::materials::id::e_none);
        }

        /// Helper method to check if a kernel is exhausted - const
        DETRAY_HOST_DEVICE
        inline auto is_exhausted() const -> bool {
            return std::distance(m_next, m_last) <= 0;
        }

        /// @returns flag that indicates whether navigation was successful
        DETRAY_HOST_DEVICE
        inline auto is_complete() const -> bool {
            // Normal exit for this navigation?
            return m_status == navigation::status::e_on_target and !m_heartbeat;
        }

        /// Navigation state that cannot be recovered from. Leave the other
        /// data for inspection.
        ///
        /// @return navigation heartbeat (dead)
        DETRAY_HOST_DEVICE
        inline auto abort() -> bool {
            m_status = navigation::status::e_abort;
            m_heartbeat = false;
            // Don't do anything if aborted
            m_trust_level = navigation::trust_level::e_full;
            run_inspector({}, "Aborted: ");
            return m_heartbeat;
        }

        /// Navigation reaches target or leaves detector world. Stop
        /// navigation.
        ///
        /// @return navigation heartbeat (dead)
        DETRAY_HOST_DEVICE
        inline auto exit() -> bool {
            m_status = navigation::status::e_on_target;
            m_heartbeat = false;
            m_trust_level = navigation::trust_level::e_full;
            run_inspector({}, "Exited: ");
            this->clear();
            return m_heartbeat;
        }

        private:
        /// Helper method to check if a candidate lies on a surface - const
        DETRAY_HOST_DEVICE inline auto is_on_object(
            const intersection_type &candidate,
            const navigation::config &cfg) const -> bool {
            return (math_ns::abs(candidate.path) < cfg.on_surface_tolerance);
        }

        /// @returns next object that we want to reach (current target)
        DETRAY_HOST_DEVICE
        inline auto next() -> candidate_itr_t & { return m_next; }

        /// Updates the iterator position of the last valid candidate
        DETRAY_HOST_DEVICE
        inline void set_next(candidate_itr_t &&new_next) {
            m_next = std::move(new_next);
        }

        /// Updates the iterator position of the last valid candidate
        DETRAY_HOST_DEVICE
        inline void set_last(candidate_itr_t &&new_last) {
            m_last = std::move(new_last);
        }

        /// @returns currently cached candidates
        DETRAY_HOST_DEVICE
        inline auto candidates() -> vector_type<intersection_type> & {
            return m_candidates;
        }

        /// Clear the state
        DETRAY_HOST_DEVICE
        inline void clear() {
            m_candidates.clear();
            m_next = m_candidates.end();
            m_last = m_candidates.end();
        }

        /// Call the navigation inspector
        DETRAY_HOST_DEVICE
        inline void run_inspector(
            [[maybe_unused]] const navigation::config &cfg,
            [[maybe_unused]] const char *message) {
            if constexpr (not std::is_same_v<inspector_t,
                                             navigation::void_inspector>) {
                m_inspector(*this, cfg, message);
            }
        }

        /// Heartbeat of this navigation flow signals navigation is alive
        bool m_heartbeat = false;

        /// Detector pointer
        const detector_type *const m_detector;

        /// Our cache of candidates (intersections with any kind of surface)
        vector_type<intersection_type> m_candidates = {};

        /// The next best candidate
        candidate_itr_t m_next = m_candidates.end();

        /// The last reachable candidate
        candidate_itr_t m_last = m_candidates.end();

        /// The inspector type of this navigation engine
        inspector_type m_inspector;

        /// The navigation status
        navigation::status m_status = navigation::status::e_unknown;

        /// The navigation direction
        navigation::direction m_direction = navigation::direction::e_forward;

        /// The navigation trust level determines how this states cache is to
        /// be updated in the current navigation call
        navigation::trust_level m_trust_level =
            navigation::trust_level::e_no_trust;

        /// Index in the detector volume container of current navigation volume
        nav_link_type m_volume_index{0u};
    };

    /// @brief Helper method to initialize a volume.
    ///
    /// Calls the volumes accelerator structure for local navigation, then tests
    /// the surfaces for intersection and sorts the reachable candidates to find
    /// the clostest one (next candidate).
    ///
    /// @tparam propagator_state_t state type of the propagator
    ///
    /// @param propagation contains the stepper and navigator states
    template <typename propagator_state_t>
    DETRAY_HOST_DEVICE inline bool init(
        propagator_state_t &propagation,
        const navigation::config &cfg = {}) const {

        state &navigation = propagation._navigation;
        const auto det = navigation.detector();
        const auto &track = propagation._stepping();
        const auto volume = detector_volume{*det, navigation.volume()};

        // Clean up state
        navigation.clear();
        navigation.m_heartbeat = true;
        // Get the max number of candidates & run them through the kernel
        // detail::call_reserve(navigation.candidates(), volume.n_objects());
        // @TODO: switch to fixed size buffer
        detail::call_reserve(navigation.candidates(), 20u);

        // Search for neighboring surfaces and fill candidates into cache
        volume.template visit_neighborhood<candidate_search>(
            track, cfg, *det, track, navigation.candidates(),
            cfg.mask_tolerance, cfg.overstep_tolerance);

        // Sort all candidates and pick the closest one
        detail::sequential_sort(navigation.candidates().begin(),
                                navigation.candidates().end());

        navigation.set_next(navigation.candidates().begin());
        // No unreachable candidates in cache after local navigation
        navigation.set_last(navigation.candidates().end());
        // Determine overall state of the navigation after updating the cache
        update_navigation_state(cfg, navigation);
        // If init was not successful, the propagation setup is broken
        if (navigation.trust_level() != navigation::trust_level::e_full) {
            navigation.m_heartbeat = false;
        }
        navigation.run_inspector(cfg, "Init complete: ");

        return navigation.m_heartbeat;
    }

    /// @brief Complete update of the nvaigation flow.
    ///
    /// Restores 'full trust' state to the cadidates cache and checks whether
    /// the track stepped onto a portal and a volume switch is due. If so, or
    /// when the previous update according to the given trust level
    /// failed to restore trust, it performs a complete reinitialization of the
    /// navigation.
    ///
    /// @tparam propagator_state_t state type of the propagator
    ///
    /// @param propagation contains the stepper and navigator states
    ///
    /// @return a heartbeat to indicate if the navigation is still alive
    template <typename propagator_state_t>
    DETRAY_HOST_DEVICE inline bool update(
        propagator_state_t &propagation,
        const navigation::config &cfg = {}) const {

        state &navigation = propagation._navigation;

        // Candidates are re-evaluated based on the current trust level.
        // Should result in 'full trust'
        update_kernel(propagation, cfg);

        // Update was completely successful (most likely case)
        if (navigation.trust_level() == navigation::trust_level::e_full) {
            return navigation.m_heartbeat;
        }
        // Otherwise: did we run into a portal?
        else if (navigation.is_on_portal()) {
            // Set volume index to the next volume provided by the portal
            navigation.set_volume(navigation.current()->volume_link);

            // Navigation reached the end of the detector world
            if (detail::is_invalid_value(navigation.volume())) {
                navigation.exit();
                return navigation.m_heartbeat;
            }
            // Run inspection when needed (keep for debugging)
            // navigation.run_inspector(cfg, "Volume switch: ");

            init(propagation, cfg);

            // Fresh initialization, reset trust and hearbeat
            navigation.m_trust_level = navigation::trust_level::e_full;
            navigation.m_heartbeat = true;

            return navigation.m_heartbeat;
        }
        // If no trust could be restored for the current state, (local)
        // navigation might be exhausted: re-initialize volume
        navigation.m_heartbeat &= init(propagation, cfg);

        // Sanity check: Should never be the case after complete update call
        if (navigation.trust_level() != navigation::trust_level::e_full or
            navigation.is_exhausted()) {
            navigation.abort();
        }

        return navigation.m_heartbeat;
    }

    private:
    /// Helper method to update the candidates (surface intersections)
    /// based on an externally provided trust level. Will (re-)initialize the
    /// navigation if there is no trust.
    ///
    /// @tparam propagator_state_t state type of the propagator
    ///
    /// @param propagation contains the stepper and navigator states
    template <typename propagator_state_t>
    DETRAY_HOST_DEVICE inline void update_kernel(
        propagator_state_t &propagation, const navigation::config &cfg) const {

        state &navigation = propagation._navigation;
        const auto det = navigation.detector();
        const auto &track = propagation._stepping();

        // Current candidates are up to date, nothing left to do
        if (navigation.trust_level() == navigation::trust_level::e_full) {
            return;
        }

        // Update only the current candidate and the corresponding next target
        // - do this only when the navigation state is still coherent
        if (navigation.trust_level() == navigation::trust_level::e_high ||
            (navigation.n_candidates() == 1 &&
             navigation.trust_level() == navigation::trust_level::e_high)) {

            // Update next candidate: If not reachable, 'high trust' is broken
            if (not update_candidate(*navigation.next(), track, det, cfg)) {
                navigation.m_status = navigation::status::e_unknown;
                navigation.set_no_trust();
                return;
            }

            // Update navigation flow on the new candidate information
            update_navigation_state(cfg, navigation);

            navigation.run_inspector(cfg, "Update complete: high trust: ");

            // The work is done if: the track has not reached a surface yet or
            // trust is gone (portal was reached or the cache is broken).
            if (navigation.status() == navigation::status::e_towards_object or
                navigation.trust_level() ==
                    navigation::trust_level::e_no_trust) {
                return;
            }

            // Else: Track is on module.
            // Ready the next candidate after the current module
            if (update_candidate(*navigation.next(), track, det, cfg)) {
                return;
            }

            // If next candidate is not reachable, don't 'return', but
            // escalate the trust level.
            // This will run into the fair trust case below.
            navigation.set_fair_trust();
        }

        // Re-evaluate all currently available candidates and sort again
        // - do this when your navigation state is stale, but not invalid
        if (navigation.trust_level() == navigation::trust_level::e_fair) {

            for (auto &candidate : navigation) {
                // Disregard this candidate if it is not reachable
                if (not update_candidate(candidate, track, det, cfg)) {
                    // Forcefully set dist to numeric max for sorting
                    candidate.path = std::numeric_limits<scalar_type>::max();
                }
            }
            detail::sequential_sort(navigation.begin(), navigation.end());
            // Take the nearest (sorted) candidate first
            navigation.set_next(navigation.begin());
            // Ignore unreachable elements (needed to determine exhaustion)
            navigation.set_last(find_invalid(navigation.candidates()));
            // Update navigation flow on the new candidate information
            update_navigation_state(cfg, navigation);

            navigation.run_inspector(cfg, "Update complete: fair trust: ");

            return;
        }

        // Actor flagged cache as broken (other cases of 'no trust' are
        // handeled after volume switch was checked in 'update()')
        if (navigation.trust_level() == navigation::trust_level::e_no_trust) {
            navigation.m_heartbeat &= init(propagation, cfg);
            return;
        }
    }

    /// @brief Helper method that re-establishes the navigation state after an
    /// update.
    ///
    /// It checks wether the track has reached a surface or is still moving
    /// towards the next surface candidate. If no new next candidate can be
    //  found, it flags 'no trust' in order to trigger a volume initialization.
    ///
    /// @tparam track_t the type of the track parametrisation
    /// @tparam propagator_state_t state type of the propagator
    ///
    /// @param track the track that belongs to the current propagation state
    /// @param propagation contains the stepper and navigator states
    DETRAY_HOST_DEVICE inline void update_navigation_state(
        const navigation::config &cfg, state &navigation) const {

        // Check whether the track reached the current candidate. Might be a
        // portal, in which case the navigation needs to be re-initialized
        if (navigation.is_on_object(*navigation.next(), cfg)) {
            // Set the next object that we want to reach (this function is only
            // called once the cache has been updated to a full trust state).
            // Might lead to exhausted cache.
            ++navigation.next();
            navigation.m_status = (navigation.current()->sf_desc.is_portal())
                                      ? navigation::status::e_on_portal
                                      : navigation::status::e_on_module;
        } else {
            // Otherwise the track is moving towards a surface
            navigation.m_status = navigation::status::e_towards_object;
        }
        // Exhaustion happens when after an update no next candidate in the
        // cache is reachable anymore -> triggers init of [new] volume
        // In backwards navigation or with strongly bent tracks, the cache may
        // not be exhausted when trying to exit the volume (the ray is seeing
        // the opposite side of the volume)
        navigation.m_trust_level =
            navigation.is_exhausted() || navigation.is_on_portal()
                ? navigation::trust_level::e_no_trust
                : navigation::trust_level::e_full;
    }

    /// @brief Helper method that updates the intersection of a single candidate
    /// and checks reachability
    ///
    /// @tparam track_t type of the track parametrization
    ///
    /// @param candidate the intersection to be updated
    /// @param track the track information
    ///
    /// @returns whether the track can reach this candidate.
    template <typename track_t>
    DETRAY_HOST_DEVICE inline bool update_candidate(
        intersection_type &candidate, const track_t &track,
        const detector_type *det, const navigation::config &cfg) const {

        if (candidate.sf_desc.barcode().is_invalid()) {
            return false;
        }

        const auto sf = surface{*det, candidate.sf_desc};

        // Check whether this candidate is reachable by the track
        return sf.template visit_mask<intersection_update>(
            detail::ray(track), candidate, det->transform_store(),
            sf.is_portal() ? 0.f : cfg.mask_tolerance, cfg.overstep_tolerance);
    }

    /// Helper to evict all unreachable/invalid candidates from the cache:
    /// Finds the first unreachable candidate (has been invalidated during
    /// update) in a sorted (!) cache.
    ///
    /// @param candidates the cache of candidates to be cleaned
    DETRAY_HOST_DEVICE inline auto find_invalid(
        vector_type<intersection_type> &candidates) const {
        // Depends on previous invalidation of unreachable candidates!
        auto not_reachable = [](const intersection_type &candidate) {
            return candidate.path == std::numeric_limits<scalar_type>::max();
        };

        return detail::find_if(candidates.begin(), candidates.end(),
                               not_reachable);
    }
};

/// @return the vecmem jagged vector buffer for surface candidates
// TODO: det.get_n_max_objects_per_volume() is way too many for
// candidates size allocation. With the local navigation, the size can be
// restricted to much smaller value
template <typename detector_t>
DETRAY_HOST vecmem::data::jagged_vector_buffer<intersection2D<
    typename detector_t::surface_type, typename detector_t::transform3>>
create_candidates_buffer(
    const detector_t &det, const std::size_t n_tracks,
    vecmem::memory_resource &device_resource,
    vecmem::memory_resource *host_access_resource = nullptr) {
    // Build the buffer from capacities, device and host accessible resources
    return vecmem::data::jagged_vector_buffer<intersection2D<
        typename detector_t::surface_type, typename detector_t::transform3>>(
        std::vector<std::size_t>(n_tracks, det.n_max_candidates()),
        device_resource, host_access_resource,
        vecmem::data::buffer_type::resizable);
}

}  // namespace detray
