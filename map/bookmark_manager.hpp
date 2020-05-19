#pragma once

#include "map/bookmark.hpp"
#include "map/bookmark_catalog.hpp"
#include "map/bookmark_helpers.hpp"
#include "map/cloud.hpp"
#include "map/elevation_info.hpp"
#include "map/track.hpp"
#include "map/track_mark.hpp"
#include "map/user_mark_layer.hpp"

#include "drape_frontend/drape_engine_safe_ptr.hpp"

#include "platform/safe_callback.hpp"

#include "geometry/any_rect2d.hpp"
#include "geometry/screenbase.hpp"

#include "base/macros.hpp"
#include "base/strings_bundle.hpp"
#include "base/thread_checker.hpp"
#include "base/visitor.hpp"

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace search
{
class RegionAddressGetter;
}  // namespace search

namespace storage
{
class CountryInfoGetter;
}  // namespace storage

class DataSource;
class SearchAPI;
class User;

class BookmarkManager final
{
  using UserMarkLayers = std::vector<std::unique_ptr<UserMarkLayer>>;
  using CategoriesCollection = std::map<kml::MarkGroupId, std::unique_ptr<BookmarkCategory>>;

  using MarksCollection = std::map<kml::MarkId, std::unique_ptr<UserMark>>;
  using BookmarksCollection = std::map<kml::MarkId, std::unique_ptr<Bookmark>>;
  using TracksCollection = std::map<kml::TrackId, std::unique_ptr<Track>>;

public:
  using KMLDataCollection = std::vector<std::pair<std::string, std::unique_ptr<kml::FileData>>>;
  using KMLDataCollectionPtr = std::shared_ptr<KMLDataCollection>;

  using BookmarksChangedCallback = std::function<void()>;
  using ElevationActivePointChangedCallback = std::function<void()>;
  using ElevationMyPositionChangedCallback = std::function<void()>;

  using OnSymbolSizesAcquiredCallback = std::function<void()>;

  using AsyncLoadingStartedCallback = std::function<void()>;
  using AsyncLoadingFinishedCallback = std::function<void()>;
  using AsyncLoadingFileCallback = std::function<void(std::string const &, bool)>;

  struct AsyncLoadingCallbacks
  {
    AsyncLoadingStartedCallback m_onStarted;
    AsyncLoadingFinishedCallback m_onFinished;
    AsyncLoadingFileCallback m_onFileError;
    AsyncLoadingFileCallback m_onFileSuccess;
  };

  struct Callbacks
  {
    using GetStringsBundleFn = std::function<StringsBundle const &()>;
    using GetSeacrhAPIFn = std::function<SearchAPI &()>;
    using CreatedBookmarksCallback = std::function<void(std::vector<BookmarkInfo> const &)>;
    using UpdatedBookmarksCallback = std::function<void(std::vector<BookmarkInfo> const &)>;
    using DeletedBookmarksCallback = std::function<void(std::vector<kml::MarkId> const &)>;
    using AttachedBookmarksCallback = std::function<void(std::vector<BookmarkGroupInfo> const &)>;
    using DetachedBookmarksCallback = std::function<void(std::vector<BookmarkGroupInfo> const &)>;

    template <typename StringsBundleProvider, typename SearchAPIProvider,
              typename CatalogHeadersProvider, typename CreateListener, typename UpdateListener,
              typename DeleteListener, typename AttachListener, typename DetachListener>
    Callbacks(StringsBundleProvider && stringsBundleProvider,
              SearchAPIProvider && searchAPIProvider, CatalogHeadersProvider && catalogHeaders,
              CreateListener && createListener, UpdateListener && updateListener,
              DeleteListener && deleteListener, AttachListener && attachListener,
              DetachListener && detachListener)
      : m_getStringsBundle(std::forward<StringsBundleProvider>(stringsBundleProvider))
      , m_getSearchAPI(std::forward<SearchAPIProvider>(searchAPIProvider))
      , m_catalogHeadersProvider(std::forward<BookmarkCatalog::HeadersProvider>(catalogHeaders))
      , m_createdBookmarksCallback(std::forward<CreateListener>(createListener))
      , m_updatedBookmarksCallback(std::forward<UpdateListener>(updateListener))
      , m_deletedBookmarksCallback(std::forward<DeleteListener>(deleteListener))
      , m_attachedBookmarksCallback(std::forward<AttachListener>(attachListener))
      , m_detachedBookmarksCallback(std::forward<DetachListener>(detachListener))
    {}

    GetStringsBundleFn m_getStringsBundle;
    GetSeacrhAPIFn m_getSearchAPI;
    BookmarkCatalog::HeadersProvider m_catalogHeadersProvider;
    CreatedBookmarksCallback m_createdBookmarksCallback;
    UpdatedBookmarksCallback m_updatedBookmarksCallback;
    DeletedBookmarksCallback m_deletedBookmarksCallback;
    AttachedBookmarksCallback m_attachedBookmarksCallback;
    DetachedBookmarksCallback m_detachedBookmarksCallback;
  };

  class EditSession
  {
  public:
    explicit EditSession(BookmarkManager & bmManager);
    ~EditSession();

