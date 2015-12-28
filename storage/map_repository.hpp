#pragma once

#include "search/params.hpp"

#include "geometry/point2d.hpp"

#include "std/function.hpp"
#include "std/list.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

namespace storage
{
/// This class is responsible for downloading, updating and deleting maps.
/// It represents the interface which can be used by clients (Android/iOS).
/// The term node means an mwm or a group of mwm like a big country.
/// The term node id means a string id of mwm or a group of mwm. The sting contains
/// a name of file with mwm of a name country(territory).
class MapRepository
{
public:
  enum class ErrorCode;
  using TNodeId = string;
  using TOnSearchResultCallback = function<void (TNodeId const &)>;
  using TOnStatusChangedCallback = function<void (TNodeId const &)>;
  using TOnErrorCallback = function<void (TNodeId const &, ErrorCode)>;

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
    TNodeId parentId;
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

  /// \brief Initializing MapRepository. Initializing mwm hierarchy graph based on county_hierarchy.txt
  /// and mwm attributes (like size, version and so on) based on county_attributes.txt and
  /// downloaded mwm(s).
  MapRepository();

  /// \brief Returns root node id of the county tree.
  TNodeId const GetRootId() const;
  /// \brief Returns children node ids by a parent. For example GetChildren(GetRootId())
  /// returns all counties ids. It's content of map downloader list by default.
  vector<TNodeId> const GetChildren(TNodeId const & parent) const;
  /// \brief Returns children node ids by a parent for downloaded mwms.
  /// The result of the method is composed in a special way because of design requirements.
  /// If a direct child (of parent) contains two or more downloaded mwms the direct child id will be added to result.
  /// If a direct child (of parent) contains one downloaded mwm the mwm id will be added to result.
  /// If there's no downloaded mwms contained by a direct child the direct child id will not be added to result.
  vector<TNodeId> const GetDownloadedChildren(TNodeId const & parent) const;

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
  bool IsNodeDownloaded(TNodeId const & nodeId) const;
  /// \brief Returns true if position is covered by a downloaded mwms.
  /// \note position is coordinates in mercator.
  bool IsPointCovered(m2::PointD const & position) const;

  /// \brief Gets attributes for an available on server node by nodeId.
  /// \param nodeId is id of single mwm or a group of mwm.
  /// \param ServerNodeAttrs is filled with attibutes of node which is available for downloading.
  /// I.e. is written in county_attributes.txt.
  /// \return false in case of error and true otherwise.
  bool GetServerNodeAttrs(TNodeId const & nodeId, ClientNodeAttrs & serverNodeAttrs) const;
  /// \brief Gets attributes for downloaded a node by nodeId.
  /// \param ClientNodeAttrs is filled with attibutes in this method.
  /// \return false in case of error and true otherwise.
  bool GetClientNodeAttrs(TNodeId const & nodeId, ClientNodeAttrs & clientNodeAttrs) const;

  /// \brief Downloads one node (expandable or not) by nodeId.
  /// If node is expandable downloads all children (grandchildren) by the node
  /// until they havn't been downloaded before. Update all downloaded mwm if it's necessary.
  /// \return false in case of error and true otherwise.
  bool DownloadNode(TNodeId const & nodeId);
  /// \brief Delete one node (expandable or not).
  /// \return false in case of error and true otherwise.
  bool DeleteNode(TNodeId const & nodeId);
  /// \brief Updates one node (expandable or not).
  /// \note If you want to update all the maps and this update is without changing
  /// borders or hierarchy just call UpdateNode(GetRootId()).
  /// \return false in case of error and true otherwise.
  bool UpdateNode(TNodeId const & nodeId);
  /// \brief Cancels downloading a node if the downloading is in process.
  /// \return false in case of error and true otherwise.
  bool CancelNodeDownloading(TNodeId const & nodeId);
  /// \brief Downloading process could be interupted because of bad internet connection.
  /// In that case user could want to recover it. This method is done for it.
  /// This method works with expandable and not expandable nodeId.
  /// \return false in case of error and true otherwise.
  bool RestoreNodeDownloading(TNodeId const & nodeId);

  /// \brief Shows a node (expandable or not) on the map.
  /// \return false in case of error and true otherwise.
  bool ShowNode(TNodeId const & nodeId);

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

private:
  list<StatusCallback> m_statusCallbacks;
};
} //  namespace storage
