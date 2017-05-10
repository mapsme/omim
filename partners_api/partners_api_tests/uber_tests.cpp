#include "testing/testing.hpp"

#include "partners_api/uber_api.hpp"

#include "geometry/latlon.hpp"

#include "base/scope_guard.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>

#include "3party/jansson/myjansson.hpp"

namespace
{
bool IsComplete(uber::Product const & product)
{
  return !product.m_productId.empty() && !product.m_name.empty() && !product.m_time.empty() &&
         !product.m_price.empty();
}
}  // namespace

UNIT_TEST(Uber_GetProducts)
{
  ms::LatLon const pos(38.897724, -77.036531);
  std::string result;
  TEST(uber::RawApi::GetProducts(pos, result), ());
  TEST(!result.empty(), ());
}

UNIT_TEST(Uber_GetTimes)
{
  ms::LatLon const pos(38.897724, -77.036531);
  std::string result;

  TEST(uber::RawApi::GetEstimatedTime(pos, result), ());
  TEST(!result.empty(), ());

  my::Json timeRoot(result.c_str());
  auto const timesArray = json_object_get(timeRoot.get(), "times");

  TEST(json_is_array(timesArray), ());
  TEST_GREATER(json_array_size(timesArray), 0, ());

  auto const timeSize = json_array_size(timesArray);
  for (size_t i = 0; i < timeSize; ++i)
  {
    std::string name;
    int64_t estimatedTime = 0;
    auto const item = json_array_get(timesArray, i);

    try
    {
      FromJSONObject(item, "display_name", name);
      FromJSONObject(item, "estimate", estimatedTime);
    }
    catch (my::Json::Exception const & e)
    {
      TEST(false, (e.Msg()));
    }

    std::string const estimated = strings::to_string(estimatedTime);

    TEST(!name.empty(), ());
    TEST(!estimated.empty(), ());
  }
}

UNIT_TEST(Uber_GetPrices)
{
  ms::LatLon const from(38.897724, -77.036531);
  ms::LatLon const to(38.862416, -76.883316);
  std::string result;

  TEST(uber::RawApi::GetEstimatedPrice(from, to, result), ());
  TEST(!result.empty(), ());

  my::Json priceRoot(result.c_str());
  auto const pricesArray = json_object_get(priceRoot.get(), "prices");

  TEST(json_is_array(pricesArray), ());
  TEST_GREATER(json_array_size(pricesArray), 0, ());

  auto const pricesSize = json_array_size(pricesArray);
  for (size_t i = 0; i < pricesSize; ++i)
  {
    std::string productId;
    std::string price;
    std::string currency;

    auto const item = json_array_get(pricesArray, i);

    try
    {
      FromJSONObject(item, "product_id", productId);
      FromJSONObject(item, "estimate", price);

      auto const val = json_object_get(item, "currency_code");
      if (val != nullptr)
      {
        if (!json_is_null(val))
          currency = json_string_value(val);
        else
          currency = "null";
      }
    }
    catch (my::Json::Exception const & e)
    {
      TEST(false, (e.Msg()));
    }

    TEST(!productId.empty(), ());
    TEST(!price.empty(), ());
    TEST(!currency.empty(), ());
  }
}

UNIT_TEST(Uber_ProductMaker)
{
  size_t reqId = 1;
  ms::LatLon const from(38.897724, -77.036531);
  ms::LatLon const to(38.862416, -76.883316);

  size_t returnedId = 0;
  std::vector<uber::Product> returnedProducts;

  uber::ProductMaker maker;

  std::string times;
  std::string prices;

  auto const errorCallback = [](uber::ErrorCode const code, uint64_t const requestId)
  {
    TEST(false, ());
  };

  TEST(uber::RawApi::GetEstimatedTime(from, times), ());
  TEST(uber::RawApi::GetEstimatedPrice(from, to, prices), ());

  maker.Reset(reqId);
  maker.SetTimes(reqId, times);
  maker.SetPrices(reqId, prices);
  maker.MakeProducts(reqId,
                     [&returnedId, &returnedProducts](std::vector<uber::Product> const & products,
                                                      size_t const requestId) {
                       returnedId = requestId;
                       returnedProducts = products;
                     },
                     errorCallback);

  TEST(!returnedProducts.empty(), ());
  TEST_EQUAL(returnedId, reqId, ());

  for (auto const & product : returnedProducts)
    TEST(IsComplete(product), ());

  ++reqId;

  TEST(uber::RawApi::GetEstimatedTime(from, times), ());
  TEST(uber::RawApi::GetEstimatedPrice(from, to, prices), ());

  maker.Reset(reqId);
  maker.SetTimes(reqId, times);
  maker.SetPrices(reqId, prices);

  maker.MakeProducts(reqId + 1, [](std::vector<uber::Product> const & products, size_t const requestId)
  {
    TEST(false, ());
  }, errorCallback);
}