    template <typename UserMarkT>
    UserMarkT * CreateUserMark(m2::PointD const & ptOrg)
    {
      return m_bmManager.CreateUserMark<UserMarkT>(ptOrg);
    }

    Bookmark * CreateBookmark(kml::BookmarkData && bmData);
    Bookmark * CreateBookmark(kml::BookmarkData && bmData, kml::MarkGroupId groupId);
    Track * CreateTrack(kml::TrackData && trackData);

    template <typename UserMarkT>
    UserMarkT * GetMarkForEdit(kml::MarkId markId)
    {
      return m_bmManager.GetMarkForEdit<UserMarkT>(markId);
    }

    Bookmark * GetBookmarkForEdit(kml::MarkId bmId);

    template <typename UserMarkT, typename F>
    void DeleteUserMarks(UserMark::Type type, F && deletePredicate)
    {
      return m_bmManager.DeleteUserMarks<UserMarkT>(type, std::move(deletePredicate));
    }

    void DeleteUserMark(kml::MarkId markId);
    void DeleteBookmark(kml::MarkId bmId);
    void DeleteTrack(kml::TrackId trackId);

    void ClearGroup(kml::MarkGroupId groupId);

    void SetIsVisible(kml::MarkGroupId groupId, bool visible);

    void MoveBookmark(kml::MarkId bmID, kml::MarkGroupId curGroupID, kml::MarkGroupId newGroupID);
    void UpdateBookmark(kml::MarkId bmId, kml::BookmarkData const & bm);

    void AttachBookmark(kml::MarkId bmId, kml::MarkGroupId groupId);
    void DetachBookmark(kml::MarkId bmId, kml::MarkGroupId groupId);

    void AttachTrack(kml::TrackId trackId, kml::MarkGroupId groupId);
    void DetachTrack(kml::TrackId trackId, kml::MarkGroupId groupId);

    void SetCategoryName(kml::MarkGroupId categoryId, std::string const & name);
    void SetCategoryDescription(kml::MarkGroupId categoryId, std::string const & desc);
    void SetCategoryTags(kml::MarkGroupId categoryId, std::vector<std::string> const & tags);
    void SetCategoryAccessRules(kml::MarkGroupId categoryId, kml::AccessRules accessRules);
    void SetCategoryCustomProperty(kml::MarkGroupId categoryId, std::string const & key,
                                   std::string const & value);
    bool DeleteBmCategory(kml::MarkGroupId groupId);

    void NotifyChanges();

  private:
    BookmarkManager & m_bmManager;
  };

  BookmarkManager(User & user, Callbacks && callbacks);

  void SetDrapeEngine(ref_ptr<df::DrapeEngine> engine);

  void InitRegionAddressGetter(DataSource const & dataSource,
                               storage::CountryInfoGetter const & infoGetter);
  void ResetRegionAddressGetter();

  void SetBookmarksChangedCallback(BookmarksChangedCallback && callback);
  void SetAsyncLoadingCallbacks(AsyncLoadingCallbacks && callbacks);
  bool IsAsyncLoadingInProgress() const { return m_asyncLoadingInProgress; }

  bool AreSymbolSizesAcquired(OnSymbolSizesAcquiredCallback && callback);

  EditSession GetEditSession();

  void UpdateViewport(ScreenBase const & screen);
  void Teardown();

  static bool IsBookmarkCategory(kml::MarkGroupId groupId) { return groupId >= UserMark::USER_MARK_TYPES_COUNT_MAX; }
  static bool IsBookmark(kml::MarkId markId) { return UserMark::GetMarkType(markId) == UserMark::BOOKMARK; }
  static UserMark::Type GetGroupType(kml::MarkGroupId groupId)
  {
    return IsBookmarkCategory(groupId) ? UserMark::BOOKMARK : static_cast<UserMark::Type>(groupId);
  }

  template <typename UserMarkT>
  UserMarkT const * GetMark(kml::MarkId markId) const
  {
    auto * mark = GetUserMark(markId);
    ASSERT(dynamic_cast<UserMarkT const *>(mark) != nullptr, ());
    return static_cast<UserMarkT const *>(mark);
  }

  UserMark const * GetUserMark(kml::MarkId markId) const;
  Bookmark const * GetBookmark(kml::MarkId markId) const;
  Track const * GetTrack(kml::TrackId trackId) const;

  kml::MarkIdSet const & GetUserMarkIds(kml::MarkGroupId groupId) const;
  kml::TrackIdSet const & GetTrackIds(kml::MarkGroupId groupId) const;

  // Do not change the order.
  enum class SortingType
  {
    ByType,
    ByDistance,
    ByTime
  };

  struct SortedBlock
  {
    bool operator==(SortedBlock const & other) const;

    std::string m_blockName;
    kml::MarkIdCollection m_markIds;
    kml::MarkIdCollection m_trackIds;
  };
  using SortedBlocksCollection = std::vector<SortedBlock>;

  struct SortParams
  {
    enum class Status
    {
      Completed,
      Cancelled
    };

    using OnResults = std::function<void(SortedBlocksCollection && sortedBlocks, Status status)>;

    kml::MarkGroupId m_groupId = kml::kInvalidMarkGroupId;
    SortingType m_sortingType = SortingType::ByType;
    bool m_hasMyPosition = false;
    m2::PointD m_myPosition = {0.0, 0.0};
    OnResults m_onResults;
  };

