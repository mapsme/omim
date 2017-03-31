#import "MWMCoreBanner.h"
#import "SwiftBridge.h"

#include "partners_api/banner.hpp"

namespace banner_helpers
{
static inline MWMBannerType MatchBannerType(ads::Banner::Type coreType)
{
  switch (coreType)
  {
  case ads::Banner::Type::None: return MWMBannerTypeNone;
  case ads::Banner::Type::Facebook: return MWMBannerTypeFacebook;
  case ads::Banner::Type::RB: return MWMBannerTypeRb;
  }
}

static inline MWMCoreBanner * MatchBanner(ads::Banner const & banner)
{
  return [[MWMCoreBanner alloc] initWith:MatchBannerType(banner.m_type)
                                bannerID:@(banner.m_bannerId.c_str())];
}

static inline NSArray<MWMCoreBanner *> * MatchPriorityBanners(vector<ads::Banner> const & banners)
{
  NSMutableArray<MWMCoreBanner *> * mBanners = [@[] mutableCopy];
  for (auto const & banner : banners)
    [mBanners addObject:MatchBanner(banner)];
  return [mBanners copy];
}
}
