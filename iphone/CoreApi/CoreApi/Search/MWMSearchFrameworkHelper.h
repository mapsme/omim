#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MWMSearchFrameworkHelper : NSObject

- (NSArray<NSString *> *)searchCategories;
- (BOOL)hasMegafonCategoryBanner;
- (NSURL *)megafonBannerUrl;
- (nullable NSURL *)citymobilBannerUrl;

- (BOOL)isSearchHistoryEmpty;
- (NSArray<NSString *> *)lastSearchQueries;
- (void)clearSearchHistory;

@end

NS_ASSUME_NONNULL_END