  std::vector<SortingType> GetAvailableSortingTypes(kml::MarkGroupId groupId,
                                                    bool hasMyPosition) const;
  void GetSortedCategory(SortParams const & params);

  bool GetLastSortingType(kml::MarkGroupId groupId, SortingType & sortingType) const;
  void SetLastSortingType(kml::MarkGroupId groupId, SortingType sortingType);
  void ResetLastSortingType(kml::MarkGroupId groupId);

  bool IsSearchAllowed(kml::MarkGroupId groupId) const;
  void PrepareForSearch(kml::MarkGroupId groupId);

  bool IsVisible(kml::MarkGroupId groupId) const;

  kml::MarkGroupId CreateBookmarkCategory(kml::CategoryData && data, bool autoSave = true);
  kml::MarkGroupId CreateBookmarkCategory(std::string const & name, bool autoSave = true);

  std::string GetCategoryName(kml::MarkGroupId categoryId) const;
  std::string GetCategoryFileName(kml::MarkGroupId categoryId) const;
  m2::RectD GetCategoryRect(kml::MarkGroupId categoryId, bool addIconsSize) const;
  kml::CategoryData const & GetCategoryData(kml::MarkGroupId categoryId) const;

  kml::MarkGroupId GetCategoryId(std::string const & name) const;

  kml::GroupIdCollection const & GetBmGroupsIdList() const { return m_bmGroupsIdList; }
  bool HasBmCategory(kml::MarkGroupId groupId) const;
  kml::MarkGroupId LastEditedBMCategory();
  kml::PredefinedColor LastEditedBMColor() const;

  void SetLastEditedBmCategory(kml::MarkGroupId groupId);
  void SetLastEditedBmColor(kml::PredefinedColor color);

  using TTouchRectHolder = std::function<m2::AnyRectD(UserMark::Type)>;
  using TFindOnlyVisibleChecker = std::function<bool(UserMark::Type)>;
  UserMark const * FindNearestUserMark(TTouchRectHolder const & holder,
                                       TFindOnlyVisibleChecker const & findOnlyVisible) const;
  UserMark const * FindNearestUserMark(m2::AnyRectD const & rect) const;
  UserMark const * FindMarkInRect(kml::MarkGroupId groupId, m2::AnyRectD const & rect, bool findOnlyVisible,
                                  double & d) const;

  /// Scans and loads all kml files with bookmarks.
  void LoadBookmarks();
  void LoadBookmark(std::string const & filePath, bool isTemporaryFile);

  /// Uses the same file name from which was loaded, or
  /// creates unique file name on first save and uses it every time.
  void SaveBookmarks(kml::GroupIdCollection const & groupIdCollection);

  StaticMarkPoint & SelectionMark() { return *m_selectionMark; }
  StaticMarkPoint const & SelectionMark() const { return *m_selectionMark; }

  MyPositionMarkPoint & MyPositionMark() { return *m_myPositionMark; }
  MyPositionMarkPoint const & MyPositionMark() const { return *m_myPositionMark; }

  void SetCloudEnabled(bool enabled);
  bool IsCloudEnabled() const;
  uint64_t GetLastSynchronizationTimestampInMs() const;
  std::unique_ptr<User::Subscriber> GetUserSubscriber();

  enum class CategoryFilterType
  {
    Private = 0,
    Public,
    All
  };

  struct SharingResult
  {
    enum class Code
    {
      Success = 0,
      EmptyCategory,
      ArchiveError,
      FileError
    };

    SharingResult(kml::MarkGroupId categoryId, std::string const & sharingPath)
      : m_categoryId(categoryId)
      , m_code(Code::Success)
      , m_sharingPath(sharingPath)
    {}

    SharingResult(kml::MarkGroupId categoryId, Code code)
      : m_categoryId(categoryId)
      , m_code(code)
    {}

    SharingResult(kml::MarkGroupId categoryId, Code code, std::string const & errorString)
      : m_categoryId(categoryId)
      , m_code(code)
      , m_errorString(errorString)
    {}

    kml::MarkGroupId m_categoryId;
    Code m_code;
    std::string m_sharingPath;
    std::string m_errorString;
  };

  using SharingHandler = platform::SafeCallback<void(SharingResult const & result)>;
  void PrepareFileForSharing(kml::MarkGroupId categoryId, SharingHandler && handler);

  bool IsCategoryEmpty(kml::MarkGroupId categoryId) const;

  bool IsEditableBookmark(kml::MarkId bmId) const;
  bool IsEditableTrack(kml::TrackId trackId) const;
  bool IsEditableCategory(kml::MarkGroupId groupId) const;

  bool IsUsedCategoryName(std::string const & name) const;
  bool AreAllCategoriesVisible(CategoryFilterType const filter) const;
  bool AreAllCategoriesInvisible(CategoryFilterType const filter) const;
  void SetAllCategoriesVisibility(CategoryFilterType const filter, bool visible);

  // Return number of files for the conversion to the binary format.
  size_t GetKmlFilesCountForConversion() const;

  // Convert all found kml files to the binary format.
  using ConversionHandler = platform::SafeCallback<void(bool success)>;
  void ConvertAllKmlFiles(ConversionHandler && handler);