UNIT_TEST(Uber_Smoke)
{
  // Used to synchronize access into GetAvailableProducts callback method.
  std::mutex resultsMutex;
  size_t reqId = 1;
  std::vector<uber::Product> productsContainer;
  ms::LatLon const from(38.897724, -77.036531);
  ms::LatLon const to(38.862416, -76.883316);

  uber::SetUberUrlForTesting("http://localhost:34568/partners");
  MY_SCOPE_GUARD(cleanup, []() { uber::SetUberUrlForTesting(""); });

  auto const errorCallback = [](uber::ErrorCode const code, uint64_t const requestId)
  {
    TEST(false, ());
  };

  auto const errorPossibleCallback = [](uber::ErrorCode const code, uint64_t const requestId)
  {
    TEST(code == uber::ErrorCode::NoProducts, ());
  };

  auto const standardCallback =
      [&reqId, &productsContainer, &resultsMutex](std::vector<uber::Product> const & products, size_t const requestId)
  {
    std::lock_guard<std::mutex> lock(resultsMutex);

    if (reqId == requestId)
      productsContainer = products;
  };

  auto const lastCallback =
      [&standardCallback](std::vector<uber::Product> const & products, size_t const requestId)
  {
    standardCallback(products, requestId);
    testing::StopEventLoop();
  };

  std::string times;
  std::string prices;

  TEST(uber::RawApi::GetEstimatedTime(from, times), ());
  TEST(uber::RawApi::GetEstimatedPrice(from, to, prices), ());

  uber::ProductMaker maker;

  maker.Reset(reqId);
  maker.SetTimes(reqId, times);
  maker.SetPrices(reqId, prices);
  maker.MakeProducts(reqId, standardCallback, errorCallback);

  reqId = 0;

  auto const synchronousProducts = productsContainer;
  productsContainer.clear();

  {
    uber::Api uberApi;

    {
      std::lock_guard<std::mutex> lock(resultsMutex);
      reqId = uberApi.GetAvailableProducts(ms::LatLon(55.753960, 37.624513),
                                           ms::LatLon(55.765866, 37.661270), standardCallback,
                                           errorPossibleCallback);
    }
    {
      std::lock_guard<std::mutex> lock(resultsMutex);
      reqId = uberApi.GetAvailableProducts(ms::LatLon(59.922445, 30.367201),
                                           ms::LatLon(59.943675, 30.361123), standardCallback,
                                           errorPossibleCallback);
    }
    {
      std::lock_guard<std::mutex> lock(resultsMutex);
      reqId = uberApi.GetAvailableProducts(ms::LatLon(52.509621, 13.450067),
                                           ms::LatLon(52.510811, 13.409490), standardCallback,
                                           errorPossibleCallback);
    }
    {
      std::lock_guard<std::mutex> lock(resultsMutex);
      reqId = uberApi.GetAvailableProducts(from, to, lastCallback, errorCallback);
    }
  }

  testing::RunEventLoop();

  TEST_EQUAL(synchronousProducts.size(), productsContainer.size(), ());

  auto const isEqual =
      std::equal(synchronousProducts.begin(), synchronousProducts.end(), productsContainer.begin(),
            [](uber::Product const & lhs, uber::Product const & rhs) {
              return lhs.m_productId == rhs.m_productId && lhs.m_name == rhs.m_name &&
                     lhs.m_price == rhs.m_price;
            });
  TEST(isEqual, ());
}
