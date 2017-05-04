#pragma once

#include "routing/index_router.hpp"
#include "routing/osrm_data_facade.hpp"
#include "routing/osrm_engine.hpp"
#include "routing/route.hpp"
#include "routing/router.hpp"
#include "routing/routing_mapping.hpp"

#include "geometry/tree4d.hpp"

#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

namespace feature
{
class TypesHolder;
}

class Index;
struct RawRouteData;
struct PhantomNode;
struct PathData;
class FeatureType;

namespace routing
{
class RoadGraphRouter;
struct RoutePathCross;
using TCheckedPath = vector<RoutePathCross>;

typedef vector<FeatureGraphNode> TFeatureGraphNodeVec;

class CarRouter : public IRouter
{
public:
  typedef vector<double> GeomTurnCandidateT;

  CarRouter(Index & index, TCountryFileFn const & countryFileFn,
            unique_ptr<IndexRouter> localRouter);

  virtual string GetName() const override;

  ResultCode CalculateRoute(m2::PointD const & startPoint, m2::PointD const & startDirection,
                            m2::PointD const & finalPoint, RouterDelegate const & delegate,
                            Route & route) override;

  virtual void ClearState() override;

protected:
  /*!
   * \brief FindPhantomNodes finds OSRM graph nodes by point and graph name.
   * \param mapName Name of the map without data file extension.
   * \param point Point in lon/lat coordinates.
   * \param direction Movement direction vector in planar coordinates.
   * \param res Result graph nodes.
   * \param maxCount Maximum count of graph nodes in the result vector.
   * \param mapping Reference to routing indexes.
   * \return NoError if there are any nodes in res. RouteNotFound otherwise.
   */
  IRouter::ResultCode FindPhantomNodes(m2::PointD const & point, m2::PointD const & direction,
                                       TFeatureGraphNodeVec & res, size_t maxCount,
                                       TRoutingMappingPtr const & mapping);

private:
  /*!
   * \brief Makes route (points turns and other annotations) from the map cross structs and submits
   * them to @route class
   * \warning monitors m_requestCancel flag for process interrupting.
   * \param path vector of pathes through mwms
   * \param route class to render final route
   * \return NoError or error code
   */
  ResultCode MakeRouteFromCrossesPath(TCheckedPath const & path, RouterDelegate const & delegate,
                                      Route & route);

  // @TODO(bykoianko) When routing section implementation is merged to master
  // this method should be moved to routing loader.
  bool DoesEdgeIndexExist(Index::MwmId const & mwmId);
  bool AllMwmsHaveRoutingIndex() const;
  bool ThereIsCrossMwmMix(Route & route) const;

  /*!
   * \brief Builds a route within one mwm using A* if edge index section is available and osrm
   * otherwise.
   * Then reconstructs the route and restores all route attributes.
   * \param route The found route is added to the |route| if the method returns true.
   */
  IRouter::ResultCode FindSingleRouteDispatcher(FeatureGraphNode const & source,
                                                FeatureGraphNode const & target,
                                                RouterDelegate const & delegate,
                                                TRoutingMappingPtr & mapping, Route & route);

  /*! Finds single shortest path in a single MWM between 2 sets of edges.
     * It's a route from multiple sources to multiple targets (MSMT).
     * \param source: vector of source edges to make path
     * \param target: vector of target edges to make path
     * \param facade: OSRM routing data facade to recover graph information
     * \param rawRoutingResult: routing result store
     * \return true when path exists, false otherwise.
     */
  IRouter::ResultCode FindRouteMSMT(TFeatureGraphNodeVec const & source, TFeatureGraphNodeVec const & target,
                                    RouterDelegate const & delegate, TRoutingMappingPtr & mapping, Route & route);

  Index & m_index;

  TFeatureGraphNodeVec m_cachedTargets;
  m2::PointD m_cachedTargetPoint;

  RoutingIndexManager m_indexManager;

  unique_ptr<IndexRouter> m_router;
};
}  // namespace routing