  // These handlers are always called from UI-thread.
  void SetCloudHandlers(Cloud::SynchronizationStartedHandler && onSynchronizationStarted,
                        Cloud::SynchronizationFinishedHandler && onSynchronizationFinished,
                        Cloud::RestoreRequestedHandler && onRestoreRequested,
                        Cloud::RestoredFilesPreparedHandler && onRestoredFilesPrepared);

  void RequestCloudRestoring();
  void ApplyCloudRestoring();
  void CancelCloudRestoring();

  void SetNotificationsEnabled(bool enabled);
  bool AreNotificationsEnabled() const;

  using OnCatalogDownloadStartedHandler = platform::SafeCallback<void(std::string const & id)>;
  using OnCatalogDownloadFinishedHandler = platform::SafeCallback<void(std::string const & id,
                                                                       BookmarkCatalog::DownloadResult result)>;
  using OnCatalogImportStartedHandler = platform::SafeCallback<void(std::string const & id)>;
  using OnCatalogImportFinishedHandler = platform::SafeCallback<void(std::string const & id,
                                                                     kml::MarkGroupId categoryId,
                                                                     bool successful)>;
  using OnCatalogUploadStartedHandler = std::function<void(kml::MarkGroupId originCategoryId)>;
  using OnCatalogUploadFinishedHandler = std::function<void(BookmarkCatalog::UploadResult uploadResult,
                                                            std::string const & description,
                                                            kml::MarkGroupId originCategoryId,
                                                            kml::MarkGroupId resultCategoryId)>;

  void SetCatalogHandlers(OnCatalogDownloadStartedHandler && onCatalogDownloadStarted,
                          OnCatalogDownloadFinishedHandler && onCatalogDownloadFinished,
                          OnCatalogImportStartedHandler && onCatalogImportStarted,
                          OnCatalogImportFinishedHandler && onCatalogImportFinished,
                          OnCatalogUploadStartedHandler && onCatalogUploadStartedHandler,
                          OnCatalogUploadFinishedHandler && onCatalogUploadFinishedHandler);
  void DownloadFromCatalogAndImport(std::string const & id, std::string const & name);
  void ImportDownloadedFromCatalog(std::string const & id, std::string const & filePath);
  void UploadToCatalog(kml::MarkGroupId categoryId, kml::AccessRules accessRules);
  bool IsCategoryFromCatalog(kml::MarkGroupId categoryId) const;
  kml::MarkGroupId GetCategoryIdByServerId(std::string const & serverId) const;
  std::string GetCategoryServerId(kml::MarkGroupId categoryId) const;
  std::string GetCategoryCatalogDeeplink(kml::MarkGroupId categoryId) const;
  std::string GetCategoryCatalogPublicLink(kml::MarkGroupId categoryId) const;
  BookmarkCatalog const & GetCatalog() const;

  bool IsMyCategory(kml::MarkGroupId categoryId) const;

  // CheckInvalidCategories checks invalid categories asynchronously, it prepares a state for following
  // functions calls.
  using CheckInvalidCategoriesHandler = std::function<void(bool hasInvalidCategories)>;
  void CheckInvalidCategories(CheckInvalidCategoriesHandler && handler);

  // The following 2 functions allow to delete invalid categories or forget about them.
  // These functions are stateful, so they must be called after finishing CheckInvalidCategoriesHandler.
  // ResetInvalidCategories resets internal state.
  void DeleteInvalidCategories();
  void ResetInvalidCategories();

  void FilterInvalidBookmarks(kml::MarkIdCollection & bookmarks) const;
  void FilterInvalidTracks(kml::TrackIdCollection & tracks) const;

  /// These functions are public for unit tests only. You shouldn't call them from client code.
  void EnableTestMode(bool enable);
  bool SaveBookmarkCategory(kml::MarkGroupId groupId);
  bool SaveBookmarkCategory(kml::MarkGroupId groupId, Writer & writer, KmlFileType fileType) const;
  void CreateCategories(KMLDataCollection && dataCollection, bool autoSave = true);
  static std::string RemoveInvalidSymbols(std::string const & name, std::string const & defaultName);
  static std::string GenerateUniqueFileName(std::string const & path, std::string name, std::string const & fileExt);
  static std::string GenerateValidAndUniqueFilePathForKML(std::string const & fileName);
  static std::string GenerateValidAndUniqueFilePathForKMB(std::string const & fileName);
  static std::string GetActualBookmarksDirectory();
  static bool IsMigrated();
  static std::string GetTracksSortedBlockName();
  static std::string GetOthersSortedBlockName();
  static std::string GetNearMeSortedBlockName();
  enum class SortedByTimeBlockType : uint32_t
  {
    WeekAgo,
    MonthAgo,
    MoreThanMonthAgo,
    MoreThanYearAgo,
    Others
  };
  static std::string GetSortedByTimeBlockName(SortedByTimeBlockType blockType);
  std::string GetLocalizedRegionAddress(m2::PointD const & pt);

  using AccessRulesFilter = std::function<bool(kml::AccessRules)>;
  std::vector<std::string> GetCategoriesFromCatalog(AccessRulesFilter && filter) const;
  static bool IsGuide(kml::AccessRules accessRules);

