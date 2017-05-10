#pragma once

#include "routing/loaded_path_segment.hpp"
#include "routing/road_graph.hpp"
#include "routing/turn_candidate.hpp"

#include <vector>

namespace routing
{
namespace turns
{
/*!
 * \brief The IRoutingResult interface for the routing result. Uncouple router from the
 * annotation code that describes turns. See routers for detail implementations.
 */
class IRoutingResult
{
public:
  /// \returns information about all route segments.
  virtual TUnpackedPathSegments const & GetSegments() const = 0;
  /// \brief For a |node|, |junctionPoint| and |ingoingPoint| (point before the |node|)
  /// this method computes number of ingoing ways to |junctionPoint| and fills |outgoingTurns|.
  virtual void GetPossibleTurns(UniNodeId const & node, m2::PointD const & ingoingPoint,
                                m2::PointD const & junctionPoint, size_t & ingoingCount,
                                TurnCandidates & outgoingTurns) const = 0;
  virtual double GetPathLength() const = 0;
  virtual Junction GetStartPoint() const = 0;
  virtual Junction GetEndPoint() const = 0;

  virtual ~IRoutingResult() = default;
};
}  // namespace routing
}  // namespace turns
