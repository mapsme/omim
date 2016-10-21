#pragma once

#include "drape_frontend/user_marks_provider.hpp"

#include "indexer/feature_decl.hpp"

#include "geometry/latlon.hpp"
#include "geometry/point2d.hpp"

#include "base/macros.hpp"

#include "std/string.hpp"
#include "std/unique_ptr.hpp"
#include "std/utility.hpp"


class UserMarkContainer;
class UserMarkCopy;

class UserMark : public df::UserPointMark
{
  DISALLOW_COPY_AND_MOVE(UserMark);
public:
  enum class Type
  {
    API,
    SEARCH,
    POI,
    BOOKMARK,
    MY_POSITION,
    GEOCHAT,
    DEBUG_MARK
  };

  UserMark(m2::PointD const & ptOrg, UserMarkContainer * container);
  virtual ~UserMark() {}

  // df::UserPointMark overrides.
  m2::PointD const & GetPivot() const override;
  m2::PointD GetPixelOffset() const override;
  dp::Anchor GetAnchor() const override;
  float GetDepth() const override;
  bool RunCreationAnim() const override;

  UserMarkContainer const * GetContainer() const;
  ms::LatLon GetLatLon() const;
  virtual Type GetMarkType() const = 0;

protected:
  m2::PointD m_ptOrg;
  mutable UserMarkContainer * m_container;
};

enum CustomMarkType
{
  DefaultMark = 0,
  BookingMark,
  GeochatMark,

  CustomMarkTypesCount
};

class SearchMarkPoint : public UserMark
{
public:
  SearchMarkPoint(m2::PointD const & ptOrg, UserMarkContainer * container);

  string GetSymbolName() const override;
  UserMark::Type GetMarkType() const override;

  FeatureID const & GetFoundFeature() const { return m_foundFeatureID; }
  void SetFoundFeature(FeatureID const & feature) { m_foundFeatureID = feature; }

  string const & GetMatchedName() const { return m_matchedName; }
  void SetMatchedName(string const & name) { m_matchedName = name; }

  string const & GetCustomSymbol() const { return m_customSymbol; }
  void SetCustomSymbol(string const & symbol) { m_customSymbol = symbol; }

protected:
  FeatureID m_foundFeatureID;
  // Used to pass exact search result matched string into a place page.
  string m_matchedName;
  string m_customSymbol;
};

class PoiMarkPoint : public SearchMarkPoint
{
public:
  PoiMarkPoint(UserMarkContainer * container);
  UserMark::Type GetMarkType() const override;

  void SetPtOrg(m2::PointD const & ptOrg);
};

class MyPositionMarkPoint : public PoiMarkPoint
{
public:
  MyPositionMarkPoint(UserMarkContainer * container);

  UserMark::Type GetMarkType() const override;

  void SetUserPosition(m2::PointD const & pt)
  {
    SetPtOrg(pt);
    m_hasPosition = true;
  }
  bool HasPosition() const { return m_hasPosition; }

private:
  bool m_hasPosition = false;
};

class GeochatMarkPoint : public UserMark
{
public:
  GeochatMarkPoint(m2::PointD const & ptOrg, UserMarkContainer * container)
    : UserMark(ptOrg, container)
  {
  }

  // df::UserPointMark overrides.
  string GetSymbolName() const override { return "search-geochats"; }
  UserMark::Type GetMarkType() const override { return UserMark::Type::GEOCHAT; }

  void SetGeochatId(string const & geochatId) { m_geochatId = geochatId; }
  string const & GetGeochatId() const { return m_geochatId; }
  void SetGeochatName(string const & geochatName) { m_geochatName = geochatName; }
  string const & GetGeochatName() const { return m_geochatName; }
  void SetMercator(m2::PointD const & pos) { m_pos = pos; }
  m2::PointD const & GetMercator() const { return m_pos; }
  void SetMembersCount(uint32_t const count) { m_membersCount = count; }
  uint32_t GetMmbersCount() const { return m_membersCount; }

private:
  string m_geochatId;
  string m_geochatName;
  m2::PointD m_pos = {0.0, 0.0};
  uint32_t m_membersCount = 0;
};

class DebugMarkPoint : public UserMark
{
public:
  DebugMarkPoint(m2::PointD const & ptOrg, UserMarkContainer * container);

  string GetSymbolName() const override;

  Type GetMarkType() const override { return UserMark::Type::DEBUG_MARK; }
};

string DebugPrint(UserMark::Type type);