  ElevationInfo MakeElevationInfo(kml::TrackId trackId) const;

  void SetElevationActivePoint(kml::TrackId const & trackId, double distanceInMeters);
  // Returns distance from the start of the track to active point in meters.
  double GetElevationActivePoint(kml::TrackId const & trackId) const;

  void UpdateElevationMyPosition(kml::TrackId const & trackId);
  // Returns distance from the start of the track to my position in meters.
  // Returns negative value if my position is not on the track.
  double GetElevationMyPosition(kml::TrackId const & trackId) const;

  void SetElevationActivePointChangedCallback(ElevationActivePointChangedCallback const & cb);
  void SetElevationMyPositionChangedCallback(ElevationMyPositionChangedCallback const & cb);

  struct TrackSelectionInfo
  {
    TrackSelectionInfo() = default;
    TrackSelectionInfo(kml::TrackId trackId, m2::PointD const & trackPoint, double distanceInMeters)
      : m_trackId(trackId)
      , m_trackPoint(trackPoint)
      , m_distanceInMeters(distanceInMeters)
    {}

    kml::TrackId m_trackId = kml::kInvalidTrackId;
    m2::PointD m_trackPoint = m2::PointD::Zero();
    double m_distanceInMeters = 0.0;
  };

  using TracksFilter = std::function<bool(Track const * track)>;
  TrackSelectionInfo FindNearestTrack(m2::RectD const & touchRect,
                                      TracksFilter const & tracksFilter = nullptr) const;
  TrackSelectionInfo GetTrackSelectionInfo(kml::TrackId const & trackId) const;

  void SetTrackSelectionInfo(TrackSelectionInfo const & trackSelectionInfo, bool notifyListeners);
  void SetDefaultTrackSelection(kml::TrackId trackId, bool showInfoSign);
  void OnTrackSelected(kml::TrackId trackId);
  void OnTrackDeselected();

private:
  class MarksChangesTracker : public df::UserMarksProvider
  {
  public:
    explicit MarksChangesTracker(BookmarkManager * bmManager)
      : m_bmManager(bmManager)
    {
      CHECK(m_bmManager != nullptr, ());
    }

    void OnAddMark(kml::MarkId markId);
    void OnDeleteMark(kml::MarkId markId);
    void OnUpdateMark(kml::MarkId markId);

    void OnAddLine(kml::TrackId lineId);
    void OnDeleteLine(kml::TrackId lineId);

    void OnAddGroup(kml::MarkGroupId groupId);
    void OnDeleteGroup(kml::MarkGroupId groupId);

    void OnAttachBookmark(kml::MarkId markId, kml::MarkGroupId catId);
    void OnDetachBookmark(kml::MarkId markId, kml::MarkGroupId catId);

    void AcceptDirtyItems();
    bool HasChanges() const;
    bool HasBookmarksChanges() const;
    void ResetChanges();
    void AddChanges(MarksChangesTracker const & changes);

    using GroupMarkIdSet = std::map<kml::MarkGroupId, kml::MarkIdSet>;
    GroupMarkIdSet const & GetAttachedBookmarks() const { return m_attachedBookmarks; }
    GroupMarkIdSet const & GetDetachedBookmarks() const { return m_detachedBookmarks; }

    // UserMarksProvider
    kml::GroupIdSet GetAllGroupIds() const override;
    kml::GroupIdSet const & GetUpdatedGroupIds() const override { return m_updatedGroups; }
    kml::GroupIdSet const & GetRemovedGroupIds() const override { return m_removedGroups; }
    kml::MarkIdSet const & GetCreatedMarkIds() const override { return m_createdMarks; }
    kml::MarkIdSet const & GetRemovedMarkIds() const override { return m_removedMarks; }
    kml::MarkIdSet const & GetUpdatedMarkIds() const override { return m_updatedMarks; }
    kml::TrackIdSet const & GetCreatedLineIds() const override { return m_createdLines; }
    kml::TrackIdSet const & GetRemovedLineIds() const override { return m_removedLines; }
    kml::GroupIdSet const & GetBecameVisibleGroupIds() const override { return m_becameVisibleGroups; }
    kml::GroupIdSet const & GetBecameInvisibleGroupIds() const override { return m_becameInvisibleGroups; }
    bool IsGroupVisible(kml::MarkGroupId groupId) const override;
    kml::MarkIdSet const & GetGroupPointIds(kml::MarkGroupId groupId) const override;
    kml::TrackIdSet const & GetGroupLineIds(kml::MarkGroupId groupId) const override;
    df::UserPointMark const * GetUserPointMark(kml::MarkId markId) const override;
    df::UserLineMark const * GetUserLineMark(kml::TrackId lineId) const override;

  private:
    void OnUpdateGroup(kml::MarkGroupId groupId);
    void OnBecomeVisibleGroup(kml::MarkGroupId groupId);
    void OnBecomeInvisibleGroup(kml::MarkGroupId groupId);

    void InsertBookmark(kml::MarkId markId, kml::MarkGroupId catId,
                        GroupMarkIdSet & setToInsert, GroupMarkIdSet & setToErase);

    BookmarkManager * m_bmManager;

