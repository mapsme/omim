#include "map/bookmark.hpp"
#include "map/bookmark_helpers.hpp"

#include "coding/url.hpp"

#include "base/string_utils.hpp"

#include <sstream>

#include "private.h"

namespace
{
std::string const kDeepLinkUrl = DLINK_URL;

std::string GetBookmarkIconType(kml::BookmarkIcon const & icon)
{
  switch (icon)
  {
  case kml::BookmarkIcon::None: return "default";
  case kml::BookmarkIcon::Hotel: return "hotel";
  case kml::BookmarkIcon::Animals: return "animals";
  case kml::BookmarkIcon::Buddhism: return "buddhism";
  case kml::BookmarkIcon::Building: return "building";
  case kml::BookmarkIcon::Christianity: return "christianity";
  case kml::BookmarkIcon::Entertainment: return "entertainment";
  case kml::BookmarkIcon::Exchange: return "exchange";
  case kml::BookmarkIcon::Food: return "food";
  case kml::BookmarkIcon::Gas: return "gas";
  case kml::BookmarkIcon::Judaism: return "judaism";
  case kml::BookmarkIcon::Medicine: return "medicine";
  case kml::BookmarkIcon::Mountain: return "mountain";
  case kml::BookmarkIcon::Museum: return "museum";
  case kml::BookmarkIcon::Islam: return "islam";
  case kml::BookmarkIcon::Park: return "park";
  case kml::BookmarkIcon::Parking: return "parking";
  case kml::BookmarkIcon::Shop: return "shop";
  case kml::BookmarkIcon::Sights: return "sights";
  case kml::BookmarkIcon::Swim: return "swim";
  case kml::BookmarkIcon::Water: return "water";
  case kml::BookmarkIcon::Bar: return "bar";
  case kml::BookmarkIcon::Transport: return "transport";
  case kml::BookmarkIcon::Viewpoint: return "viewpoint";
  case kml::BookmarkIcon::Sport: return "sport";
  case kml::BookmarkIcon::Start: return "start";
  case kml::BookmarkIcon::Finish: return "finish";
  case kml::BookmarkIcon::Count:
    ASSERT(false, ("Invalid bookmark icon type"));
    return {};
  }
  UNREACHABLE();
}

std::string const kCustomImageProperty = "CustomImage";
std::string const kHasElevationProfileProperty = "has_elevation_profile";
}  // namespace

Bookmark::Bookmark(m2::PointD const & ptOrg)
  : Base(ptOrg, UserMark::BOOKMARK)
  , m_groupId(kml::kInvalidMarkGroupId)
{
  m_data.m_point = ptOrg;
  m_data.m_id = GetId();
}

Bookmark::Bookmark(kml::BookmarkData && data)
  : Base(data.m_id, data.m_point, UserMark::BOOKMARK)
  , m_data(std::move(data))
  , m_groupId(kml::kInvalidMarkGroupId)
{
  m_data.m_id = GetId();
}

void Bookmark::SetData(kml::BookmarkData const & data)
{
  SetDirty();
  m_data = data;
}

kml::BookmarkData const & Bookmark::GetData() const
{
  return m_data;
}

search::ReverseGeocoder::RegionAddress const & Bookmark::GetAddress() const
{
  return m_address;
}

void Bookmark::SetAddress(search::ReverseGeocoder::RegionAddress const & address)
{
  SetDirty();
  m_address = address;
}

dp::Anchor Bookmark::GetAnchor() const
{
  return dp::Bottom;
}

drape_ptr<df::UserPointMark::SymbolNameZoomInfo> Bookmark::GetSymbolNames() const
{
  auto symbolNames = GetCustomSymbolNames();
  if (symbolNames != nullptr)
    return symbolNames;

  symbolNames = make_unique_dp<SymbolNameZoomInfo>();

  symbolNames->insert(std::make_pair(1 /* zoomLevel */, "bookmark-default-xs"));
  symbolNames->insert(std::make_pair(8 /* zoomLevel */, "bookmark-default-s"));
  auto const iconType = GetBookmarkIconType(m_data.m_icon);
  symbolNames->insert(std::make_pair(14 /* zoomLevel */, "bookmark-" + iconType + "-m"));
  return symbolNames;
}

