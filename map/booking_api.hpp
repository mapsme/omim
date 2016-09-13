#pragma once

#include "geometry/point2d.hpp"

#include "platform/http_request.hpp"

#include "std/chrono.hpp"
#include "std/function.hpp"
#include "std/initializer_list.hpp"
#include "std/string.hpp"
#include "std/unique_ptr.hpp"
#include "std/utility.hpp"

class BookingApi
{
  string m_affiliateId;
  string m_apiUrl;

public:
  static constexpr const char kDefaultCurrency[1] = {0};

  struct Details
  {
    string m_bookingId;
    string m_hotelId;
    m2::PointD m_point;
    system_clock::time_point m_arrivalDate;
    system_clock::time_point m_departureDate;
    system_clock::time_point m_bookingTimestamp;
    bool hasArrivalDate = false;
    bool hasDepartureDate = false;

    Details() = default;
    Details(string const & bookingId, string const & hotelId, m2::PointD const & point,
            system_clock::time_point const & arrivalDate,
            system_clock::time_point const & departureDate,
            system_clock::time_point const & bookingTimestamp)
      : m_bookingId(bookingId)
      , m_hotelId(hotelId)
      , m_point(point)
      , m_arrivalDate(arrivalDate)
      , m_departureDate(departureDate)
      , m_bookingTimestamp(bookingTimestamp)
    {}
  };

  BookingApi();
  string GetBookingUrl(string const & baseUrl, string const & lang = string()) const;
  string GetDescriptionUrl(string const & baseUrl, string const & lang = string()) const;
  void GetMinPrice(string const & hotelId, string const & currency,
                   function<void(string const &, string const &)> const & fn);
  void GetBookingDetails(system_clock::time_point const & timestampBegin,
                         system_clock::time_point const & timestampEnd,
                         vector<string> const & hotelIds,
                         function<void(vector<Details> const & details, bool success)> const & fn);

protected:
  unique_ptr<downloader::HttpRequest> m_minPriceRequest;
  unique_ptr<downloader::HttpRequest> m_bookingDetailsRequest;

  static string MakeApiUrl(string const & url, string const & func,
                           initializer_list<pair<string, string>> const & params);
};
