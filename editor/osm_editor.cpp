#include "osm_editor.hpp"

#include "base/logging.hpp"

#include "coding/multilang_utf8_string.hpp"

#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/features_vector.hpp"

// TODO(AlexZ): Any way to avoid dependency from map library?
#include "map/framework.hpp"

#include "std/fstream.hpp"
#include "std/map.hpp"

#include "3party/pugixml/src/pugixml.hpp"


constexpr char const * kEditsFileName = "edits.xml";

// nullptr has special meaning: feature was deleted.
typedef map<FeatureID, unique_ptr<FeatureType>> FeaturesContainerT;

struct OSMEditorImpl
{
  Framework * framework;
  Index const * index;
  pugi::xml_document xml;
  FeaturesContainerT features;
};

// TODO(AlexZ): Optimize search by FeatureID.
OSMEditor::OSMEditor() : m_impl(new OSMEditorImpl)
{
  try
  {
    string xml;
    GetPlatform().GetReader(kEditsFileName)->ReadAsString(xml);
    pugi::xml_parse_result result = m_impl->xml.load_buffer(xml.data(), xml.size());
    if (result.status == pugi::status_ok)
      LOG(LINFO, ("Successfully loaded saved edits."));
  }
  catch (std::exception const &)
  {
  }
}

void OSMEditor::SetFrameworkAndIndex(Framework & framework, Index const & index)
{
  m_impl->framework = &framework;
  m_impl->index = &index;
}

OSMEditor::~OSMEditor()
{
  delete m_impl;
}

void OSMEditor::SaveToFile()
{
  ostringstream stream;
  m_impl->xml.save(stream);
  ofstream file(GetPlatform().WritablePathForFile(kEditsFileName));
  file << stream.str();
}

bool OSMEditor::WasFeatureEdited(FeatureID const & featureId) const
{
  if (m_impl->features.find(featureId) != m_impl->features.end())
    return true;
  return false;
}

bool OSMEditor::GetEditedFeature(FeatureID const & featureId, FeatureType & outFeature) const
{
  auto found = m_impl->features.find(featureId);
  if (found == m_impl->features.end() || !found->second)
    return false;
  outFeature = *found->second;
  return true;
}

void OSMEditor::DeleteFeature(FeatureID const & featureId)
{
  // Feature ID with null feature means it's "deleted".
  m_impl->features[featureId].reset(nullptr);
  // @TODO(AlexZ): Framework's Invalidate does not work for edited features.
  // Screen is not redrawn, should do few manual drags to redraw.
  m_impl->framework->Invalidate(true);
}

static void ChangeName(StringUtf8Multilang & featureName, string const & newName)
{
  // TODO(AlexZ): Take into an account all lanugages.
  featureName.AddString(StringUtf8Multilang::DEFAULT_CODE, newName);
}

void OSMEditor::EditFeatureName(FeatureID const & featureId, string const & newName)
{
  // TODO(AlexZ): Take into an account that name can be in different languages.
  auto found = m_impl->features.find(featureId);
  if (found == m_impl->features.end())
  {
    // Create copy of original MwM feature, and modify it's name.
    unique_ptr<FeatureType> fPtr;
    auto const featureFunctor = [&](FeatureType const & f)
    {
      // TODO(AlexZ): Ugly way to copy and use replaced features.
      // Should be a better way.
      f.ParseHeader2();
      f.ParseGeometry(19);
      f.ParseTriangles(19);
      f.ParseCommon();
      f.ParseMetadata();
      fPtr.reset(new FeatureType(f));
    };
    m_impl->index->ReadFeatures(featureFunctor, {featureId});
    if (!fPtr)
    {
      LOG(LERROR, ("Can't read feature while editing it's name:", featureId));
      return;
    }
    ChangeName(fPtr->m_params.name, newName);
    // TODO(AlexZ): Access to private feature impl...
    fPtr->m_header |= feature::HEADER_HAS_NAME;
    m_impl->features.emplace(featureId, move(fPtr));
    // @TODO(AlexZ): Framework's Invalidate does not work for edited features.
    // Screen is not redrawn, should do few manual drags to redraw.
    m_impl->framework->Invalidate(true);
  }
  else if (found->second)
  {
    // Change name of already edited feature.
    ChangeName(found->second->m_params.name, newName);
    // @TODO(AlexZ): Framework's Invalidate does not work for edited features.
    // Screen is not redrawn, should do few manual drags to redraw.
    m_impl->framework->Invalidate(true);
  }
  else
  {
    LOG(LERROR, ("Attempt to edit deleted feature", featureId));
  }
}