drape_ptr<df::UserPointMark::SymbolNameZoomInfo> Bookmark::GetCustomSymbolNames() const
{
  auto const it = m_data.m_properties.find(kCustomImageProperty);
  if (it == m_data.m_properties.end())
    return nullptr;

  auto symbolNames = make_unique_dp<SymbolNameZoomInfo>();
  strings::Tokenize(it->second, ";", [&](std::string const & token)
  {
    int zoomLevel = 1;
    std::string name = token;
    auto pos = token.find(',');
    if (pos != std::string::npos && strings::to_int(token.substr(0, pos), zoomLevel))
      name = token.substr(pos + 1);
    if (!name.empty() && zoomLevel >= 1 && zoomLevel <= 18)
      symbolNames->insert(std::make_pair(zoomLevel, name));
  });

  if (symbolNames->empty())
    return nullptr;

  return symbolNames;
}

df::ColorConstant Bookmark::GetColorConstant() const
{
  switch (m_data.m_color.m_predefinedColor)
  {
    case kml::PredefinedColor::Red:
      return "BookmarkRed";
    case kml::PredefinedColor::Blue:
      return "BookmarkBlue";
    case kml::PredefinedColor::Purple:
      return "BookmarkPurple";
    case kml::PredefinedColor::Yellow:
      return "BookmarkYellow";
    case kml::PredefinedColor::Pink:
      return "BookmarkPink";
    case kml::PredefinedColor::Brown:
      return "BookmarkBrown";
    case kml::PredefinedColor::Green:
      return "BookmarkGreen";
    case kml::PredefinedColor::Orange:
      return "BookmarkOrange";
    case kml::PredefinedColor::DeepPurple:
      return "BookmarkDeepPurple";
    case kml::PredefinedColor::LightBlue:
      return "BookmarkLightBlue";
    case kml::PredefinedColor::Cyan:
      return "BookmarkCyan";
    case kml::PredefinedColor::Teal:
      return "BookmarkTeal";
    case kml::PredefinedColor::Lime:
      return "BookmarkLime";
    case kml::PredefinedColor::DeepOrange:
      return "BookmarkDeepOrange";
    case kml::PredefinedColor::Gray:
      return "BookmarkGray";
    case kml::PredefinedColor::BlueGray:
      return "BookmarkBlueGray";
    case kml::PredefinedColor::None:
    case kml::PredefinedColor::Count:
      return "BookmarkRed";
  }
  UNREACHABLE();
}

bool Bookmark::HasCreationAnimation() const
{
  return true;
}

kml::PredefinedColor Bookmark::GetColor() const
{
  return m_data.m_color.m_predefinedColor;
}

void Bookmark::SetColor(kml::PredefinedColor color)
{
  SetDirty();
  m_data.m_color.m_predefinedColor = color;
}

std::string Bookmark::GetPreferredName() const
{
  return GetPreferredBookmarkName(m_data);
}

kml::LocalizableString Bookmark::GetName() const
{
  return m_data.m_name;
}

void Bookmark::SetName(kml::LocalizableString const & name)
{
  SetDirty();
  m_data.m_name = name;
}

void Bookmark::SetName(std::string const & name, int8_t langCode)
{
  SetDirty();
  m_data.m_name[langCode] = name;
}

std::string Bookmark::GetCustomName() const
{
  return GetPreferredBookmarkStr(m_data.m_customName);
}

void Bookmark::SetCustomName(std::string const & customName)
{
  SetDirty();
  kml::SetDefaultStr(m_data.m_customName, customName);
}

m2::RectD Bookmark::GetViewport() const
{
  return m2::RectD(GetPivot(), GetPivot());
}

std::string Bookmark::GetDescription() const
{
  return GetPreferredBookmarkStr(m_data.m_description);
}

void Bookmark::SetDescription(std::string const & description)
{
  SetDirty();
  kml::SetDefaultStr(m_data.m_description, description);
}

kml::Timestamp Bookmark::GetTimeStamp() const
{
  return m_data.m_timestamp;
}

