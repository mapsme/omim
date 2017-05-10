#pragma once

#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <memory>
#include <vector>

namespace ms
{
class LatLon;
}

namespace downloader
{
class HttpRequest;
}

namespace uber
{
// Uber api wrapper based on synchronous http requests.
class RawApi
{
public:
  /// Returns true when http request was executed successfully and response copied into @result,
  /// otherwise returns false. The response contains the display name and other details about each
  /// product, and lists the products in the proper display order. This endpoint does not reflect
  /// real-time availability of the products.
  static bool GetProducts(ms::LatLon const & pos, std::string & result);
  /// Returns true when http request was executed successfully and response copied into @result,
  /// otherwise returns false. The response contains ETAs for all products currently available
  /// at a given location, with the ETA for each product expressed as integers in seconds. If a
  /// product returned from GetProducts is not returned from this endpoint for a given
  /// latitude/longitude pair then there are currently none of that product available to request.
  /// Call this endpoint every minute to provide the most accurate, up-to-date ETAs.
  static bool GetEstimatedTime(ms::LatLon const & pos, std::string & result);
  /// Returns true when http request was executed successfully and response copied into @result,
  /// otherwise returns false. The response contains an estimated price range for each product
  /// offered at a given location. The price estimate is provided as a formatted string with the
  /// full price range and the localized currency symbol.
  static bool GetEstimatedPrice(ms::LatLon const & from, ms::LatLon const & to, std::string & result);
};

struct Product
{
  std::string m_productId;
  std::string m_name;
  std::string m_time;
  std::string m_price;     // for some currencies this field contains symbol of currency but not always
  std::string m_currency;  // currency can be empty, for ex. when m_price equal to Metered
};
/// @products - vector of available products for requested route, cannot be empty.
/// @requestId - identificator which was provided to GetAvailableProducts to identify request.
using ProductsCallback = std::function<void(std::vector<Product> const & products, uint64_t const requestId)>;

enum class ErrorCode
{
  NoProducts,
  RemoteError
};

/// Callback which is called when an errors occurs.
using ErrorCallback = std::function<void(ErrorCode const code, uint64_t const requestId)>;

/// Class which used for making products from http requests results.
class ProductMaker
{
public:
  void Reset(uint64_t const requestId);
  void SetTimes(uint64_t const requestId, std::string const & times);
  void SetPrices(uint64_t const requestId, std::string const & prices);
  void MakeProducts(uint64_t const requestId, ProductsCallback const & successFn,
                    ErrorCallback const & errorFn);

private:
  uint64_t m_requestId = 0;
  std::unique_ptr<std::string> m_times;
  std::unique_ptr<std::string> m_prices;
  std::mutex m_mutex;
};

struct RideRequestLinks
{
  std::string m_deepLink;
  std::string m_universalLink;
};

class Api
{
public:
  /// Requests list of available products from Uber. Returns request identificator immediately.
  uint64_t GetAvailableProducts(ms::LatLon const & from, ms::LatLon const & to,
                                ProductsCallback const & successFn, ErrorCallback const & errorFn);

  /// Returns link which allows you to launch the Uber app.
  static RideRequestLinks GetRideRequestLinks(std::string const & productId, ms::LatLon const & from,
                                              ms::LatLon const & to);

private:
  std::shared_ptr<ProductMaker> m_maker = std::make_shared<ProductMaker>();
  uint64_t m_requestId = 0;
};

void SetUberUrlForTesting(std::string const & url);
std::string DebugPrint(ErrorCode error);
}  // namespace uber
