// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "NumericalAlgorithms/Interpolation/InterpolationTargetWedgeSectionTorus.hpp"

#include <pup.h>

namespace intrp {
namespace OptionHolders {

WedgeSectionTorus::WedgeSectionTorus(
    const double min_radius_in, const double max_radius_in,
    const double min_theta_in, const double max_theta_in,
    const size_t number_of_radial_points_in,
    const size_t number_of_theta_points_in,
    const size_t number_of_phi_points_in, const bool use_uniform_radial_grid_in,
    const bool use_uniform_theta_grid_in, const OptionContext& context)
    : min_radius(min_radius_in),
      max_radius(max_radius_in),
      min_theta(min_theta_in),
      max_theta(max_theta_in),
      number_of_radial_points(number_of_radial_points_in),
      number_of_theta_points(number_of_theta_points_in),
      number_of_phi_points(number_of_phi_points_in),
      use_uniform_radial_grid(use_uniform_radial_grid_in),
      use_uniform_theta_grid(use_uniform_theta_grid_in) {
  if (min_radius >= max_radius) {
    PARSE_ERROR(context, "WedgeSectionTorus expects min_radius < max_radius");
  }
  if (min_theta >= max_theta) {
    PARSE_ERROR(context, "WedgeSectionTorus expects min_theta < max_theta");
  }
}

void WedgeSectionTorus::pup(PUP::er& p) noexcept {
  p | min_radius;
  p | max_radius;
  p | min_theta;
  p | max_theta;
  p | number_of_radial_points;
  p | number_of_theta_points;
  p | number_of_phi_points;
  p | use_uniform_radial_grid;
  p | use_uniform_theta_grid;
}

}  // namespace OptionHolders
}  // namespace intrp
