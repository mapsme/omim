#pragma once

#include "geometry/rect2d.hpp"

#include "std/string.hpp"


namespace storage
{
  struct CountryDef
  {
    /// File name without extension (equal to english name - used in search for region).
    string m_name;
    m2::RectD m_rect;

    CountryDef() {}
    CountryDef(string const & name, m2::RectD const & rect)
      : m_name(name), m_rect(rect)
    {
    }
  };

  struct CountryInfo
  {
    CountryInfo() {}
    CountryInfo(string const & id, string const & flag) : m_name(id), m_flag(flag) {}

    /// Name (in native language) of country or region.
    /// (if empty - equals to file name of country - no additional memory)
    string m_name;

    /// Flag of country or region.
    string m_flag;

    bool IsNotEmpty() const { return !m_name.empty(); }

    // @TODO(bykoianko) Twine will be used intead of this function.
    // So id (fName) will be converted to a local name.
    static void FileName2FullName(string & fName);
    static void FullName2GroupAndMap(string const & fName, string & group, string & map);
  };
}
