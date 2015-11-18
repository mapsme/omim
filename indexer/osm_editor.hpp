#pragma once

#include "geometry/rect2d.hpp"

#include "indexer/mwm_set.hpp"

#include "std/function.hpp"
#include "std/set.hpp"
#include "std/string.hpp"

struct FeatureID;
class FeatureType;
class Index;

namespace osm
{
class Editor final
{
  struct Impl;
  Impl * m_impl;

  Editor();
  ~Editor();
  static Impl & GetImpl();

public:
  using TEmitterLambda = function<void(FeatureType &)>;
  static void ForEachFeatureInRectAndScale(TEmitterLambda f, m2::RectD const & rect,
                                           uint32_t scale);

  template <class TFeaturesEmitter>
  struct Emitter
  {
    Emitter(TFeaturesEmitter & emitter, m2::RectD const & rect, uint32_t scale)
    {
      Editor::ForEachFeatureInRectAndScale(
          [&emitter](FeatureType & ft)
          {
            emitter(ft);
          },
          rect, scale);
    }
  };

  /// True if feature was deleted or edited by user.
  static bool IsFeatureDeleted(FeatureID const & fid);

  /// Marks feature as "deleted" from MwM file.
  /// Returns deleted feature's rect.
  static void DeleteFeature(FeatureID const & fid);

  /// Returns false if no feature with given fid was edited or created.
  static bool GetEditedFeature(FeatureID const & fid, FeatureType & outFeature);

  static void EditFeatureName(FeatureID const & fid, string const & newName, Index const & index);

};  // class Editor

}  // namespace osm
