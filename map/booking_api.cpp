#include "map/booking_api.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "std/iostream.hpp"
#include "std/sstream.hpp"

#include "3party/jansson/myjansson.hpp"

#include "private.h"

char const BookingApi::kDefaultCurrency[1];

string const kRequestTimestampFormat = "%Y-%m-%d+%H:%M:%S";
string const kResponseTimestampFormat = "%Y-%m-%d %H:%M:%S";
string const kResponseDateFormat = "%Y-%m-%d";

string const kServerUrl = "http://tile.osmz.ru/bt/";

BookingApi::BookingApi() : m_affiliateId(BOOKING_AFFILIATE_ID)
{
  stringstream ss;
  ss << BOOKING_KEY << ":" << BOOKING_SECRET;
  m_apiUrl = "https://" + ss.str() + "@distribution-xml.booking.com/json/bookings.";
}
string BookingApi::GetBookingUrl(string const & baseUrl, string const & /* lang */) const
{
  return GetDescriptionUrl(baseUrl) + "#availability";
}

string BookingApi::GetDescriptionUrl(string const & baseUrl, string const & /* lang */) const
{
  return baseUrl + "?aid=" + m_affiliateId;
}

void BookingApi::GetMinPrice(string const & hotelId, string const & currency,
                             function<void(string const &, string const &)> const & fn)
{
  system_clock::time_point p = system_clock::from_time_t(time(nullptr));

  int constexpr kBufferSize = 12;
  string const kFormat = "%Y-%m-%d";
  string const dateArrival = strings::FromTimePoint<kBufferSize>(p, kFormat);
  string const dateDeparture = strings::FromTimePoint<kBufferSize>(p + hours(24), kFormat);

  string url = MakeApiUrl(m_apiUrl, "getHotelAvailability", {{"hotel_ids", hotelId},
                                                             {"currency_code", currency},
                                                             {"arrival_date", dateArrival},
                                                             {"departure_date", dateDeparture}});
  auto const callback = [this, fn, currency](downloader::HttpRequest & answer)
  {
    string minPrice;
    string priceCurrency;
    try
    {
      my::Json root(answer.Data().c_str());
      if (!json_is_array(root.get()))
        MYTHROW(my::Json::Exception, ("The answer must contain a json array."));
      size_t const sz = json_array_size(root.get());

      if (sz > 0)
      {
        // Read default hotel price and currency.
        auto obj = json_array_get(root.get(), 0);
        my::FromJSONObject(obj, "min_price", minPrice);
        my::FromJSONObject(obj, "currency_code", priceCurrency);

        // Try to get price in requested currency.
        if (!currency.empty() && priceCurrency != currency)
        {
          json_t * arr = json_object_get(obj, "other_currency");
          if (arr && json_is_array(arr))
          {
            size_t sz = json_array_size(arr);
            for (size_t i = 0; i < sz; ++i)
            {
              auto el = json_array_get(arr, i);
              string code;
              my::FromJSONObject(el, "currency_code", code);
              if (code == currency)
              {
                priceCurrency = code;
                my::FromJSONObject(el, "min_price", minPrice);
                break;
              }
            }
          }
        }
      }
    }
    catch (my::Json::Exception const & e)
    {
      LOG(LERROR, (e.Msg()));
      minPrice.clear();
      priceCurrency.clear();
    }
    fn(minPrice, priceCurrency);
    m_minPriceRequest.reset();
  };

  m_minPriceRequest.reset(downloader::HttpRequest::Get(url, callback));
}

void BookingApi::GetBookingDetails(system_clock::time_point const & timestampBegin,
                                   system_clock::time_point const & timestampEnd,
                                   vector<string> const & hotelIds,
                                   function<void(vector<Details> const & details, bool success)> const & fn)
{
  if (fn == nullptr)
    return;

  if (hotelIds.empty())
  {
    fn({}, false /* success */);
    return;
  }

  int constexpr kBufferSize = 32;
  string const dateBegin = strings::FromTimePoint<kBufferSize>(timestampBegin, kRequestTimestampFormat);
  string const dateEnd = strings::FromTimePoint<kBufferSize>(timestampEnd, kRequestTimestampFormat);
  string const hotelIdsStr = strings::JoinStrings(hotelIds, ",");
  string url = MakeApiUrl(kServerUrl, "q", {{"after", dateBegin},
                                            {"before", dateEnd},
                                            {"hotel_ids", hotelIdsStr}});
  auto const callback = [this, fn](downloader::HttpRequest & answer)
  {
    vector<Details> details;
    bool success = true;
    try
    {
      my::Json root(answer.Data().c_str());
      if (!json_is_array(root.get()))
        MYTHROW(my::Json::Exception, ("The answer must contain a json array."));
      size_t const sz = json_array_size(root.get());
      json_int_t intVal;
      string strVal;
      double lat, lon;
      details.reserve(sz);
      for (size_t i = 0; i < sz; i++)
      {
        Details bookingDetails;
        auto obj = json_array_get(root.get(), i);

        my::FromJSONObject(obj, "hotel_id", intVal);
        bookingDetails.m_hotelId = strings::to_string(intVal);

        my::FromJSONObjectOptionalField(obj, "booking_id", intVal);
        if (intVal != 0)
          bookingDetails.m_bookingId = strings::to_string(intVal);

        my::FromJSONObjectOptionalField(obj, "arrival_date", strVal);
        if (!strVal.empty())
        {
          bookingDetails.m_arrivalDate = strings::ToTimePoint(strVal, kResponseDateFormat);
          bookingDetails.hasArrivalDate = true;
        }

        my::FromJSONObjectOptionalField(obj, "departure_date", strVal);
        if (!strVal.empty())
        {
          bookingDetails.m_departureDate = strings::ToTimePoint(strVal, kResponseDateFormat);
          bookingDetails.hasDepartureDate = true;
        }

        my::FromJSONObject(obj, "booking_datetime", strVal);
        bookingDetails.m_bookingTimestamp = strings::ToTimePoint(strVal, kResponseTimestampFormat);

        my::FromJSONObject(obj, "lat", lat);
        my::FromJSONObject(obj, "lon", lon);
        bookingDetails.m_point = MercatorBounds::FromLatLon(lat, lon);

        details.push_back(move(bookingDetails));
      }
    }
    catch (my::Json::Exception const & e)
    {
      LOG(LWARNING, (e.Msg()));
      success = false;
    }

    fn(details, success);
    m_bookingDetailsRequest.reset();
  };

  m_bookingDetailsRequest.reset(downloader::HttpRequest::Get(url, callback));
}

string BookingApi::MakeApiUrl(string const & url, string const & func,
                              initializer_list<pair<string, string>> const & params)
{
  stringstream ss;
  ss << url << func << "?";
  bool firstRun = true;
  for (auto const & param : params)
    ss << (firstRun ? firstRun = false, "" : "&") << param.first << "=" << param.second;

  return ss.str();
}
