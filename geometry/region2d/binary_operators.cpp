#include "geometry/region2d/boost_concept.hpp"
#include "geometry/region2d/binary_operators.hpp"


namespace m2
{
  using namespace boost::polygon;
  using namespace boost::polygon::operators;

  void SpliceRegions(std::vector<RegionI> & src, std::vector<RegionI> & res)
  {
    for (size_t i = 0; i < src.size(); ++i)
    {
      res.push_back(RegionI());
      res.back().Swap(src[i]);
    }
  }

  void IntersectRegions(RegionI const & r1, RegionI const & r2, std::vector<RegionI> & res)
  {
    std::vector<RegionI> local;
    local += (r1 * r2);
    SpliceRegions(local, res);
  }

  void DiffRegions(RegionI const & r1, RegionI const & r2, std::vector<RegionI> & res)
  {
    std::vector<RegionI> local;
    local += boost::polygon::operators::operator-(r1, r2);
    SpliceRegions(local, res);
  }
}
