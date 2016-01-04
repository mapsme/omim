#pragma once

#include "storage/country.hpp"
#include "storage/index.hpp"
#include "storage/map_files_downloader.hpp"
#include "storage/queued_country.hpp"
#include "storage/storage_defines.hpp"

#include "search/params.hpp"

#include "platform/local_country_file.hpp"

#include "std/function.hpp"
#include "std/list.hpp"
#include "std/set.hpp"
#include "std/shared_ptr.hpp"
#include "std/string.hpp"
#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

namespace storage
{
/// This class is used for downloading, updating and deleting maps.
class Storage
{
public:
  struct StatusCallback;
  using TUpdate = function<void(platform::LocalCountryFile const &)>;

private:
  /// We support only one simultaneous request at the moment
  unique_ptr<MapFilesDownloader> m_downloader;

  /// stores timestamp for update checks
  int64_t m_currentVersion;

  CountriesContainerT m_countries;

  typedef list<QueuedCountry> TQueue;

  /// @todo. It appeared that our application uses m_queue from
  /// different threads without any synchronization. To reproduce it
  /// just download a map "from the map" on Android. (CountryStatus is
  /// called from a different thread.)  It's necessary to check if we
  /// can call all the methods from a single thread using
  /// RunOnUIThread.  If not, at least use a syncronization object.
  TQueue m_queue;

  /// stores countries whose download has failed recently
  typedef set<TIndex> TCountriesSet;
  TCountriesSet m_failedCountries;

  using TLocalFilePtr = shared_ptr<platform::LocalCountryFile>;
  map<TIndex, list<TLocalFilePtr>> m_localFiles;

  // Our World.mwm and WorldCoasts.mwm are fake countries, together with any custom mwm in data
  // folder.
  map<platform::CountryFile, TLocalFilePtr> m_localFilesForFakeCountries;

  /// used to correctly calculate total country download progress with more than 1 file
  /// <current, total>
  MapFilesDownloader::TProgress m_countryProgress;

  /// @name Communicate with GUI
  //@{
  typedef function<void(TIndex const &)> TChangeCountryFunction;
  typedef function<void(TIndex const &, LocalAndRemoteSizeT const &)> TProgressFunction;

  int m_currentSlotId;

  list<StatusCallback> m_statusCallbacks;

  struct CountryObservers
  {
    TChangeCountryFunction m_changeCountryFn;
    TProgressFunction m_progressFn;
    int m_slotId;
  };

  typedef list<CountryObservers> ObserversContT;
  ObserversContT m_observers;
  //@}

  // This function is called each time all files requested for a
  // country were successfully downloaded.
  TUpdate m_update;

  void DownloadNextCountryFromQueue();

  void LoadCountriesFile(bool forceReload);

  void ReportProgress(TIndex const & index, pair<int64_t, int64_t> const & p);

  /// Called on the main thread by MapFilesDownloader when list of
  /// suitable servers is received.
  void OnServerListDownloaded(vector<string> const & urls);

  /// Called on the main thread by MapFilesDownloader when
  /// downloading of a map file succeeds/fails.
  void OnMapFileDownloadFinished(bool success, MapFilesDownloader::TProgress const & progress);

  /// Periodically called on the main thread by MapFilesDownloader
  /// during the downloading process.
  void OnMapFileDownloadProgress(MapFilesDownloader::TProgress const & progress);

  bool RegisterDownloadedFiles(TIndex const & index, MapOptions files);
  void OnMapDownloadFinished(TIndex const & index, bool success, MapOptions files);

  /// Initiates downloading of the next file from the queue.
  void DownloadNextFile(QueuedCountry const & country);

public:
  Storage();

  /// @name Interface with clients (Android/iOS).
  /// \brief It represents the interface which can be used by clients (Android/iOS).
  /// The term node means an mwm or a group of mwm like a big country.
  /// The term node id means a string id of mwm or a group of mwm. The sting contains
  /// a name of file with mwm of a name country(territory).
  //@{
  enum class ErrorCode;
  using TOnSearchResultCallback = function<void (TIndex const &)>;
  using TOnStatusChangedCallback = function<void (TIndex const &)>;
  using TOnErrorCallback = function<void (TIndex const &, ErrorCode)>;

  /// \brief This enum describes status of a downloaded mwm or a group of downloaded mwm.
  enum class ClientNodeStatus
  {
    UpToDate,              /**< Downloaded mwm(s) is up to date. No need update it. */
    DownloadingInProcess,  /**< Downloading a new mwm or updating a old one. */
    DownloadWasPaused,     /**< Downloading was paused or stopped by some reasons. E.g lost connection. */
    NeedsToUpdate,         /**< An update for a downloaded mwm is ready according to county_attributes.txt. */
    InQueue,               /**< A mwm is waiting for downloading in the queue. */
  };

