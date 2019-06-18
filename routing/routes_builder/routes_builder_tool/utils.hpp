#pragma once

#include "routing/routing_quality/api/api.hpp"

#include "routing/routes_builder/routes_builder.hpp"

#include <string>
#include <vector>
#include <memory>

namespace routing
{
namespace routes_builder
{
std::vector<RoutesBuilder::Result> BuildRoutes(std::string const & routesPath,
                                               std::string const & dumpPath);

std::vector<routing_quality::api::Response>
BuildRoutesWithApi(std::unique_ptr<routing_quality::api::RoutingApi> routingApi,
                   std::string const & routesPath,
                   std::string const & dumpPath);

std::unique_ptr<routing_quality::api::RoutingApi> CreateRoutingApi(std::string const & name);
}  // namespace routes_builder
}  // routing
