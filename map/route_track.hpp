#pragma once

#include "map/track.hpp"

#include "routing/turns.hpp"
#include "drape/drape_global.hpp"

#include "map/location_state.hpp"

class RouteTrack : public Track
{
public:
  explicit RouteTrack(PolylineD const & polyline) : Track(polyline, Params()) {}

  void SetTurnsGeometry(routing::turns::TTurnsGeom const & turnsGeom) { m_turnsGeom = turnsGeom; }
  
  void AddClosingSymbol(bool isBeginSymbol, string const & symbolName,
                        dp::Anchor pos, double depth);

private:
  void DeleteClosestSegmentDisplayList() const;

  struct ClosingSymbol
  {
    ClosingSymbol(string const & iconName, dp::Anchor pos, double depth)
      : m_iconName(iconName), m_position(pos), m_depth(depth) {}
    string m_iconName;
    dp::Anchor m_position;
    double m_depth;
  };

  vector<ClosingSymbol> m_beginSymbols;
  vector<ClosingSymbol> m_endSymbols;

  routing::turns::TTurnsGeom m_turnsGeom;
  mutable location::RouteMatchingInfo m_relevantMatchedInfo;
};

bool ClipArrowBodyAndGetArrowDirection(vector<m2::PointD> & ptsTurn, pair<m2::PointD, m2::PointD> & arrowDirection,
                                       size_t turnIndex, double beforeTurn, double afterTurn, double arrowLength);
bool MergeArrows(vector<m2::PointD> & ptsCurrentTurn, vector<m2::PointD> const & ptsNextTurn, double bodyLen, double arrowLen);