    kml::MarkIdSet m_createdMarks;
    kml::MarkIdSet m_removedMarks;
    kml::MarkIdSet m_updatedMarks;

    kml::TrackIdSet m_createdLines;
    kml::TrackIdSet m_removedLines;

    kml::GroupIdSet m_createdGroups;
    kml::GroupIdSet m_removedGroups;

    kml::GroupIdSet m_updatedGroups;
    kml::GroupIdSet m_becameVisibleGroups;
    kml::GroupIdSet m_becameInvisibleGroups;

    GroupMarkIdSet m_attachedBookmarks;
    GroupMarkIdSet m_detachedBookmarks;
  };

  template <typename UserMarkT>
  UserMarkT * CreateUserMark(m2::PointD const & ptOrg)
  {
    CHECK_THREAD_CHECKER(m_threadChecker, ());
    auto mark = std::make_unique<UserMarkT>(ptOrg);
    auto * m = mark.get();
    auto const markId = m->GetId();
    auto const groupId = static_cast<kml::MarkGroupId>(m->GetMarkType());
    CHECK_EQUAL(m_userMarks.count(markId), 0, ());
    ASSERT_GREATER(groupId, 0, ());
    ASSERT_LESS(groupId - 1, m_userMarkLayers.size(), ());
    m_userMarks.emplace(markId, std::move(mark));
    m_changesTracker.OnAddMark(markId);
    m_userMarkLayers[static_cast<size_t>(groupId - 1)]->AttachUserMark(markId);
    return m;
  }

  template <typename UserMarkT>
  UserMarkT * GetMarkForEdit(kml::MarkId markId)
  {
    CHECK_THREAD_CHECKER(m_threadChecker, ());
    auto * mark = GetUserMarkForEdit(markId);
    ASSERT(dynamic_cast<UserMarkT *>(mark) != nullptr, ());
    return static_cast<UserMarkT *>(mark);
  }

  template <typename UserMarkT, typename F>
  void DeleteUserMarks(UserMark::Type type, F && deletePredicate)
  {
    CHECK_THREAD_CHECKER(m_threadChecker, ());
    std::list<kml::MarkId> marksToDelete;
    for (auto markId : GetUserMarkIds(type))
    {
      if (deletePredicate(GetMark<UserMarkT>(markId)))
        marksToDelete.push_back(markId);
    }
    // Delete after iterating to avoid iterators invalidation issues.
    for (auto markId : marksToDelete)
      DeleteUserMark(markId);
  }

  UserMark * GetUserMarkForEdit(kml::MarkId markId);
  void DeleteUserMark(kml::MarkId markId);

  Bookmark * CreateBookmark(kml::BookmarkData && bmData);
  Bookmark * CreateBookmark(kml::BookmarkData && bmData, kml::MarkGroupId groupId);

  Bookmark * GetBookmarkForEdit(kml::MarkId markId);
  void AttachBookmark(kml::MarkId bmId, kml::MarkGroupId groupId);
  void DetachBookmark(kml::MarkId bmId, kml::MarkGroupId groupId);
  void DeleteBookmark(kml::MarkId bmId);

  Track * CreateTrack(kml::TrackData && trackData);

  void AttachTrack(kml::TrackId trackId, kml::MarkGroupId groupId);
  void DetachTrack(kml::TrackId trackId, kml::MarkGroupId groupId);
  void DeleteTrack(kml::TrackId trackId);

  void ClearGroup(kml::MarkGroupId groupId);
  void SetIsVisible(kml::MarkGroupId groupId, bool visible);

  void SetCategoryName(kml::MarkGroupId categoryId, std::string const & name);
  void SetCategoryDescription(kml::MarkGroupId categoryId, std::string const & desc);
  void SetCategoryTags(kml::MarkGroupId categoryId, std::vector<std::string> const & tags);
  void SetCategoryAccessRules(kml::MarkGroupId categoryId, kml::AccessRules accessRules);
  void SetCategoryCustomProperty(kml::MarkGroupId categoryId, std::string const & key,
                                 std::string const & value);
  bool DeleteBmCategory(kml::MarkGroupId groupId);
  void ClearCategories();

  void MoveBookmark(kml::MarkId bmID, kml::MarkGroupId curGroupID, kml::MarkGroupId newGroupID);
  void UpdateBookmark(kml::MarkId bmId, kml::BookmarkData const & bm);

  UserMark const * GetMark(kml::MarkId markId) const;

  UserMarkLayer const * GetGroup(kml::MarkGroupId groupId) const;
  UserMarkLayer * GetGroup(kml::MarkGroupId groupId);
  BookmarkCategory const * GetBmCategory(kml::MarkGroupId categoryId) const;
  BookmarkCategory * GetBmCategory(kml::MarkGroupId categoryId);

  Bookmark * AddBookmark(std::unique_ptr<Bookmark> && bookmark);
  Track * AddTrack(std::unique_ptr<Track> && track);

  void OnEditSessionOpened();
  void OnEditSessionClosed();
  void NotifyChanges();

  void SaveState() const;
  void LoadState();

  void SaveMetadata();
  void LoadMetadata();
  void CleanupInvalidMetadata();
  std::string GetMetadataEntryName(kml::MarkGroupId groupId) const;