  /// \brief Contains all properties for a node on the server.
  struct ServerNodeAttrs
  {
    /// If it's not an extandable m_nodeSize is size of one mwm.
    /// Otherwise m_nodeSize is a sum of all mwm sizes which belong to the group.
    size_t m_nodeSize;
    /// If the node is expandalbe (a big country) m_childrenCounter is number of children of this node.
    /// If the node isn't expandable m_childrenCounter == -1.
    int m_childrenCounter;
    /// parentId is a node id of parent of the node.
    /// If the node is "world" (that means the root) parentId == "".
    TIndex parentId;
  };

  /// \brief Contains all properties for a downloaded mwm.
  /// It's applicable for expandable and not expandable node id.
  struct ClientNodeAttrs
  {
    /// If it's not an extandable node m_nodeSize is size of one mwm.
    /// Otherwise m_nodeSize is a sum of all mwm sizes which belong to the group and
    /// have been downloaded.
    size_t m_nodeSize;
    /// If the node is expandable (a big country) m_mapsDownloaded is number of maps has been downloaded.
    /// If the node isn't expandable m_mapsDownloaded == -1.
    int m_mapsDownloaded;
    /// \brief It's an mwm version which was taken for mwm header.
    /// @TODO Discuss a version format. It should represent date and time (one second precision).
    /// It should be converted easily to unix time.
    /// \note It's set to zero in it's attributes of expandable node.
    size_t m_mwmVersion;
    /// A number for 0 to 100. It reflects downloading progress in case of
    /// downloading and updating mwm.
    uint8_t m_downloadingProgress;
    ClientNodeStatus m_status;
  };

  /// \brief Error code of MapRepository.
  enum class ErrorCode
  {
    NoError,                /**< An operation was finished without errors. */
    NotEnoughSpace,         /**< No space on flash memory to download a file. */
    NoInternetConnection,   /**< No internet connection. */
  };

  /// \brief Information for "Update all mwms" button.
  struct UpdateInfo
  {
    size_t m_numberOfMwmFilesToUpdate;
    size_t m_totalUpdateSizeInBytes;
  };

  struct StatusCallback
  {
    /// \brief m_onStatusChanged is called by MapRepository when status of
    /// a node is changed. If this method is called for an mwm it'll be called for
    /// every its parent and grandparents.
    /// \param nodeId is id of mwm or an mwm group which status has been changed.
    TOnStatusChangedCallback m_onStatusChanged;
    /// \brief m_onError is called when an error happend while async operation.
    /// \note A client should be ready for any value of error.
    TOnErrorCallback m_onError;
  };

  /// \brief Returns root node id of the county tree.
  TIndex const GetRootId() const;
  /// \brief Returns children node ids by a parent. For example GetChildren(GetRootId())
  /// returns all counties ids. It's content of map downloader list by default.
  vector<TIndex> const GetChildren(TIndex const & parent) const;
  /// \brief Returns children node ids by a parent for downloaded mwms.
  /// The result of the method is composed in a special way because of design requirements.
  /// If a direct child (of parent) contains two or more downloaded mwms the direct child id will be added to result.
  /// If a direct child (of parent) contains one downloaded mwm the mwm id will be added to result.
  /// If there's no downloaded mwms contained by a direct child the direct child id will not be added to result.
  vector<TIndex> const GetDownloadedChildren(TIndex const & parent) const;

  /// \brief Search for node ids (mwms not groups of mwms) by position in mercator.
  /// Find all mwms which are close to position.
  /// For getting result callback SearchResultCallback should be implemented.
  /// \note For the time being it returns node id of single mwms (not groups).
  /// \return false in case of error and true otherwise.
  bool SearchNodesByPosition(m2::PointD const & position,
                             TOnSearchResultCallback const & result) const;
  /// \brief Returns node ids (mwms not groups of mwms) by a region name in a locale language.
  /// Finds in world mwm with name.
  /// For getting result callback SearchResultCallback should be implemented.
  /// \param name string to search in world.mwm.
  /// \param locale wich is used for searching. E.g. "ru" or "zh-Hant".
  /// \return false in case of error and true otherwise.
  bool SearchNodesByName(search::SearchParams const & searchParams,
                         TOnSearchResultCallback const & result) const;

