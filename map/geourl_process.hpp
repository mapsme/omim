#pragma once

#include "geometry/rect2d.hpp"

#include <string>


namespace url_scheme
{
  struct Info
  {
    double m_lat, m_lon, m_zoom;

    bool IsValid() const;
    void Reset();

    Info()
    {
      Reset();
    }

    void SetZoom(double x);
    bool SetLat(double x);
    bool SetLon(double x);
  };

  void ParseGeoURL(std::string const & s, Info & info);
}
