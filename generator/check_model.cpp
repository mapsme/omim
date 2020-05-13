#include "generator/check_model.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/features_tag.hpp"
#include "indexer/features_vector.hpp"

#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include "defines.hpp"

#include <vector>

using namespace feature;

namespace check_model
{
void ReadFeatures(std::string const & fName)
{
  Classificator const & c = classif();

  for (auto const tag : GetFeaturesTags(FeaturesEnumerationMode::All))
  {
    auto const fv = FeaturesVectorTest(fName, tag);
    fv.GetVector().ForEach([&](FeatureType & ft, uint32_t) {
      TypesHolder types(ft);

      std::vector<uint32_t> vTypes;
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
  }

  LOG(LINFO, ("OK"));
}
}
