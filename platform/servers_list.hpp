#pragma once

#include <vector>
#include <string>


namespace downloader
{
  class HttpRequest;

  void GetServerListFromRequest(HttpRequest const & request, std::vector<std::string> & urls);
}
