#include "generator/regions/region.hpp"

#include "generator/boost_helpers.hpp"
#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/place_point.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <numeric>

#include <boost/geometry.hpp>

namespace generator
{
namespace regions
{

Region::Region(FeatureBuilder1 const & fb, RegionDataProxy const & rd)
  : RegionWithName(fb.GetParams().name)
  , RegionWithData(rd)
  , m_polygon(std::make_shared<BoostPolygon>())
{
  FillPolygon(fb);
  boost::geometry::envelope(*m_polygon, m_rect);
  m_area = boost::geometry::area(*m_polygon);
}

Region::Region(StringUtf8Multilang const & name, RegionDataProxy const & rd,
               std::shared_ptr<BoostPolygon> const & polygon)
  : RegionWithName(name)
  , RegionWithData(rd)
  , m_polygon(polygon)
{
  boost::geometry::envelope(*m_polygon, m_rect);
  m_area = boost::geometry::area(*m_polygon);
}

void Region::DeletePolygon()
{
  m_polygon = nullptr;
}

void Region::FillPolygon(FeatureBuilder1 const & fb)
{
  CHECK(m_polygon, ());
  boost_helpers::FillPolygon(*m_polygon, fb);
}

bool Region::Contains(Region const & smaller) const
{
  CHECK(m_polygon, ());
  CHECK(smaller.m_polygon, ());

  return boost::geometry::covered_by(smaller.m_rect, m_rect) &&
      boost::geometry::covered_by(*smaller.m_polygon, *m_polygon);
}

double Region::CalculateOverlapPercentage(Region const & other) const
{
  CHECK(m_polygon, ());
  CHECK(other.m_polygon, ());

  if (!boost::geometry::intersects(other.m_rect, m_rect))
    return 0.0;

  std::vector<BoostPolygon> coll;
  boost::geometry::intersection(*other.m_polygon, *m_polygon, coll);
  auto const min = std::min(boost::geometry::area(*other.m_polygon),
                            boost::geometry::area(*m_polygon));
  auto const binOp = [](double x, BoostPolygon const & y) { return x + boost::geometry::area(y); };
  auto const sum = std::accumulate(std::begin(coll), std::end(coll), 0., binOp);
  return (sum / min) * 100;
}

bool Region::ContainsRect(Region const & smaller) const
{
  return boost::geometry::covered_by(smaller.m_rect, m_rect);
}

BoostPoint Region::GetCenter() const
{
  BoostPoint p;
  boost::geometry::centroid(m_rect, p);
  return p;
}

bool Region::Contains(PlacePoint const & placePoint) const
{
  CHECK(m_polygon, ());

  return Contains(placePoint.GetPosition());
}

bool Region::Contains(BoostPoint const & point) const
{
  CHECK(m_polygon, ());

  return boost::geometry::covered_by(point, m_rect) &&
      boost::geometry::covered_by(point, *m_polygon);
}
}  // namespace regions
}  // namespace generator
