// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "tests/Unit/TestingFramework.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/Tensor/IndexType.hpp"
#include "Domain/Domain.hpp"
#include "Domain/DomainCreators/Shell.hpp"
#include "NumericalAlgorithms/Interpolation/AddTemporalIdsToInterpolationTarget.hpp" // IWYU pragma: keep
#include "NumericalAlgorithms/Interpolation/InitializeInterpolationTarget.hpp"
#include "PointwiseFunctions/GeneralRelativity/Tags.hpp"
#include "Time/Slab.hpp"
#include "Time/Time.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/Rational.hpp"
#include "Utilities/Requires.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"
#include "tests/Unit/ActionTesting.hpp"
// IWYU pragma: no_forward_declare db::DataBox

/// \cond
class DataVector;
namespace intrp {
namespace Tags {
struct IndicesOfFilledInterpPoints;
template <typename Metavariables>
struct TemporalIds;
} // namespace Tags
} // namespace intrp
namespace Parallel {
template <typename Metavariables>
class ConstGlobalCache;
} // namespace Parallel
/// \endcond


namespace {

template <typename Metavariables, typename InterpolationTargetTag>
struct mock_interpolation_target {
  using metavariables = Metavariables;
  using chare_type = ActionTesting::MockArrayChare;
  using array_index = size_t;
  using const_global_cache_tag_list = tmpl::list<>;
  using action_list = tmpl::list<>;
  using initial_databox = db::compute_databox_type<
      typename ::intrp::Actions::InitializeInterpolationTarget<
          InterpolationTargetTag>::template return_tag_list<Metavariables, 3>>;
};

struct MockComputeTargetPoints {
  template <
      typename DbTags, typename... InboxTags, typename Metavariables,
      typename ArrayIndex, typename ActionList, typename ParallelComponent,
      Requires<tmpl::list_contains_v<
          DbTags, typename intrp::Tags::TemporalIds<Metavariables>>> = nullptr>
  static void apply(
      db::DataBox<DbTags>& box,
      const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      Parallel::ConstGlobalCache<Metavariables>& /*cache*/,
      const ArrayIndex& /*array_index*/, const ActionList /*meta*/,
      const ParallelComponent* const /*meta*/,
      const typename Metavariables::temporal_id& temporal_id) noexcept {
    Slab slab(0.0, 1.0);
    CHECK(temporal_id == Time(slab, 0));
    // Put something in IndicesOfFilledInterpPts so we can check later whether
    // this function was called.  This isn't the usual usage of
    // IndicesOfFilledInterpPoints.
    db::mutate<::intrp::Tags::IndicesOfFilledInterpPoints>(
        make_not_null(&box),
        [](const gsl::not_null<
            db::item_type<::intrp::Tags::IndicesOfFilledInterpPoints>*>
               indices) noexcept { indices->insert(indices->size() + 1); });
  }
};

struct MockMetavariables {
  struct InterpolationTargetA {
    using vars_to_interpolate_to_target =
        tmpl::list<gr::Tags::Lapse<DataVector>>;
    using compute_target_points = MockComputeTargetPoints;
  };
  using temporal_id = Time;
  using domain_frame = Frame::Inertial;

  using component_list = tmpl::list<
      mock_interpolation_target<MockMetavariables, InterpolationTargetA>>;
  using const_global_cache_tag_list = tmpl::list<>;
  enum class Phase { Initialize, Exit };
};

SPECTRE_TEST_CASE("Unit.NumericalAlgorithms.InterpolationTarget.AddTemporalIds",
                  "[Unit]") {
  using metavars = MockMetavariables;
  using MockRuntimeSystem = ActionTesting::MockRuntimeSystem<metavars>;
  using TupleOfMockDistributedObjects =
      MockRuntimeSystem::TupleOfMockDistributedObjects;
  TupleOfMockDistributedObjects dist_objects{};
  using MockDistributedObjectsTag =
      typename MockRuntimeSystem::template MockDistributedObjectsTag<
          mock_interpolation_target<metavars, metavars::InterpolationTargetA>>;
  tuples::get<MockDistributedObjectsTag>(dist_objects)
      .emplace(0,
               ActionTesting::MockDistributedObject<mock_interpolation_target<
                   metavars, metavars::InterpolationTargetA>>{});
  MockRuntimeSystem runner{{}, std::move(dist_objects)};

  const auto domain_creator =
      DomainCreators::Shell<Frame::Inertial>(0.9, 4.9, 1, {{5, 5}}, false);

  runner.simple_action<
      mock_interpolation_target<metavars, metavars::InterpolationTargetA>,
      ::intrp::Actions::InitializeInterpolationTarget<
          metavars::InterpolationTargetA>>(0, domain_creator.create_domain());

  const auto& box =
      runner
          .template algorithms<mock_interpolation_target<
              metavars, metavars::InterpolationTargetA>>()
          .at(0)
          .template get_databox<typename mock_interpolation_target<
              metavars, metavars::InterpolationTargetA>::initial_databox>();

  CHECK(db::get<::intrp::Tags::TemporalIds<metavars>>(box).empty());

  Slab slab(0.0, 1.0);
  const std::vector<Time> temporal_ids = {Time(slab, 0),
                                          Time(slab, Rational(1, 3))};

  runner.simple_action<
      mock_interpolation_target<metavars, metavars::InterpolationTargetA>,
      ::intrp::Actions::AddTemporalIdsToInterpolationTarget<
          metavars::InterpolationTargetA>>(0, temporal_ids);

  CHECK(db::get<::intrp::Tags::TemporalIds<metavars>>(box) ==
        std::deque<Time>(temporal_ids.begin(), temporal_ids.end()));

  runner.invoke_queued_simple_action<
      mock_interpolation_target<metavars, metavars::InterpolationTargetA>>(0);

  // Check that MockComputeTargetPoints was called.
  CHECK(db::get<::intrp::Tags::IndicesOfFilledInterpPoints>(box).size() == 1);

  // Call again; it should not call MockComputeTargetPoints this time.
  const std::vector<Time> temporal_ids_2 = {Time(slab, Rational(2, 3)),
                                            Time(slab, Rational(3, 3))};
  runner.simple_action<
      mock_interpolation_target<metavars, metavars::InterpolationTargetA>,
      ::intrp::Actions::AddTemporalIdsToInterpolationTarget<
          metavars::InterpolationTargetA>>(0, temporal_ids_2);

  // Check that MockComputeTargetPoints was not called.
  CHECK(runner.is_simple_action_queue_empty<
        mock_interpolation_target<metavars, metavars::InterpolationTargetA>>(
      0));
}

}  // namespace
