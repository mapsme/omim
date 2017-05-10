#pragma once

#include "map/user_mark.hpp"
#include "map/user_mark_container.hpp"

#include "geometry/point2d.hpp"

#include <string>

namespace style
{

// Fixes icons which are not supported by MapsWithMe.
std::string GetSupportedStyle(std::string const & s, std::string const & context, std::string const & fallback);
// Default icon.
std::string GetDefaultStyle();

} // style

class ApiMarkPoint : public UserMark
{
public:
  ApiMarkPoint(m2::PointD const & ptOrg, UserMarkContainer * container);

  ApiMarkPoint(std::string const & name, std::string const & id, std::string const & style,
               m2::PointD const & ptOrg, UserMarkContainer * container);

  std::string GetSymbolName() const override;
  UserMark::Type GetMarkType() const override;
  m2::PointD GetPixelOffset() const override;

  std::string const & GetName() const { return m_name; }
  void SetName(std::string const & name) { m_name = name; }

  std::string const & GetID() const { return m_id; }
  void SetID(std::string const & id) { m_id = id; }

  void SetStyle(std::string const & style) { m_style = style; }
  std::string const & GetStyle() const { return m_style; }

private:
  std::string m_name;
  std::string m_id;
  std::string m_style;
};