void Bookmark::SetTimeStamp(kml::Timestamp timeStamp)
{
  SetDirty();
  m_data.m_timestamp = timeStamp;
}

uint8_t Bookmark::GetScale() const
{
  return m_data.m_viewportScale;
}

void Bookmark::SetScale(uint8_t scale)
{
  SetDirty();
  m_data.m_viewportScale = scale;
}

kml::MarkGroupId Bookmark::GetGroupId() const
{
  return m_groupId;
}

void Bookmark::Attach(kml::MarkGroupId groupId)
{
  ASSERT(m_groupId == kml::kInvalidMarkGroupId, ());
  m_groupId = groupId;
}

void Bookmark::Detach()
{
  m_groupId = kml::kInvalidMarkGroupId;
}

BookmarkCategory::BookmarkCategory(std::string const & name, kml::MarkGroupId groupId, bool autoSave)
  : Base(UserMark::Type::BOOKMARK)
  , m_autoSave(autoSave)
{
  m_data.m_id = groupId;
  SetName(name);
}

BookmarkCategory::BookmarkCategory(kml::CategoryData && data, bool autoSave)
  : Base(UserMark::Type::BOOKMARK)
  , m_autoSave(autoSave)
  , m_data(std::move(data))
{
  Base::SetIsVisible(m_data.m_visible);
}

void BookmarkCategory::SetIsVisible(bool isVisible)
{
  Base::SetIsVisible(isVisible);
  m_data.m_visible = isVisible;
}

void BookmarkCategory::SetName(std::string const & name)
{
  SetDirty();
  kml::SetDefaultStr(m_data.m_name, name);
}

void BookmarkCategory::SetDescription(std::string const & desc)
{
  SetDirty();
  kml::SetDefaultStr(m_data.m_description, desc);
}

void BookmarkCategory::SetServerId(std::string const & serverId)
{
  if (m_serverId == serverId)
    return;

  SetDirty();
  m_serverId = serverId;
}

void BookmarkCategory::SetTags(std::vector<std::string> const & tags)
{
  if (m_data.m_tags == tags)
    return;

  SetDirty();
  m_data.m_tags = tags;
}

void BookmarkCategory::SetCustomProperty(std::string const & key, std::string const & value)
{
  auto it = m_data.m_properties.find(key);
  if (it != m_data.m_properties.end() && it->second == value)
    return;

  SetDirty();
  m_data.m_properties[key] = value;
}

std::string BookmarkCategory::GetName() const
{
  return GetPreferredBookmarkStr(m_data.m_name);
}

bool BookmarkCategory::IsCategoryFromCatalog() const
{
  return FromCatalog(m_data, m_serverId);
}

bool BookmarkCategory::HasElevationProfile() const
{
  auto const it = m_data.m_properties.find(kHasElevationProfileProperty);
  return (it != m_data.m_properties.end()) && (it->second != "0");
}

std::string BookmarkCategory::GetCatalogDeeplink() const
{
  if (kDeepLinkUrl.empty())
    return {};

  std::ostringstream ss;
  ss << kDeepLinkUrl << "catalogue?id=" << m_serverId << "&name=" << url::UrlEncode(GetName());
  return ss.str();
}

std::string BookmarkCategory::GetCatalogPublicLink() const
{
  if (kDeepLinkUrl.empty())
    return {};

  std::ostringstream ss;
  ss << kDeepLinkUrl << "guides_page?url=route%2F" << m_serverId;
  return ss.str();
}

void BookmarkCategory::SetAuthor(std::string const & name, std::string const & id)
{
  if (m_data.m_authorName == name && m_data.m_authorId == id)
    return;

  SetDirty();
  m_data.m_authorName = name;
  m_data.m_authorId = id;
}

void BookmarkCategory::SetAccessRules(kml::AccessRules accessRules)
{
  if (m_data.m_accessRules == accessRules)
    return;

  SetDirty();
  m_data.m_accessRules = accessRules;
}

// static
kml::PredefinedColor BookmarkCategory::GetDefaultColor()
{
  return kml::PredefinedColor::Red;
}

void BookmarkCategory::SetDirty()
{
  Base::SetDirty();
  m_data.m_lastModified = std::chrono::system_clock::now();
}
