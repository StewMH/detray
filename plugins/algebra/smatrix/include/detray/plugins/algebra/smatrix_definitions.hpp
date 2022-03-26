/** Detray library, part of the ACTS project (R&D line)
 *
 * (c) 2020-2022 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Algebra-Plugins include
#include "algebra/smatrix_smatrix.hpp"

#define __plugin algebra::smatrix
#define ALGEBRA_PLUGIN smatrix

namespace detray {

// Define scalar type
using scalar = DETRAY_CUSTOM_SCALARTYPE;

// Define namespace(s)
namespace getter = algebra::getter;
namespace vector = algebra::vector;
namespace matrix = algebra::matrix;

// Define matrix actor
template <typename T>
using matrix_actor = matrix::actor<T>;

}  // namespace detray