  /// \brief Returns current version for mwms which are available on the server.
  /// That means the version is written in county_attributes.txt
  size_t GetNodeServerVersion() const;
  /// \brief Returns true if the node with nodeId has been downloaded and false othewise.
  /// If nodeId is a expandable returns true if all mwms which belongs to it have downloaded.
  /// Returns false if nodeId is an unknown string.
  bool IsNodeDownloaded(TIndex const & nodeId) const;
  /// \brief Returns true if position is covered by a downloaded mwms.
  /// \note position is coordinates in mercator.
  bool IsPointCovered(m2::PointD const & position) const;

  /// \brief Gets attributes for an available on server node by nodeId.
  /// \param nodeId is id of single mwm or a group of mwm.
  /// \param ServerNodeAttrs is filled with attibutes of node which is available for downloading.
  /// I.e. is written in county_attributes.txt.
  /// \return false in case of error and true otherwise.
  bool GetServerNodeAttrs(TIndex const & nodeId, ClientNodeAttrs & serverNodeAttrs) const;
  /// \brief Gets attributes for downloaded a node by nodeId.
  /// \param ClientNodeAttrs is filled with attibutes in this method.
  /// \return false in case of error and true otherwise.
  bool GetClientNodeAttrs(TIndex const & nodeId, ClientNodeAttrs & clientNodeAttrs) const;

  /// \brief Downloads one node (expandable or not) by nodeId.
  /// If node is expandable downloads all children (grandchildren) by the node
  /// until they havn't been downloaded before. Update all downloaded mwm if it's necessary.
  /// \return false in case of error and true otherwise.
  bool DownloadNode(TIndex const & nodeId);
  /// \brief Delete one node (expandable or not).
  /// \return false in case of error and true otherwise.
  bool DeleteNode(TIndex const & nodeId);
  /// \brief Updates one node (expandable or not).
  /// \note If you want to update all the maps and this update is without changing
  /// borders or hierarchy just call UpdateNode(GetRootId()).
  /// \return false in case of error and true otherwise.
  bool UpdateNode(TIndex const & nodeId);
  /// \brief Cancels downloading a node if the downloading is in process.
  /// \return false in case of error and true otherwise.
  bool CancelNodeDownloading(TIndex const & nodeId);
  /// \brief Downloading process could be interupted because of bad internet connection.
  /// In that case user could want to recover it. This method is done for it.
  /// This method works with expandable and not expandable nodeId.
  /// \return false in case of error and true otherwise.
  bool RestoreNodeDownloading(TIndex const & nodeId);

  /// \brief Shows a node (expandable or not) on the map.
  /// \return false in case of error and true otherwise.
  bool ShowNode(TIndex const & nodeId);

  /// \brief Get information for mwm update button.
  /// \return true if updateInfo is filled correctly and false otherwise.
  bool GetUpdateInfo(UpdateInfo & updateInfo) const;
  /// \brief Update all mwm in case of changing mwm hierarchy of mwm borders.
  /// This method:
  /// * removes all mwms
  /// * downloads mwms with the same coverage
  /// \note This method is used in very rare case.
  /// \return false in case of error and true otherwise.
  bool UpdateAllAndChangeHierarchy();

  /// \brief Subscribe on change status callback.
  /// \returns a unique index of added status callback structure.
  size_t SubscribeStatusCallback(StatusCallback const & statusCallbacks);
  /// \brief Unsubscribe from change status callback.
  /// \param index is a unique index of callback retruned by SubscribeStatusCallback.
  void UnsubscribeStatusCallback(size_t index);
  //@}

  void Init(TUpdate const & update);

  // Clears local files registry and downloader's queue.
  void Clear();

  // Finds and registers all map files in maps directory. In the case
  // of several versions of the same map keeps only the latest one, others
  // are deleted from disk.
  // *NOTE* storage will forget all already known local maps.
  void RegisterAllLocalMaps();

  // Returns list of all local maps, including fake countries (World*.mwm).
  void GetLocalMaps(vector<TLocalFilePtr> & maps) const;
  // Returns number of downloaded maps (files), excluding fake countries (World*.mwm).
  size_t GetDownloadedFilesCount() const;

  /// @return unique identifier that should be used with Unsubscribe function
  int Subscribe(TChangeCountryFunction const & change, TProgressFunction const & progress);
  void Unsubscribe(int slotId);

  Country const & CountryByIndex(TIndex const & index) const;
  TIndex FindIndexByFile(string const & name) const;
  /// @todo Temporary function to gel all associated indexes for the country file name.
  /// Will be removed in future after refactoring.
  vector<TIndex> FindAllIndexesByFile(string const & name) const;
  void GetGroupAndCountry(TIndex const & index, string & group, string & country) const;

  size_t CountriesCount(TIndex const & index) const;
  string const & CountryName(TIndex const & index) const;
  bool IsIndexInCountryTree(TIndex const & index) const;