  void NotifyAboutStartAsyncLoading();
  void NotifyAboutFinishAsyncLoading(KMLDataCollectionPtr && collection);
  std::optional<std::string> GetKMLPath(std::string const & filePath);
  void NotifyAboutFile(bool success, std::string const & filePath, bool isTemporaryFile);
  void LoadBookmarkRoutine(std::string const & filePath, bool isTemporaryFile);
  
  using BookmarksChecker = std::function<bool(kml::FileData const &)>;
  KMLDataCollectionPtr LoadBookmarks(std::string const & dir, std::string const & ext,
                                     KmlFileType fileType, BookmarksChecker const & checker,
                                     std::vector<std::string> & cloudFilePaths);

  void GetDirtyGroups(kml::GroupIdSet & dirtyGroups) const;
  void UpdateBmGroupIdList();

  void NotifyBookmarksChanged();

  void SendBookmarksChanges(MarksChangesTracker const & changesTracker);
  void GetBookmarksInfo(kml::MarkIdSet const & marks, std::vector<BookmarkInfo> & bookmarks);
  void GetBookmarkGroupsInfo(MarksChangesTracker::GroupMarkIdSet const & groups,
                             std::vector<BookmarkGroupInfo> & groupsInfo);

  kml::MarkGroupId CheckAndCreateDefaultCategory();
  void CheckAndResetLastIds();

  std::unique_ptr<kml::FileData> CollectBmGroupKMLData(BookmarkCategory const * group) const;
  KMLDataCollectionPtr PrepareToSaveBookmarks(kml::GroupIdCollection const & groupIdCollection);
  bool SaveKmlFileByExt(kml::FileData & kmlData, std::string const & file);

  void OnSynchronizationStarted(Cloud::SynchronizationType type);
  void OnSynchronizationFinished(Cloud::SynchronizationType type, Cloud::SynchronizationResult result,
                                 std::string const & errorStr);
  void OnRestoreRequested(Cloud::RestoringRequestResult result, std::string const & deviceName,
                          uint64_t backupTimestampInMs);
  void OnRestoredFilesPrepared();

  bool CanConvert() const;
  void FinishConversion(ConversionHandler const & handler, bool result);

  bool HasDuplicatedIds(kml::FileData const & fileData) const;
  bool CheckVisibility(CategoryFilterType const filter, bool isVisible) const;

  struct SortBookmarkData
  {
    SortBookmarkData() = default;
    SortBookmarkData(kml::BookmarkData const & bmData,
                     search::ReverseGeocoder::RegionAddress const & address)
      : m_id(bmData.m_id)
      , m_point(bmData.m_point)
      , m_type(GetBookmarkBaseType(bmData.m_featureTypes))
      , m_timestamp(bmData.m_timestamp)
      , m_address(address)
    {}

    kml::MarkId m_id;
    m2::PointD m_point;
    BookmarkBaseType m_type;
    kml::Timestamp m_timestamp;
    search::ReverseGeocoder::RegionAddress m_address;
  };

  struct SortTrackData
  {
    SortTrackData() = default;
    SortTrackData(kml::TrackData const & trackData)
      : m_id(trackData.m_id)
      , m_timestamp(trackData.m_timestamp)
    {}

    kml::TrackId m_id;
    kml::Timestamp m_timestamp;
  };

  void GetSortedCategoryImpl(SortParams const & params,
                             std::vector<SortBookmarkData> const & bookmarksForSort,
                             std::vector<SortTrackData> const & tracksForSort,
                             SortedBlocksCollection & sortedBlocks);

  void SortByDistance(std::vector<SortBookmarkData> const & bookmarksForSort,
                      std::vector<SortTrackData> const & tracksForSort,
                      m2::PointD const & myPosition, SortedBlocksCollection & sortedBlocks);
  void SortByTime(std::vector<SortBookmarkData> const & bookmarksForSort,
                  std::vector<SortTrackData> const & tracksForSort,
                  SortedBlocksCollection & sortedBlocks) const;
  void SortByType(std::vector<SortBookmarkData> const & bookmarksForSort,
                  std::vector<SortTrackData> const & tracksForSort,
                  SortedBlocksCollection & sortedBlocks) const;

  using AddressesCollection = std::vector<std::pair<kml::MarkId, search::ReverseGeocoder::RegionAddress>>;
  void PrepareBookmarksAddresses(std::vector<SortBookmarkData> & bookmarksForSort, AddressesCollection & newAddresses);
  void FilterInvalidData(SortedBlocksCollection & sortedBlocks, AddressesCollection & newAddresses) const;
  void SetBookmarksAddresses(AddressesCollection const & addresses);
  void AddTracksSortedBlock(std::vector<SortTrackData> const & sortedTracks,
                            SortedBlocksCollection & sortedBlocks) const;
  void SortTracksByTime(std::vector<SortTrackData> & tracks) const;

  std::vector<std::string> GetAllPaidCategoriesIds() const;

  kml::MarkId GetTrackSelectionMarkId(kml::TrackId trackId) const;
  int GetTrackSelectionMarkMinZoom(kml::TrackId trackId) const;
  void SetTrackSelectionMark(kml::TrackId trackId, m2::PointD const & pt, double distance);
  void DeleteTrackSelectionMark(kml::TrackId trackId);
  void SetTrackInfoMark(kml::TrackId trackId, m2::PointD const & pt);
  void ResetTrackInfoMark(kml::TrackId trackId);

