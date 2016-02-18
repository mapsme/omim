#pragma once

#include <jni.h>

#include "map/framework.hpp"

#include "search/result.hpp"

#include "drape_frontend/gui/skin.hpp"

#include "drape/pointers.hpp"
#include "drape/oglcontextfactory.hpp"

#include "platform/country_defines.hpp"
#include "platform/location.hpp"

#include "geometry/avg_vector.hpp"

#include "base/timer.hpp"

#include "indexer/map_style.hpp"

#include "std/map.hpp"
#include "std/mutex.hpp"
#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"
#include "std/cstdint.hpp"

namespace android
{
  class Framework
  {
  private:
    drape_ptr<dp::ThreadSafeFactory> m_contextFactory;
    ::Framework m_work;

    math::LowPassVector<float, 3> m_sensors[2];
    double m_lastCompass;

    string m_searchQuery;

    map<gui::EWidget, gui::Position> m_guiPositions;

    void MyPositionModeChanged(location::EMyPositionMode mode);

    location::TMyPositionModeChanged m_myPositionModeSignal;
    location::EMyPositionMode m_currentMode;
    bool m_isCurrentModeInitialized;

    bool m_isChoosePositionMode;

    UserMark const * m_activeUserMark;

  public:
    Framework();
    ~Framework();

    storage::Storage & Storage();

    void ShowCountry(storage::TCountryId const & countryId, bool zoomToDownloadButton);

    void OnLocationError(int/* == location::TLocationStatus*/ newStatus);
    void OnLocationUpdated(location::GpsInfo const & info);
    void OnCompassUpdated(location::CompassInfo const & info, bool forceRedraw);
    void UpdateCompassSensor(int ind, float * arr);

    void Invalidate();

    bool CreateDrapeEngine(JNIEnv * env, jobject jSurface, int densityDpi);
    void DeleteDrapeEngine();
    bool IsDrapeEngineCreated();

    void DetachSurface();
    void AttachSurface(JNIEnv * env, jobject jSurface);

    void SetMapStyle(MapStyle mapStyle);
    void MarkMapStyle(MapStyle mapStyle);
    MapStyle GetMapStyle() const;

    void SetupMeasurementSystem();

    void SetRouter(routing::RouterType type) { m_work.SetRouter(type); }
    routing::RouterType GetRouter() const { return m_work.GetRouter(); }
    routing::RouterType GetLastUsedRouter() const { return m_work.GetLastUsedRouter(); }

    void Resize(int w, int h);

    struct Finger
    {
      Finger(int64_t id, float x, float y)
        : m_id(id)
        , m_x(x)
        , m_y(y)
      {
      }

      int64_t m_id;
      float m_x, m_y;
    };

    void Touch(int action, Finger const & f1, Finger const & f2, uint8_t maskedPointer);

    bool Search(search::SearchParams const & params);
    string GetLastSearchQuery() { return m_searchQuery; }
    void ClearLastSearchQuery() { m_searchQuery.clear(); }

    void AddLocalMaps();
    void RemoveLocalMaps();

    string GetCountryNameIfAbsent(m2::PointD const & pt) const;
    m2::PointD GetViewportCenter() const;

    void AddString(string const & name, string const & value);

    void Scale(::Framework::EScaleMode mode);

    BookmarkAndCategory AddBookmark(size_t category, m2::PointD const & pt, BookmarkData & bm);
    void ReplaceBookmark(BookmarkAndCategory const & ind, BookmarkData & bm);
    size_t ChangeBookmarkCategory(BookmarkAndCategory const & ind, size_t newCat);

    ::Framework * NativeFramework();

    bool IsDownloadingActive();

    bool ShowMapForURL(string const & url);

    void DeactivatePopup();

    string GetOutdatedCountriesString();

    void ShowTrack(int category, int track);

    void SetMyPositionModeListener(location::TMyPositionModeChanged const & fn);
    location::EMyPositionMode GetMyPositionMode() const;
    void SetMyPositionMode(location::EMyPositionMode mode);

    void Save3dMode(bool allow3d, bool allow3dBuildings);
    void Set3dMode(bool allow3d, bool allow3dBuildings);
    void Get3dMode(bool & allow3d, bool & allow3dBuildings);

    void SetChoosePositionMode(bool isChoosePositionMode);

    void SetupWidget(gui::EWidget widget, float x, float y, dp::Anchor anchor);
    void ApplyWidgets();
    void CleanWidgets();

    using TDrapeTask = function<void()>;
    // Posts a task which must be executed when Drape Engine is alive.
    void PostDrapeTask(TDrapeTask && task);

    void SetActiveUserMark(UserMark const * mark);
    UserMark const * GetActiveUserMark();

    bool NeedMigrate();
    void Migrate();

  private:
    vector<TDrapeTask> m_drapeTasksQueue;
    mutex m_drapeQueueMutex;

    // This method must be executed under mutex m_drapeQueueMutex.
    void ExecuteDrapeTasks();
  };
}

extern android::Framework * g_framework;
