#pragma once

#include "map/bookmark_manager.hpp"

#include <string>

enum class RouteMarkType : uint8_t
{
  // Do not change the order!
  Start = 0,
  Intermediate = 1,
  Finish = 2
};

struct RouteMarkData
{
  std::string m_title;
  std::string m_subTitle;
  RouteMarkType m_pointType = RouteMarkType::Start;
  size_t m_intermediateIndex = 0;
  bool m_isVisible = true;
  bool m_isMyPosition = false;
  bool m_isPassed = false;
  m2::PointD m_position;
};

class RouteMarkPoint : public UserMark
{
public:
  RouteMarkPoint(m2::PointD const & ptOrg);
  virtual ~RouteMarkPoint() {}

  bool IsVisible() const override { return m_markData.m_isVisible; }
  void SetIsVisible(bool isVisible) { m_markData.m_isVisible = isVisible; }

  dp::Anchor GetAnchor() const override;
  df::DepthLayer GetDepthLayer() const override;

  drape_ptr<SymbolNameZoomInfo> GetSymbolNames() const override;
  bool IsAvailableForSearch() const override { return !IsPassed(); }

  RouteMarkType GetRoutePointType() const { return m_markData.m_pointType; }
  void SetRoutePointType(RouteMarkType type);

  void SetIntermediateIndex(size_t index);
  size_t GetIntermediateIndex() const { return m_markData.m_intermediateIndex; }

  void SetRoutePointFullType(RouteMarkType type, size_t intermediateIndex);
  bool IsEqualFullType(RouteMarkType type, size_t intermediateIndex) const;

  void SetIsMyPosition(bool isMyPosition);
  bool IsMyPosition() const { return m_markData.m_isMyPosition; }

  void SetPassed(bool isPassed);
  bool IsPassed() const { return m_markData.m_isPassed; }

  uint16_t GetPriority() const override;
  uint32_t GetIndex() const override;

  RouteMarkData const & GetMarkData() const { return m_markData; }
  void SetMarkData(RouteMarkData && data);

  drape_ptr<TitlesInfo> GetTitleDecl() const override;

  bool HasSymbolPriority() const override { return false; }
  bool HasTitlePriority() const override { return true; }

  void SetFollowingMode(bool enabled);

private:
  RouteMarkData m_markData;
  dp::TitleDecl m_titleDecl;
  bool m_followingMode = false;
};

class RoutePointsLayout
{
public:
  static size_t const kMaxIntermediatePointsCount;

  RoutePointsLayout(BookmarkManager & manager);

  void AddRoutePoint(RouteMarkData && data);
  RouteMarkPoint const * GetRoutePoint(RouteMarkType type, size_t intermediateIndex = 0) const;
  RouteMarkPoint * GetRoutePointForEdit(RouteMarkType type, size_t intermediateIndex = 0);
  RouteMarkPoint const * GetMyPositionPoint() const;
  std::vector<RouteMarkPoint *> GetRoutePoints();
  size_t GetRoutePointsCount() const;
  bool RemoveRoutePoint(RouteMarkType type, size_t intermediateIndex = 0);
  void RemoveRoutePoints();
  void RemoveIntermediateRoutePoints();
  bool MoveRoutePoint(RouteMarkType currentType, size_t currentIntermediateIndex,
                      RouteMarkType destType, size_t destIntermediateIndex);
  void PassRoutePoint(RouteMarkType type, size_t intermediateIndex = 0);
  void SetFollowingMode(bool enabled);

private:
  using TRoutePointCallback = function<void (RouteMarkPoint * mark)>;
  void ForEachIntermediatePoint(TRoutePointCallback const & fn);

  BookmarkManager & m_manager;
  BookmarkManager::EditSession m_editSession;
};

class TransitMark : public UserMark
{
public:
  explicit TransitMark(m2::PointD const & ptOrg);

  df::DepthLayer GetDepthLayer() const override { return df::DepthLayer::TransitMarkLayer; }

  bool HasSymbolPriority() const override { return !m_symbolNames.empty() || !m_coloredSymbols.empty(); }
  bool HasTitlePriority() const override { return true; }

  void SetAnchor(dp::Anchor anchor);
  dp::Anchor GetAnchor() const override;

  void SetFeatureId(FeatureID featureId);
  FeatureID GetFeatureID() const override { return m_featureId; }

  void SetIndex(uint32_t index);
  uint32_t GetIndex() const override { return m_index; }

  void SetPriority(Priority priority);
  uint16_t GetPriority() const override { return static_cast<uint16_t>(m_priority); }

  void SetMinZoom(int minZoom);
  int GetMinZoom() const override { return m_minZoom; }

  void SetMinTitleZoom(int minTitleZoom);
  int GetMinTitleZoom() const override { return m_minTitleZoom; }

  void SetColoredSymbols(ColoredSymbolZoomInfo const & symbolParams);
  drape_ptr<ColoredSymbolZoomInfo> GetColoredSymbols() const override;

  void SetSymbolNames(SymbolNameZoomInfo const & symbolNames);
  drape_ptr<SymbolNameZoomInfo> GetSymbolNames() const override;

  void SetSymbolSizes(SymbolSizes const & symbolSizes);
  drape_ptr<SymbolSizes> GetSymbolSizes() const override;

  void SetSymbolOffsets(SymbolOffsets const & symbolSizes);
  drape_ptr<SymbolOffsets> GetSymbolOffsets() const override;

  void AddTitle(dp::TitleDecl const & titleDecl);
  drape_ptr<TitlesInfo> GetTitleDecl() const override;

  static void GetDefaultTransitTitle(dp::TitleDecl & titleDecl);

private:
  int m_minZoom = 1;
  int m_minTitleZoom = 1;
  uint32_t m_index = 0;
  Priority m_priority;
  FeatureID m_featureId;
  TitlesInfo m_titles;
  SymbolNameZoomInfo m_symbolNames;
  ColoredSymbolZoomInfo m_coloredSymbols;
  SymbolSizes m_symbolSizes;
  SymbolOffsets m_symbolOffsets;
  dp::Anchor m_anchor = dp::Center;
};
