#pragma once

#include "routing/turns.hpp"

#include <vector>

namespace ftypes
{
enum class HighwayClass;
}

namespace routing
{
namespace turns
{
/*!
 * \brief The TurnCandidate struct contains information about possible ways from a junction.
 */
struct TurnCandidate
{
  /*!
   * angle is an angle of the turn in degrees. It means angle is 180 minus
   * an angle between the current edge and the edge of the candidate. A counterclockwise rotation.
   * The current edge is an edge which belongs the route and located before the junction.
   * angle belongs to the range [-180; 180];
   */
  double angle;
  /*!
   * |m_nodeId| is a possible node (a possible way) from the juction.
   * |m_nodeId| contain either only unique NodeID for OSRM case or mwm id, feature id, segment id
   * and direction in case of A*.
   */
  UniNodeId m_nodeId;
  /*!
   * \brief highwayClass field for the road class caching. Because feature reading is a long
   * function.
   */
  ftypes::HighwayClass highwayClass;

  TurnCandidate(double a, UniNodeId const & n, ftypes::HighwayClass c)
    : angle(a), m_nodeId(n), highwayClass(c)
  {
  }
};

struct TurnCandidates
{
  std::vector<TurnCandidate> candidates;
  bool isCandidatesAngleValid;

  explicit TurnCandidates(bool angleValid = true) : isCandidatesAngleValid(angleValid) {}
};

}  // namespace routing
}  // namespace turns
