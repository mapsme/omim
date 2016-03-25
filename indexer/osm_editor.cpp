#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/edits_migration.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/feature_impl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/index.hpp"
#include "indexer/osm_editor.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "editor/changeset_wrapper.hpp"
#include "editor/osm_auth.hpp"
#include "editor/server_api.hpp"
#include "editor/xml_feature.hpp"

#include "coding/internal/file_data.hpp"

#include "geometry/algorithm.hpp"

#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"
#include "base/thread_checker.hpp"
#include "base/timer.hpp"

#include "std/algorithm.hpp"
#include "std/chrono.hpp"
#include "std/future.hpp"
#include "std/mutex.hpp"
#include "std/target_os.hpp"
#include "std/tuple.hpp"
#include "std/unordered_map.hpp"
#include "std/unordered_set.hpp"

#include "3party/Alohalytics/src/alohalytics.h"
#include "3party/pugixml/src/pugixml.hpp"

using namespace pugi;
using feature::EGeomType;
using feature::Metadata;
using editor::XMLFeature;

namespace
{
constexpr char const * kEditorXMLFileName = "edits.xml";
constexpr char const * kXmlRootNode = "mapsme";
constexpr char const * kXmlMwmNode = "mwm";
constexpr char const * kDeleteSection = "delete";
constexpr char const * kModifySection = "modify";
constexpr char const * kCreateSection = "create";
/// We store edited streets in OSM-compatible way.
constexpr char const * kAddrStreetTag = "addr:street";

constexpr char const * kUploaded = "Uploaded";
constexpr char const * kDeletedFromOSMServer = "Deleted from OSM by someone";
constexpr char const * kRelationsAreNotSupported = "Relations are not supported yet";
constexpr char const * kNeedsRetry = "Needs Retry";
constexpr char const * kWrongMatch = "Matched feature has no tags";

bool NeedsUpload(string const & uploadStatus)
{
  return uploadStatus != kUploaded &&
      uploadStatus != kDeletedFromOSMServer &&
      // TODO: Remove this line when relations are supported.
      uploadStatus != kRelationsAreNotSupported &&
      // TODO: Remove this when we have better matching algorithm.
      uploadStatus != kWrongMatch;
}

string GetEditorFilePath() { return GetPlatform().WritablePathForFile(kEditorXMLFileName); }

/// Compares editable fields connected with feature ignoring street.
bool AreFeaturesEqualButStreet(FeatureType const & a, FeatureType const & b)
{
  feature::TypesHolder const aTypes(a);
  feature::TypesHolder const bTypes(b);

  if (!aTypes.Equals(bTypes))
    return false;

  if (a.GetHouseNumber() != b.GetHouseNumber())
    return false;

  if (!a.GetMetadata().Equals(b.GetMetadata()))
      return false;

  if (a.GetNames() != b.GetNames())
    return false;

  return true;
}

XMLFeature GetMatchingFeatureFromOSM(osm::ChangesetWrapper & cw,
                                     unique_ptr<FeatureType const> featurePtr)
{
  ASSERT_NOT_EQUAL(featurePtr->GetFeatureType(), feature::GEOM_LINE,
                   ("Line features are not supported yet."));
  if (featurePtr->GetFeatureType() == feature::GEOM_POINT)
    return cw.GetMatchingNodeFeatureFromOSM(featurePtr->GetCenter());

  // Warning: geometry is cached in FeatureType. So if it wasn't BEST_GEOMETRY,
  // we can never have it. Features here came from editors loader and should
  // have BEST_GEOMETRY geometry.
  auto geometry = featurePtr->GetTriangesAsPoints(FeatureType::BEST_GEOMETRY);

  // Filters out duplicate points for closed ways or triangles' vertices.
  my::SortUnique(geometry);

  ASSERT_GREATER_OR_EQUAL(geometry.size(), 3, ("Is it an area feature?"));

  return cw.GetMatchingAreaFeatureFromOSM(geometry);
}

uint64_t GetMwmCreationTimeByMwmId(MwmSet::MwmId const & mwmId)
{
  return mwmId.GetInfo()->m_version.GetSecondsSinceEpoch();
}

bool IsObsolete(editor::XMLFeature const & xml, FeatureID const & fid)
{
  // TODO(mgsergio): If xml and feature are identical return true
  auto const uploadTime = xml.GetUploadTime();
  return uploadTime != my::INVALID_TIME_STAMP &&
         my::TimeTToSecondsSinceEpoch(uploadTime) < GetMwmCreationTimeByMwmId(fid.m_mwmId);
}
} // namespace