  LocalAndRemoteSizeT CountrySizeInBytes(TIndex const & index, MapOptions opt) const;
  platform::CountryFile const & GetCountryFile(TIndex const & index) const;
  TLocalFilePtr GetLatestLocalFile(platform::CountryFile const & countryFile) const;
  TLocalFilePtr GetLatestLocalFile(TIndex const & index) const;

  /// Fast version, doesn't check if country is out of date
  TStatus CountryStatus(TIndex const & index) const;
  /// Slow version, but checks if country is out of date
  TStatus CountryStatusEx(TIndex const & index) const;
  void CountryStatusEx(TIndex const & index, TStatus & status, MapOptions & options) const;

  /// Puts country denoted by index into the downloader's queue.
  /// During downloading process notifies observers about downloading
  /// progress and status changes.
  void DownloadCountry(TIndex const & index, MapOptions opt);

  /// Removes country files (for all versions) from the device.
  /// Notifies observers about country status change.
  void DeleteCountry(TIndex const & index, MapOptions opt);

  /// Removes country files of a particular version from the device.
  /// Notifies observers about country status change.
  void DeleteCustomCountryVersion(platform::LocalCountryFile const & localFile);

  /// \return True iff country denoted by index was successfully
  ///          deleted from the downloader's queue.
  bool DeleteFromDownloader(TIndex const & index);
  bool IsDownloadInProgress() const;

  TIndex GetCurrentDownloadingCountryIndex() const;

  void NotifyStatusChanged(TIndex const & index);

  /// get download url by index & options(first search file name by index, then format url)
  string GetFileDownloadUrl(string const & baseUrl, TIndex const & index, MapOptions file) const;
  /// get download url by base url & file name
  string GetFileDownloadUrl(string const & baseUrl, string const & fName) const;

  /// @param[out] res Populated with oudated countries.
  void GetOutdatedCountries(vector<Country const *> & countries) const;

  inline int64_t GetCurrentDataVersion() const { return m_currentVersion; }

  void SetDownloaderForTesting(unique_ptr<MapFilesDownloader> && downloader);
  void SetCurrentDataVersionForTesting(int64_t currentVersion);

private:
  friend void UnitTest_StorageTest_DeleteCountry();

  TStatus CountryStatusWithoutFailed(TIndex const & index) const;
  TStatus CountryStatusFull(TIndex const & index, TStatus const status) const;

  // Modifies file set of requested files - always adds a map file
  // when routing file is requested for downloading, but drops all
  // already downloaded and up-to-date files.
  MapOptions NormalizeDownloadFileSet(TIndex const & index, MapOptions options) const;

  // Modifies file set of file to deletion - always adds (marks for
  // removal) a routing file when map file is marked for deletion.
  MapOptions NormalizeDeleteFileSet(MapOptions options) const;

  // Returns a pointer to a country in the downloader's queue.
  QueuedCountry * FindCountryInQueue(TIndex const & index);

  // Returns a pointer to a country in the downloader's queue.
  QueuedCountry const * FindCountryInQueue(TIndex const & index) const;

  // Returns true when country is in the downloader's queue.
  bool IsCountryInQueue(TIndex const & index) const;

  // Returns true when country is first in the downloader's queue.
  bool IsCountryFirstInQueue(TIndex const & index) const;

  // Returns local country files of a particular version, or wrapped
  // nullptr if there're no country files corresponding to the
  // version.
  TLocalFilePtr GetLocalFile(TIndex const & index, int64_t version) const;

  // Tries to register disk files for a real (listed in countries.txt)
  // country. If map files of the same version were already
  // registered, does nothing.
  void RegisterCountryFiles(TLocalFilePtr localFile);

  // Registers disk files for a country. This method must be used only
  // for real (listed in counties.txt) countries.
  void RegisterCountryFiles(TIndex const & index, string const & directory, int64_t version);

  // Registers disk files for a country. This method must be used only
  // for custom (made by user) map files.
  void RegisterFakeCountryFiles(platform::LocalCountryFile const & localFile);

  // Removes disk files for all versions of a country.
  void DeleteCountryFiles(TIndex const & index, MapOptions opt);

  // Removes country files from downloader.
  bool DeleteCountryFilesFromDownloader(TIndex const & index, MapOptions opt);

  // Returns download size of the currently downloading file for the
  // queued country.
  uint64_t GetDownloadSize(QueuedCountry const & queuedCountry) const;

  // Returns a path to a place on disk downloader can use for
  // downloaded files.
  string GetFileDownloadPath(TIndex const & index, MapOptions file) const;
};
}  // storage
