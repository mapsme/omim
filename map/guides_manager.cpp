#include "map/guides_manager.hpp"

#include "map/bookmark_catalog.hpp"

#include "partners_api/utm.hpp"

#include "drape_frontend/visual_params.hpp"

#include "platform/preferred_languages.hpp"

#include "private.h"

#include <utility>

namespace
{
auto constexpr kRequestAttemptsCount = 3;
}  // namespace

GuidesManager::GuidesState GuidesManager::GetState() const
{
  return m_state;
}

void GuidesManager::SetStateListener(GuidesStateChangedFn const & onStateChanged)
{
  m_onStateChanged = onStateChanged;
}

void GuidesManager::UpdateViewport(ScreenBase const & screen)
{
  auto const zoom = df::GetDrawTileScale(screen);
  // TODO(a): to implement correct way to filter out same rects.
  if (m_currentRect.EqualDxDy(screen.GlobalRect(), 1e-4) && m_zoom == zoom)
    return;

  m_currentRect = screen.GlobalRect();
  m_zoom = zoom;

  if (m_state == GuidesState::Disabled || m_state == GuidesState::FatalNetworkError)
    return;

  RequestGuides(m_currentRect, m_zoom);
}

void GuidesManager::Invalidate()
{
  // TODO: Implement.
}

void GuidesManager::Reconnect()
{
  if (m_state != GuidesState::FatalNetworkError)
    return;

  ChangeState(GuidesState::Enabled);
  RequestGuides(m_currentRect, m_zoom);
}

void GuidesManager::SetEnabled(bool enabled)
{
  auto const newState = enabled ? GuidesState::Enabled : GuidesState::Disabled;
  if (newState == m_state)
    return;

  Clear();
  ChangeState(newState);
  RequestGuides(m_currentRect, m_zoom);
}

bool GuidesManager::IsEnabled() const
{
  return m_state != GuidesState::Disabled;
}

void GuidesManager::ChangeState(GuidesState newState)
{
  if (m_state == newState)
    return;
  m_state = newState;
  if (m_onStateChanged != nullptr)
    m_onStateChanged(newState);
}

void GuidesManager::RequestGuides(m2::AnyRectD const & rect, int zoom)
{
  auto const requestNumber = ++m_requestCounter;
  m_api.GetGuidesOnMap(
      rect, zoom,
      [this](guides_on_map::GuidesOnMap const & guides) {
        if (m_state == GuidesState::Disabled)
          return;

        m_guides = guides;
        m_errorRequestsCount = 0;

        if (!m_guides.empty())
          ChangeState(GuidesState::Enabled);
        else
          ChangeState(GuidesState::NoData);

        if (m_onGalleryChanged)
          m_onGalleryChanged(true);
      },
      [this, requestNumber, zoom]() mutable {
        if (m_state == GuidesState::Disabled || m_state == GuidesState::FatalNetworkError)
          return;

        if (++m_errorRequestsCount >= kRequestAttemptsCount)
        {
          Clear();
          ChangeState(GuidesState::FatalNetworkError);
        }
        else
        {
          ChangeState(GuidesState::NetworkError);
        }

        // Re-request only when no additional requests enqueued.
        if (requestNumber == m_requestCounter)
          RequestGuides(m_currentRect, zoom);
      });
}

void GuidesManager::Clear()
{
  m_activeGuide.clear();
  m_guides.clear();
  m_errorRequestsCount = 0;
}

