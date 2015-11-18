#include "osm_editor.hpp"

#include "indexer/feature_decl.hpp"
#include "indexer/index.hpp"

#include "std/map.hpp"
#include "std/set.hpp"

namespace osm
{
// TODO(AlexZ, SergeyM): Use wrapper over XML (editor's "database" container).
struct Editor::Impl
{
  // TODO(AlexZ): Use better structure(s) to store data, think about xml tree.
  set<FeatureID> m_deletedFeatures;
  map<FeatureID, FeatureType> m_editedFeatures;

  Impl() { Load(); }
  void Load()
  {
    // TODO(AlexZ): Initialize from xml file.
  }
  void Save()
  {
    // TODO(AlexZ): Save to xml file.
  }
};

Editor::Editor() : m_impl(new Editor::Impl()) {}
Editor::~Editor() { delete m_impl; }
Editor::Impl & Editor::GetImpl()
{
  static Editor instance;
  return *instance.m_impl;
}

bool Editor::IsFeatureDeleted(FeatureID const & fid)
{
  Impl & impl = GetImpl();
  return impl.m_deletedFeatures.find(fid) != impl.m_deletedFeatures.end();
}

void Editor::DeleteFeature(FeatureID const & fid) { GetImpl().m_deletedFeatures.insert(fid); }
static void ChangeName(StringUtf8Multilang & featureName, string const & newName)
{
  // TODO(AlexZ): Take all lanugages into an account.
  featureName.AddString(StringUtf8Multilang::DEFAULT_CODE, newName);
}

static FeatureID GenerateNewFeatureId(FeatureID const & oldFeatureId)
{
  // TODO(AlexZ): Stable & unique features ID generation.
  static uint32_t newOffset = 0x0effffff;
  return FeatureID(oldFeatureId.m_mwmId, newOffset++);
}

void Editor::EditFeatureName(FeatureID const & fid, string const & newName, Index const & index)
{
  FeatureType editedFeature;
  Impl & impl = GetImpl();
  auto foundFeature = impl.m_editedFeatures.find(fid);
  if (foundFeature != impl.m_editedFeatures.end())
    editedFeature = foundFeature->second;
  else
  {
    // Create new "edited" feature.
    auto const ftReader = [&editedFeature](FeatureType const & originalFeature)
    {
      // TODO(AlexZ): Add XMLFeature constructor from unparsed FeatureType.
      originalFeature.ParseAll(17);
      originalFeature.ParseMetadata();
      editedFeature = originalFeature;
    };
    index.ReadFeatures(ftReader, {fid});
    // Replace existing feature id with a completely new one.
    editedFeature.SetID(GenerateNewFeatureId(editedFeature.GetID()));
  }
  // TODO(AlexZ): Access to private feature impl.
  ChangeName(editedFeature.m_params.name, newName);
  // TODO(AlexZ): Access to private feature impl.
  editedFeature.SetHeader(feature::HEADER_HAS_NAME | editedFeature.Header());

  impl.m_editedFeatures[fid] = editedFeature;

  // Delete the original feature.
  DeleteFeature(fid);
}

void Editor::ForEachFeatureInRectAndScale(TEmitterLambda f, m2::RectD const & /*rect*/,
                                          uint32_t /*scale*/)
{
  // TODO(AlexZ): Check rect and scale.
  for (auto & feature : GetImpl().m_editedFeatures)
    f(feature.second);
}

bool Editor::GetEditedFeature(FeatureID const & fid, FeatureType & outFeature)
{
  Impl & impl = GetImpl();
  auto found = impl.m_editedFeatures.find(fid);
  if (found != impl.m_editedFeatures.end())
  {
    outFeature = found->second;
    return true;
  }
  return false;
}

}  // namespace osm
