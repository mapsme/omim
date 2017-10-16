#import "MWMUGCViewModel.h"
#import "MWMPlacePageData.h"
#import "SwiftBridge.h"

#include "ugc/types.hpp"

using namespace place_page;

namespace
{
NSArray<MWMUGCRatingStars *> * starsRatings(ugc::Ratings const & ratings)
{
  NSMutableArray<MWMUGCRatingStars *> * mwmRatings = [@[] mutableCopy];
  for (auto const & rec : ratings)
    [mwmRatings addObject:[[MWMUGCRatingStars alloc] initWithTitle:@(rec.m_key.m_key.c_str())
                                                             value:rec.m_value
                                                          maxValue:5]];
  return [mwmRatings copy];
}

MWMUGCRatingValueType * ratingValueType(float rating)
{
  return [[MWMUGCRatingValueType alloc]
      initWithValue:@(rating::GetRatingFormatted(rating).c_str())
               type:[MWMPlacePageData ratingValueType:rating::GetImpress(rating)]];
}
}  // namespace

@interface MWMUGCViewModel ()
@property(copy, nonatomic) MWMVoidBlock refreshCallback;
@end

@implementation MWMUGCViewModel
{
  place_page::Info m_info;
  ugc::UGC m_ugc;
  ugc::UGCUpdate m_ugcUpdate;
}

- (instancetype)initWithUGC:(ugc::UGC const &)ugc update:(ugc::UGCUpdate const &)update
{
  self = [super init];
  if (self)
  {
    m_ugc = ugc;
    m_ugcUpdate = update;
  }
  return self;
}

- (BOOL)isUGCEmpty { return static_cast<BOOL>(m_ugc.IsEmpty()); }
- (BOOL)isUGCUpdateEmpty { return static_cast<BOOL>(m_ugcUpdate.IsEmpty()); }
- (NSUInteger)ratingCellsCount { return 1; }
- (NSUInteger)addReviewCellsCount { return 1; }
- (NSUInteger)totalReviewsCount { return static_cast<NSUInteger>(m_ugc.m_basedOn); }
- (MWMUGCRatingValueType *)summaryRating { return ratingValueType(m_ugc.m_totalRating); }
- (NSArray<MWMUGCRatingStars *> *)ratings { return starsRatings(m_ugc.m_ratings); }

#pragma mark - MWMReviewsViewModelProtocol

- (NSUInteger)numberOfReviews { return m_ugc.m_reviews.size() + !self.isUGCUpdateEmpty; }

- (id<MWMReviewProtocol> _Nonnull)reviewWithIndex:(NSUInteger)index
{
  auto idx = index;
  NSAssert(idx >= 0, @"Invalid index");
  if (!self.isUGCUpdateEmpty)
  {
    if (idx == 0)
    {
      auto const & review = m_ugcUpdate;
      return [[MWMUGCYourReview alloc]
          initWithDate:[NSDate
                           dateWithTimeIntervalSince1970:review.m_time.time_since_epoch().count()]
                  text:@(review.m_text.m_text.c_str())
               ratings:starsRatings(review.m_ratings)];
    }
    idx -= 1;
  }
  NSAssert(idx < m_ugc.m_reviews.size(), @"Invalid index");
  auto const & review = m_ugc.m_reviews[idx];
  return [[MWMUGCReview alloc]
      initWithTitle:@(review.m_author.c_str())
               date:[NSDate dateWithTimeIntervalSince1970:review.m_time.time_since_epoch().count()]
               text:@(review.m_text.m_text.c_str())
             rating:ratingValueType(review.m_rating)];
}

@end
