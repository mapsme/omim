#include "check_model.hpp"

#include "../defines.hpp"

#include "../indexer/features_vector.hpp"

#include "../base/logging.hpp"


namespace check_model
{
  class DoFullRead
  {
  public:
    void operator() (FeatureType const & ft, uint32_t /*pos*/)
    {
      m2::RectD const r = ft.GetLimitRect(FeatureType::BEST_GEOMETRY);
      CHECK(r.IsValid(), ());
    }
  };

  void ReadFeatures(string const & fPath)
  {
    try
    {
      feature::SharedLoadInfo info(fPath);
      FeaturesVector vec(info);

      vec.ForEachOffset(DoFullRead());
    }
    catch (RootException const & e)
    {
      LOG(LERROR, ("Can't open or read file", fPath));
    }
  }
}