namespace osm
{
// TODO(AlexZ): Normalize osm multivalue strings for correct merging
// (e.g. insert/remove spaces after ';' delimeter);

Editor::Editor() : m_notes(editor::Notes::MakeNotes()) {}
Editor & Editor::Instance()
{
  static Editor instance;
  return instance;
}

void Editor::LoadMapEdits()
{
  if (!m_mwmIdByMapNameFn)
  {
    LOG(LERROR, ("Can't load any map edits, MwmIdByNameAndVersionFn has not been set."));
    return;
  }

  xml_document doc;
  {
    string const fullFilePath = GetEditorFilePath();
    xml_parse_result const res = doc.load_file(fullFilePath.c_str());
    // Note: status_file_not_found is ok if user has never made any edits.
    if (res != status_ok && res != status_file_not_found)
    {
      LOG(LERROR, ("Can't load map edits from disk:", fullFilePath));
      return;
    }
  }

  array<pair<FeatureStatus, char const *>, 3> const sections =
  {{
      {FeatureStatus::Deleted, kDeleteSection},
      {FeatureStatus::Modified, kModifySection},
      {FeatureStatus::Created, kCreateSection}
  }};
  int deleted = 0, modified = 0, created = 0;

  bool needRewriteEdits = false;

  for (xml_node mwm : doc.child(kXmlRootNode).children(kXmlMwmNode))
  {
    string const mapName = mwm.attribute("name").as_string("");
    int64_t const mapVersion = mwm.attribute("version").as_llong(0);
    MwmSet::MwmId const mwmId = m_mwmIdByMapNameFn(mapName);
    // TODO(mgsergio, AlexZ): Is it normal to have isMwmIdAlive and mapVersion
    // NOT equal to mwmId.GetInfo()->GetVersion() at the same time?
    auto const needMigrateEdits = !mwmId.IsAlive() || mapVersion != mwmId.GetInfo()->GetVersion();
    needRewriteEdits |= needMigrateEdits;

    for (auto const & section : sections)
    {
      for (auto const nodeOrWay : mwm.child(section.second).select_nodes("node|way"))
      {
        try
        {
          XMLFeature const xml(nodeOrWay.node());

          auto const fid = needMigrateEdits
                               ? editor::MigrateFeatureIndex(m_forEachFeatureAtPointFn, xml)
                               : FeatureID(mwmId, xml.GetMWMFeatureIndex());

          // Remove obsolete edit during migration.
          if (needMigrateEdits && IsObsolete(xml, fid))
            continue;

          FeatureTypeInfo fti;
          if (section.first == FeatureStatus::Created)
          {
            fti.m_feature.FromXML(xml);
          }
          else
          {
            fti.m_feature = *m_getOriginalFeatureFn(fid);
            fti.m_feature.ApplyPatch(xml);
          }

          fti.m_feature.SetID(fid);
          fti.m_street = xml.GetTagValue(kAddrStreetTag);

          fti.m_modificationTimestamp = xml.GetModificationTime();
          ASSERT_NOT_EQUAL(my::INVALID_TIME_STAMP, fti.m_modificationTimestamp, ());
          fti.m_uploadAttemptTimestamp = xml.GetUploadTime();
          fti.m_uploadStatus = xml.GetUploadStatus();
          fti.m_uploadError = xml.GetUploadError();
          fti.m_status = section.first;
          switch (section.first)
          {
          case FeatureStatus::Deleted: ++deleted; break;
          case FeatureStatus::Modified: ++modified; break;
          case FeatureStatus::Created: ++created; break;
          case FeatureStatus::Untouched: ASSERT(false, ()); continue;
          }
          // Insert initialized structure at the end: exceptions are possible in above code.
          m_features[fid.m_mwmId].emplace(fid.m_index, move(fti));
        }
        catch (editor::XMLFeatureError const & ex)
        {
          ostringstream s;
          nodeOrWay.node().print(s, "  ");
          LOG(LERROR, (ex.what(), "Can't create XMLFeature in section", section.second, s.str()));
        }
        catch (editor::MigrationError const & ex)
        {
          LOG(LWARNING, (ex.Msg(), "mwmId =", mwmId, XMLFeature(nodeOrWay.node())));
        }
      } // for nodes
    } // for sections
  } // for mwms

  // Save edits with new indexes and mwm version to avoid another migration on next startup.
  if (needRewriteEdits)
    Save(GetEditorFilePath());
  LOG(LINFO, ("Loaded", modified, "modified,", created, "created and", deleted, "deleted features."));
}

bool Editor::Save(string const & fullFilePath) const
{
  // TODO(AlexZ): Improve synchronization in Editor code.
  static mutex saveMutex;
  lock_guard<mutex> lock(saveMutex);

  if (m_features.empty())
  {
    my::DeleteFileX(GetEditorFilePath());
    return true;
  }

  xml_document doc;
  xml_node root = doc.append_child(kXmlRootNode);
  // Use format_version for possible future format changes.
  root.append_attribute("format_version") = 1;
  for (auto const & mwm : m_features)
  {
    xml_node mwmNode = root.append_child(kXmlMwmNode);
    mwmNode.append_attribute("name") = mwm.first.GetInfo()->GetCountryName().c_str();
    mwmNode.append_attribute("version") = static_cast<long long>(mwm.first.GetInfo()->GetVersion());
    xml_node deleted = mwmNode.append_child(kDeleteSection);
    xml_node modified = mwmNode.append_child(kModifySection);
    xml_node created = mwmNode.append_child(kCreateSection);
    for (auto const & index : mwm.second)
    {
      FeatureTypeInfo const & fti = index.second;
      // TODO: Do we really need to serialize deleted features in full details? Looks like mwm ID and meta fields are enough.
      XMLFeature xf = fti.m_feature.ToXML();
      xf.SetMWMFeatureIndex(index.first);
      if (!fti.m_street.empty())
        xf.SetTagValue(kAddrStreetTag, fti.m_street);
      ASSERT_NOT_EQUAL(0, fti.m_modificationTimestamp, ());
      xf.SetModificationTime(fti.m_modificationTimestamp);
      if (fti.m_uploadAttemptTimestamp != my::INVALID_TIME_STAMP)
      {
        xf.SetUploadTime(fti.m_uploadAttemptTimestamp);
        ASSERT(!fti.m_uploadStatus.empty(), ("Upload status updates with upload timestamp."));
        xf.SetUploadStatus(fti.m_uploadStatus);
        if (!fti.m_uploadError.empty())
          xf.SetUploadError(fti.m_uploadError);
      }
      switch (fti.m_status)
      {
      case FeatureStatus::Deleted: VERIFY(xf.AttachToParentNode(deleted), ()); break;
      case FeatureStatus::Modified: VERIFY(xf.AttachToParentNode(modified), ()); break;
      case FeatureStatus::Created: VERIFY(xf.AttachToParentNode(created), ()); break;
      case FeatureStatus::Untouched: CHECK(false, ("Not edited features shouldn't be here."));
      }
    }
  }

  return my::WriteToTempAndRenameToFile(
      fullFilePath,
      [&doc](string const & fileName)
      {
        return doc.save_file(fileName.data(), "  ");
      });
}

void Editor::ClearAllLocalEdits()
{
  m_features.clear();
  Save(GetEditorFilePath());
  m_invalidateFn();
}

Editor::FeatureStatus Editor::GetFeatureStatus(MwmSet::MwmId const & mwmId, uint32_t index) const
{
  // Most popular case optimization.
  if (m_features.empty())
    return FeatureStatus::Untouched;

  auto const matchedMwm = m_features.find(mwmId);
  if (matchedMwm == m_features.end())
    return FeatureStatus::Untouched;

  auto const matchedIndex = matchedMwm->second.find(index);
  if (matchedIndex == matchedMwm->second.end())
    return FeatureStatus::Untouched;

  return matchedIndex->second.m_status;
}

void Editor::DeleteFeature(FeatureType const & feature)
{
  FeatureID const & fid = feature.GetID();
  auto const mwm = m_features.find(fid.m_mwmId);
  if (mwm != m_features.end())
  {
    auto const f = mwm->second.find(fid.m_index);
    // Created feature is deleted by removing all traces of it.
    if (f != mwm->second.end() && f->second.m_status == FeatureStatus::Created)
    {
      mwm->second.erase(f);
      return;
    }
  }

  FeatureTypeInfo & fti = m_features[fid.m_mwmId][fid.m_index];
  fti.m_status = FeatureStatus::Deleted;
  // TODO: What if local client time is absolutely wrong?
  fti.m_modificationTimestamp = time(nullptr);
  // TODO: We don't really need to serialize whole feature. Improve this code in the future.
  fti.m_feature = feature;

  // TODO(AlexZ): Synchronize Save call/make it on a separate thread.
  Save(GetEditorFilePath());

  Invalidate();
}

namespace
{
constexpr uint32_t kStartIndexForCreatedFeatures = numeric_limits<uint32_t>::max() - 0xfffff;
bool IsCreatedFeature(FeatureID const & fid) { return fid.m_index >= kStartIndexForCreatedFeatures; }
}  // namespace

Editor::SaveResult Editor::SaveEditedFeature(EditableMapObject const & emo)
{
  FeatureID const & fid = emo.GetID();
  FeatureTypeInfo fti;
  bool const isCreated = IsCreatedFeature(fid);
  if (isCreated)
  {
    fti.m_status = FeatureStatus::Created;
    fti.m_feature.ReplaceBy(emo);
  }
  else
  {
    fti.m_feature = *m_getOriginalFeatureFn(fid);
    fti.m_feature.ReplaceBy(emo);
    if (AreFeaturesEqualButStreet(fti.m_feature, *m_getOriginalFeatureFn(fid)) &&
        m_getOriginalFeatureStreetFn(fti.m_feature) == emo.GetStreet())
    {
      RemoveFeatureFromStorageIfExists(fid.m_mwmId, fid.m_index);
      // TODO(AlexZ): Synchronize Save call/make it on a separate thread.
      Save(GetEditorFilePath());
      Invalidate();
      return NothingWasChanged;
    }
    fti.m_status = FeatureStatus::Modified;
  }
  // TODO: What if local client time is absolutely wrong?
  fti.m_modificationTimestamp = time(nullptr);
  fti.m_street = emo.GetStreet();
  // Reset upload status so already uploaded features can be uploaded again after modification.
  fti.m_uploadStatus = {};
  m_features[fid.m_mwmId][fid.m_index] = move(fti);

  // TODO(AlexZ): Synchronize Save call/make it on a separate thread.
  bool const savedSuccessfully = Save(GetEditorFilePath());
  Invalidate();
  return savedSuccessfully ? SavedSuccessfully : NoFreeSpaceError;
}

void Editor::ForEachFeatureInMwmRectAndScale(MwmSet::MwmId const & id,
                                             TFeatureIDFunctor const & f,
                                             m2::RectD const & rect,
                                             uint32_t /*scale*/)
{
  auto const mwmFound = m_features.find(id);
  if (mwmFound == m_features.end())
    return;

  // TODO(AlexZ): Check that features are visible at this scale.
  // Process only new (created) features.
  for (auto const & index : mwmFound->second)
  {
    FeatureTypeInfo const & ftInfo = index.second;
    if (ftInfo.m_status == FeatureStatus::Created &&
        rect.IsPointInside(ftInfo.m_feature.GetCenter()))
      f(FeatureID(id, index.first));
  }
}

void Editor::ForEachFeatureInMwmRectAndScale(MwmSet::MwmId const & id,
                                             TFeatureTypeFunctor const & f,
                                             m2::RectD const & rect,
                                             uint32_t /*scale*/)
{
  auto mwmFound = m_features.find(id);
  if (mwmFound == m_features.end())
    return;

  // TODO(AlexZ): Check that features are visible at this scale.
  // Process only new (created) features.
  for (auto & index : mwmFound->second)
  {
    FeatureTypeInfo & ftInfo = index.second;
    if (ftInfo.m_status == FeatureStatus::Created &&
        rect.IsPointInside(ftInfo.m_feature.GetCenter()))
      f(ftInfo.m_feature);
  }
}

bool Editor::GetEditedFeature(MwmSet::MwmId const & mwmId, uint32_t index, FeatureType & outFeature) const
{
  auto const matchedMwm = m_features.find(mwmId);
  if (matchedMwm == m_features.end())
    return false;

  auto const matchedIndex = matchedMwm->second.find(index);
  if (matchedIndex == matchedMwm->second.end())
    return false;

  // TODO(AlexZ): Should we process deleted/created features as well?
  outFeature = matchedIndex->second.m_feature;
  return true;
}

bool Editor::GetEditedFeatureStreet(FeatureID const & fid, string & outFeatureStreet) const
{
  // TODO(AlexZ): Reuse common code or better make better getters/setters for edited features.
  auto const matchedMwm = m_features.find(fid.m_mwmId);
  if (matchedMwm == m_features.end())
    return false;

  auto const matchedIndex = matchedMwm->second.find(fid.m_index);
  if (matchedIndex == matchedMwm->second.end())
    return false;

  // TODO(AlexZ): Should we process deleted/created features as well?
  outFeatureStreet = matchedIndex->second.m_street;
  return true;
}

vector<uint32_t> Editor::GetFeaturesByStatus(MwmSet::MwmId const & mwmId, FeatureStatus status) const
{
  vector<uint32_t> features;
  auto const matchedMwm = m_features.find(mwmId);
  if (matchedMwm == m_features.end())
    return features;
  for (auto const & index : matchedMwm->second)
  {
    if (index.second.m_status == status)
      features.push_back(index.first);
  }
  sort(features.begin(), features.end());
  return features;
}

EditableProperties Editor::GetEditableProperties(FeatureType const & feature) const
{
  // Disable editor for old data.
  if (!version::IsSingleMwm(feature.GetID().m_mwmId.GetInfo()->m_version.GetVersion()))
    return {};
  // TODO(mgsergio): Check if feature is in the area where editing is disabled in the config.
  return GetEditablePropertiesForTypes(feature::TypesHolder(feature));
}
// private
EditableProperties Editor::GetEditablePropertiesForTypes(feature::TypesHolder const & types) const
{
  editor::TypeAggregatedDescription desc;
  if (m_config.GetTypeDescription(types.ToObjectNames(), desc))
    return {desc.GetEditableFields(), desc.IsNameEditable(), desc.IsAddressEditable()};
  return {};
}

bool Editor::HaveMapEditsToUpload() const
{
  for (auto const & id : m_features)
  {
    for (auto const & index : id.second)
    {
      if (NeedsUpload(index.second.m_uploadStatus))
        return true;
    }
  }
  return false;
}

bool Editor::HaveMapEditsOrNotesToUpload() const
{
  if (m_notes->NotUploadedNotesCount() != 0)
    return true;

  return HaveMapEditsToUpload();
}

bool Editor::HaveMapEditsToUpload(MwmSet::MwmId const & mwmId) const
{
  auto const found = m_features.find(mwmId);
  if (found != m_features.end())
  {
    for (auto const & index : found->second)
    {
      if (NeedsUpload(index.second.m_uploadStatus))
        return true;
    }
  }
  return false;
}

void Editor::UploadChanges(string const & key, string const & secret, TChangesetTags tags,
                           TFinishUploadCallback callBack)
{
  if (m_notes->NotUploadedNotesCount())
    UploadNotes(key, secret);

  if (!HaveMapEditsToUpload())
  {
    LOG(LDEBUG, ("There are no local edits to upload."));
    return;
  }

  alohalytics::LogEvent("Editor_DataSync_started");

  // TODO(AlexZ): features access should be synchronized.
  auto const upload = [this](string key, string secret, TChangesetTags tags, TFinishUploadCallback callBack)
  {
    // This lambda was designed to start after app goes into background. But for cases when user is immediately
    // coming back to the app we work with a copy, because 'for' loops below can take a significant amount of time.
    auto features = m_features;

    int uploadedFeaturesCount = 0, errorsCount = 0;
    ChangesetWrapper changeset({key, secret}, tags);
    for (auto & id : features)
    {
      for (auto & index : id.second)
      {
        FeatureTypeInfo & fti = index.second;
        // Do not process already uploaded features or those failed permanently.
        if (!NeedsUpload(fti.m_uploadStatus))
          continue;

        string ourDebugFeatureString;

        try
        {
          XMLFeature feature = fti.m_feature.ToXML();
          if (!fti.m_street.empty())
            feature.SetTagValue(kAddrStreetTag, fti.m_street);

          ourDebugFeatureString = DebugPrint(feature);

          switch (fti.m_status)
          {
          case FeatureStatus::Untouched: CHECK(false, ("It's impossible.")); continue;

          case FeatureStatus::Created:
            {
              ASSERT_EQUAL(feature.GetType(), XMLFeature::Type::Node,
                           ("Linear and area features creation is not supported yet."));
              try
              {
                XMLFeature osmFeature = changeset.GetMatchingNodeFeatureFromOSM(fti.m_feature.GetCenter());
                // If we are here, it means that object already exists at the given point.
                // To avoid nodes duplication, merge and apply changes to it instead of creating an new one.
                XMLFeature const osmFeatureCopy = osmFeature;
                osmFeature.ApplyPatch(feature);
                // Check to avoid uploading duplicates into OSM.
                if (osmFeature == osmFeatureCopy)
                {
                  LOG(LWARNING, ("Local changes are equal to OSM, feature has not been uploaded.", osmFeatureCopy));
                  // Don't delete this local change right now for user to see it in profile.
                  // It will be automatically deleted by migration code on the next maps update.
                }
                else
                {
                  LOG(LDEBUG, ("Create case: uploading patched feature", osmFeature));
                  changeset.Modify(osmFeature);
                }
              }
              catch (ChangesetWrapper::OsmObjectWasDeletedException const &)
              {
                // Object was never created by anyone else - it's safe to create it.
                changeset.Create(feature);
              }
              catch (...)
              {
                // Pas network or other errors to outside exception handler.
                throw;
              }
            }
            break;

          case FeatureStatus::Modified:
            {
              XMLFeature osmFeature =
                  GetMatchingFeatureFromOSM(changeset, m_getOriginalFeatureFn(fti.m_feature.GetID()));
              XMLFeature const osmFeatureCopy = osmFeature;
              osmFeature.ApplyPatch(feature);
              // Check to avoid uploading duplicates into OSM.
              if (osmFeature == osmFeatureCopy)
              {
                LOG(LWARNING, ("Local changes are equal to OSM, feature has not been uploaded.", osmFeatureCopy));
                // Don't delete this local change right now for user to see it in profile.
                // It will be automatically deleted by migration code on the next maps update.
              }
              else
              {
                LOG(LDEBUG, ("Uploading patched feature", osmFeature));
                changeset.Modify(osmFeature);
              }
            }
            break;

          case FeatureStatus::Deleted:
            changeset.Delete(GetMatchingFeatureFromOSM(
                changeset, m_getOriginalFeatureFn(fti.m_feature.GetID())));
            break;
          }
          fti.m_uploadStatus = kUploaded;
          fti.m_uploadError.clear();
          ++uploadedFeaturesCount;
        }
        catch (ChangesetWrapper::OsmObjectWasDeletedException const & ex)
        {
          fti.m_uploadStatus = kDeletedFromOSMServer;
          fti.m_uploadError = ex.what();
          ++errorsCount;
          LOG(LWARNING, (ex.what()));
        }
        catch (ChangesetWrapper::RelationFeatureAreNotSupportedException const & ex)
        {
          fti.m_uploadStatus = kRelationsAreNotSupported;
          fti.m_uploadError = ex.what();
          ++errorsCount;
          LOG(LWARNING, (ex.what()));
        }
        catch (ChangesetWrapper::EmptyFeatureException const & ex)
        {
          fti.m_uploadStatus = kWrongMatch;
          fti.m_uploadError = ex.what();
          ++errorsCount;
          LOG(LWARNING, (ex.what()));
        }
        catch (RootException const & ex)
        {
          fti.m_uploadStatus = kNeedsRetry;
          fti.m_uploadError = ex.what();
          ++errorsCount;
          LOG(LWARNING, (ex.what()));
        }
        // TODO(AlexZ): Use timestamp from the server.
        fti.m_uploadAttemptTimestamp = time(nullptr);

        if (fti.m_uploadStatus != kUploaded)
        {
          ms::LatLon const ll = MercatorBounds::ToLatLon(feature::GetCenter(fti.m_feature));
          alohalytics::LogEvent("Editor_DataSync_error", {{"type", fti.m_uploadStatus},
                                {"details", fti.m_uploadError}, {"our", ourDebugFeatureString},
                                {"mwm", fti.m_feature.GetID().GetMwmName()},
                                {"mwm_version", strings::to_string(fti.m_feature.GetID().GetMwmVersion())}},
                                alohalytics::Location::FromLatLon(ll.lat, ll.lon));
        }
        // Call Save every time we modify each feature's information.
        SaveUploadedInformation(fti);
      }
    }

    alohalytics::LogEvent("Editor_DataSync_finished", {{"errors", strings::to_string(errorsCount)},
                          {"uploaded", strings::to_string(uploadedFeaturesCount)},
                          {"changeset", strings::to_string(changeset.GetChangesetId())}});
    if (callBack)
    {
      UploadResult result = UploadResult::NothingToUpload;
      if (uploadedFeaturesCount)
        result = UploadResult::Success;
      else if (errorsCount)
        result = UploadResult::Error;
      callBack(result);
    }
  };

  // Do not run more than one upload thread at a time.
  static auto future = async(launch::async, upload, key, secret, tags, callBack);
  auto const status = future.wait_for(milliseconds(0));
  if (status == future_status::ready)
    future = async(launch::async, upload, key, secret, tags, callBack);
}

void Editor::SaveUploadedInformation(FeatureTypeInfo const & fromUploader)
{
  // TODO(AlexZ): Correctly synchronize this call and Save() at the end.
  FeatureID const & fid = fromUploader.m_feature.GetID();
  auto id = m_features.find(fid.m_mwmId);
  if (id == m_features.end())
    return;  // Rare case: feature was deleted at the time of changes uploading.
  auto index = id->second.find(fid.m_index);
  if (index == id->second.end())
    return;  // Rare case: feature was deleted at the time of changes uploading.
  auto & fti = index->second;
  fti.m_uploadAttemptTimestamp = fromUploader.m_uploadAttemptTimestamp;
  fti.m_uploadStatus = fromUploader.m_uploadStatus;
  fti.m_uploadError = fromUploader.m_uploadError;
  Save(GetEditorFilePath());
}

void Editor::RemoveFeatureFromStorageIfExists(MwmSet::MwmId const & mwmId, uint32_t index)
{
  auto matchedMwm = m_features.find(mwmId);
  if (matchedMwm == m_features.end())
    return;

  auto matchedIndex = matchedMwm->second.find(index);
  if (matchedIndex != matchedMwm->second.end())
    matchedMwm->second.erase(matchedIndex);

  if (matchedMwm->second.empty())
    m_features.erase(matchedMwm);
}

void Editor::Invalidate()
{
  if (m_invalidateFn)
    m_invalidateFn();
}

Editor::Stats Editor::GetStats() const
{
  Stats stats;
  LOG(LDEBUG, ("Edited features status:"));
  for (auto const & id : m_features)
  {
    for (auto const & index : id.second)
    {
      Editor::FeatureTypeInfo const & fti = index.second;
      stats.m_edits.push_back(make_pair(FeatureID(id.first, index.first),
                                        fti.m_uploadStatus + " " + fti.m_uploadError));
      LOG(LDEBUG, (fti.m_uploadAttemptTimestamp == my::INVALID_TIME_STAMP
                   ? "NOT_UPLOADED_YET" : my::TimestampToString(fti.m_uploadAttemptTimestamp), fti.m_uploadStatus,
                   fti.m_uploadError, fti.m_feature.GetFeatureType(), feature::GetCenter(fti.m_feature)));
      if (fti.m_uploadStatus == kUploaded)
      {
        ++stats.m_uploadedCount;
        if (stats.m_lastUploadTimestamp < fti.m_uploadAttemptTimestamp)
          stats.m_lastUploadTimestamp = fti.m_uploadAttemptTimestamp;
      }
    }
  }
  return stats;
}

NewFeatureCategories Editor::GetNewFeatureCategories() const
{
  // TODO(mgsergio): Load types user can create from XML file.
  // TODO: Not every editable type can be created by user.
  CategoriesHolder const & cats = GetDefaultCategories();
  int8_t const locale = CategoriesHolder::MapLocaleToInteger(languages::GetCurrentOrig());
  Classificator const & cl = classif();
  NewFeatureCategories res;
  for (auto const & classificatorType : m_config.GetTypesThatCanBeAdded())
  {
    uint32_t const type = cl.GetTypeByReadableObjectName(classificatorType);
    if (type == 0)
    {
      LOG(LWARNING, ("Unknown type in Editor's config:", classificatorType));
      continue;
    }
    res.m_allSorted.emplace_back(type, cats.GetReadableFeatureType(type, locale));
  }
  sort(res.m_allSorted.begin(), res.m_allSorted.end(), [](Category const & c1, Category const & c2)
  {
    return c1.m_name < c2.m_name;
  });
  // TODO(mgsergio): Store in Settings:: recent history of created types and use them here.
  // Max history items count shoud be set in the config.
  uint32_t const cafe = cl.GetTypeByPath({"amenity", "cafe"});
  res.m_lastUsed.emplace_back(cafe, cats.GetReadableFeatureType(cafe, locale));
  uint32_t const restaurant = cl.GetTypeByPath({"amenity", "restaurant"});
  res.m_lastUsed.emplace_back(restaurant, cats.GetReadableFeatureType(restaurant, locale));
  uint32_t const atm = cl.GetTypeByPath({"amenity", "atm"});
  res.m_lastUsed.emplace_back(atm, cats.GetReadableFeatureType(atm, locale));

  return res;
}

FeatureID Editor::GenerateNewFeatureId(MwmSet::MwmId const & id)
{
  DECLARE_AND_ASSERT_THREAD_CHECKER("GenerateNewFeatureId is single-threaded.");
  // TODO(vng): Double-check if new feature indexes should uninterruptedly continue after existing indexes in mwm file.
  uint32_t featureIndex = kStartIndexForCreatedFeatures;
  auto const found = m_features.find(id);
  if (found != m_features.end())
  {
    // Scan all already created features and choose next available ID.
    for (auto const & feature : found->second)
    {
      if (feature.second.m_status == FeatureStatus::Created && featureIndex <= feature.first)
        featureIndex = feature.first + 1;
    }
  }
  return FeatureID(id, featureIndex);
}

bool Editor::CreatePoint(uint32_t type, m2::PointD const & mercator, MwmSet::MwmId const & id, EditableMapObject & outFeature)
{
  ASSERT(id.IsAlive(), ("Please check that feature is created in valid MWM file before calling this method."));
  outFeature.SetMercator(mercator);
  outFeature.SetID(GenerateNewFeatureId(id));
  outFeature.SetType(type);
  outFeature.SetEditableProperties(GetEditablePropertiesForTypes(outFeature.GetTypes()));
  // Only point type features can be created at the moment.
  outFeature.SetPointType();
  return true;
}

void Editor::CreateNote(m2::PointD const & point, string const & note)
{
  m_notes->CreateNote(point, note);
}

void Editor::UploadNotes(string const & key, string const & secret)
{
  alohalytics::LogEvent("Editor_UploadNotes", strings::to_string(m_notes->NotUploadedNotesCount()));
  m_notes->Upload(OsmOAuth::ServerAuth({key, secret}));
}

string DebugPrint(Editor::FeatureStatus fs)
{
  switch (fs)
  {
  case Editor::FeatureStatus::Untouched: return "Untouched";
  case Editor::FeatureStatus::Deleted: return "Deleted";
  case Editor::FeatureStatus::Modified: return "Modified";
  case Editor::FeatureStatus::Created: return "Created";
  };
}
}  // namespace osm