GuidesManager::GuidesGallery GuidesManager::GetGallery() const
{
  // Dummy gallery hardcode for debug only.
  GuidesGallery gallery;
  {
    GuidesGallery::Item item;
    item.m_guideId = "048f4c49-ee80-463f-8513-e57ade2303ee";
    item.m_url = "https://routes.maps.me/en/v3/mobilefront/route/048f4c49-ee80-463f-8513-e57ade2303ee";
    item.m_imageUrl = "https://storage.maps.me/bookmarks_catalogue/"
                      "002dc2ae-7b5c-4d3c-88bc-7c7ba109d0e8.jpg?t=1584470956.009026";
    item.m_title = "Moscow by The Village";
    item.m_subTitle = "awesome city guide";
    item.m_type = GuidesGallery::Item::Type::City;
    item.m_downloaded = false;
    item.m_cityParams.m_bookmarksCount = 32;
    item.m_cityParams.m_trackIsAvailable = false;

    gallery.m_items.emplace_back(std::move(item));
  }

  {
    GuidesGallery::Item item;
    item.m_guideId = "e2d448eb-7fa4-4fab-93e7-ef0fea91cfff";
    item.m_url = "https://routes.maps.me/en/v3/mobilefront/route/e2d448eb-7fa4-4fab-93e7-ef0fea91cfff";
    item.m_imageUrl = "https://storage.maps.me/bookmarks_catalogue/"
                      "002dc2ae-7b5c-4d3c-88bc-7c7ba109d0e8.jpg?t=1584470956.009026";
    item.m_title = "Riga City Tour";
    item.m_subTitle = "awesome city guide";
    item.m_type = GuidesGallery::Item::Type::City;
    item.m_downloaded = true;
    item.m_cityParams.m_bookmarksCount = 31;
    item.m_cityParams.m_trackIsAvailable = true;

    gallery.m_items.emplace_back(std::move(item));
  }

  {
    GuidesGallery::Item item;
    item.m_guideId = "d26a6662-20a3-432c-a357-c9cb3cce6d57";
    item.m_url = "https://routes.maps.me/en/v3/mobilefront/route/d26a6662-20a3-432c-a357-c9cb3cce6d57";
    item.m_imageUrl = "https://img.oastatic.com/img2/1966324/834x417s/t.jpg";
    item.m_title = "Klassik trifft Romantik";
    item.m_subTitle = "Hiking / Trekking";
    item.m_type = GuidesGallery::Item::Type::Outdoor;
    item.m_downloaded = false;
    item.m_outdoorsParams.m_ascent = 400;
    item.m_outdoorsParams.m_distance = 24100;
    item.m_outdoorsParams.m_duration = 749246;

    gallery.m_items.emplace_back(std::move(item));
  }

  for (auto const & guide : m_guides)
  {
    if (guide.m_outdoorCount + guide.m_sightsCount != 1)
      continue;

    auto const & info = guide.m_guideInfo;

    GuidesGallery::Item item;
    item.m_guideId = info.m_id;

    auto url = url::Join(BOOKMARKS_CATALOG_FRONT_URL, languages::GetCurrentNorm(), "v3/mobilefront",
                         info.m_id);
    InjectUTM(url, UTM::GuidesOnMapGallery);
    item.m_url = std::move(url);
    item.m_imageUrl = info.m_imageUrl;
    item.m_title = info.m_name;
    item.m_downloaded = m_delegate->IsGuideDownloaded(info.m_id);

    if (guide.m_sightsCount == 1)
    {
      item.m_type = GuidesGallery::Item::Type::City;
      item.m_cityParams.m_bookmarksCount = guide.m_guideInfo.m_bookmarksCount;
      item.m_cityParams.m_trackIsAvailable = guide.m_guideInfo.m_hasTrack;
      item.m_subTitle = "TODO(a): to add correct value";
    }
    else
    {
      item.m_type = GuidesGallery::Item::Type::Outdoor;
      item.m_outdoorsParams.m_duration = guide.m_guideInfo.m_tourDuration;
      item.m_outdoorsParams.m_distance = guide.m_guideInfo.m_tracksLength;
      item.m_outdoorsParams.m_ascent = guide.m_guideInfo.m_ascent;
      item.m_subTitle = guide.m_guideInfo.m_tag;
    }

    gallery.m_items.emplace_back(std::move(item));
  }

  // Dummy, for debug only.
  while (gallery.m_items.size() < 10)
  {
    std::copy(gallery.m_items.begin(), gallery.m_items.end(), std::back_inserter(gallery.m_items));
  }

  return gallery;
}

std::string GuidesManager::GetActiveGuide() const { return m_activeGuide; }

void GuidesManager::SetActiveGuide(std::string const & guideId)
{
  if (m_activeGuide == guideId)
    return;

  m_activeGuide = guideId;
}

void GuidesManager::SetGalleryListener(GuidesGalleryChangedFn const & onGalleryChanged)
{
  m_onGalleryChanged = onGalleryChanged;
}

void GuidesManager::SetDelegate(std::unique_ptr<Delegate> delegate)
{
  m_delegate = std::move(delegate);
}

void GuidesManager::SetApiDelegate(std::unique_ptr<guides_on_map::Api::Delegate> apiDelegate)
{
  m_api.SetDelegate(std::move(apiDelegate));
}
