#include "platform/marketing_service.hpp"

#include "Platform.hpp"

void MarketingService::SendPushWooshTag(std::string const & tag)
{
  SendPushWooshTag(tag, vector<std::string>{"1"});
}

void MarketingService::SendPushWooshTag(std::string const & tag, std::string const & value)
{
  SendPushWooshTag(tag, vector<std::string>{value});
}

void MarketingService::SendPushWooshTag(std::string const & tag, vector<std::string> const & values)
{
  android::Platform::Instance().SendPushWooshTag(tag, values);
}

void MarketingService::SendMarketingEvent(std::string const & tag, map<std::string, std::string> const & params)
{
  android::Platform::Instance().SendMarketingEvent(tag, params);
}
