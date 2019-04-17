#include "generator/check_model.hpp"

#include "defines.hpp"

#include "indexer/features_vector.hpp"
#include "indexer/classificator.hpp"
#include "indexer/feature_visibility.hpp"

#include "base/logging.hpp"


using namespace feature;

namespace check_model
{
  void ReadFeatures(std::string const & fName)
  {
    Classificator const & c = classif();

    FeaturesVectorTest(fName).GetVector().ForEach([&](FeatureType & ft, uint32_t) {
      TypesHolder types(ft);

      vector<uint32_t> vTypes;
      for (uint32_t t : types)
      {
        CHECK_EQUAL(c.GetTypeForIndex(c.GetIndexForType(t)), t, ());
        vTypes.push_back(t);
      }

      sort(vTypes.begin(), vTypes.end());
      CHECK(unique(vTypes.begin(), vTypes.end()) == vTypes.end(), ());

      m2::RectD const r = ft.GetLimitRect(FeatureType::BEST_GEOMETRY);
      CHECK(r.IsValid(), ());

      GeomType const type = ft.GetGeomType();
      if (type == GeomType::Line)
        CHECK_GREATER(ft.GetPointsCount(), 1, ());

      IsDrawableLike(vTypes, ft.GetGeomType());
    });

    LOG(LINFO, ("OK"));
  }
}