  void UpdateTrackMarksMinZoom();
  void UpdateTrackMarksVisibility(kml::MarkGroupId groupId);
  void RequestSymbolSizes();

  ThreadChecker m_threadChecker;

  User & m_user;
  Callbacks m_callbacks;
  MarksChangesTracker m_changesTracker;
  MarksChangesTracker m_bookmarksChangesTracker;
  MarksChangesTracker m_drapeChangesTracker;
  df::DrapeEngineSafePtr m_drapeEngine;

  std::unique_ptr<search::RegionAddressGetter> m_regionAddressGetter;
  std::mutex m_regionAddressMutex;

  BookmarksChangedCallback m_categoriesChangedCallback;
  ElevationActivePointChangedCallback m_elevationActivePointChanged;
  ElevationMyPositionChangedCallback m_elevationMyPositionChanged;
  m2::PointD m_lastElevationMyPosition = m2::PointD::Zero();

  OnSymbolSizesAcquiredCallback m_onSymbolSizesAcquiredFn;
  bool m_symbolSizesAcquired = false;

  AsyncLoadingCallbacks m_asyncLoadingCallbacks;
  std::atomic<bool> m_needTeardown;
  size_t m_openedEditSessionsCount = 0;
  bool m_loadBookmarksFinished = false;
  bool m_firstDrapeNotification = false;
  bool m_restoreApplying = false;
  bool m_migrationInProgress = false;
  bool m_conversionInProgress = false;
  bool m_notificationsEnabled = true;

  ScreenBase m_viewport;

  CategoriesCollection m_categories;
  kml::GroupIdCollection m_bmGroupsIdList;

  std::string m_lastCategoryUrl;
  kml::MarkGroupId m_lastEditedGroupId = kml::kInvalidMarkGroupId;
  kml::PredefinedColor m_lastColor = kml::PredefinedColor::Red;
  UserMarkLayers m_userMarkLayers;

  MarksCollection m_userMarks;
  BookmarksCollection m_bookmarks;
  TracksCollection m_tracks;

  StaticMarkPoint * m_selectionMark = nullptr;
  MyPositionMarkPoint * m_myPositionMark = nullptr;

  kml::MarkId m_trackInfoMarkId = kml::kInvalidMarkId;
  kml::TrackId m_selectedTrackId = kml::kInvalidTrackId;
  m2::PointF m_maxBookmarkSymbolSize;

  bool m_asyncLoadingInProgress = false;
  struct BookmarkLoaderInfo
  {
    BookmarkLoaderInfo() = default;
    BookmarkLoaderInfo(std::string const & filename, bool isTemporaryFile)
      : m_filename(filename), m_isTemporaryFile(isTemporaryFile)
    {}

    std::string m_filename;
    bool m_isTemporaryFile = false;
  };
  std::list<BookmarkLoaderInfo> m_bookmarkLoadingQueue;

  Cloud m_bookmarkCloud;
  Cloud::SynchronizationStartedHandler m_onSynchronizationStarted;
  Cloud::SynchronizationFinishedHandler m_onSynchronizationFinished;
  Cloud::RestoreRequestedHandler m_onRestoreRequested;
  Cloud::RestoredFilesPreparedHandler m_onRestoredFilesPrepared;

  BookmarkCatalog m_bookmarkCatalog;
  OnCatalogDownloadStartedHandler m_onCatalogDownloadStarted;
  OnCatalogDownloadFinishedHandler m_onCatalogDownloadFinished;
  OnCatalogImportStartedHandler m_onCatalogImportStarted;
  OnCatalogImportFinishedHandler m_onCatalogImportFinished;
  OnCatalogUploadStartedHandler m_onCatalogUploadStartedHandler;
  OnCatalogUploadFinishedHandler m_onCatalogUploadFinishedHandler;

  struct RestoringCache
  {
    std::string m_serverId;
    kml::AccessRules m_accessRules;
  };
  std::map<std::string, RestoringCache> m_restoringCache;

  std::vector<kml::MarkGroupId> m_invalidCategories;


  struct Properties
  {
    DECLARE_VISITOR_AND_DEBUG_PRINT(Properties, visitor(m_values, "values"))

    bool GetProperty(std::string const & propertyName, std::string & value) const;

    std::map<std::string, std::string> m_values;
  };

  struct Metadata
  {
    DECLARE_VISITOR_AND_DEBUG_PRINT(Metadata, visitor(m_entriesProperties, "entriesProperties"),
                                    visitor(m_commonProperties, "commonProperties"))

    bool GetEntryProperty(std::string const & entryName, std::string const & propertyName,
                          std::string & value) const;

    std::map<std::string, Properties> m_entriesProperties;
    Properties m_commonProperties;
  };

  Metadata m_metadata;

  bool m_testModeEnabled = false;

  DISALLOW_COPY_AND_MOVE(BookmarkManager);
};

namespace lightweight
{
namespace impl
{
bool IsBookmarksCloudEnabled();
}  // namespace impl
}  //namespace lightweight
