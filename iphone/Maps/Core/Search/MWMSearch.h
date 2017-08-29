#import "MWMBanner.h"
#import "MWMSearchFilterViewController.h"
#import "MWMSearchItemType.h"
#import "MWMSearchObserver.h"

#include "search/result.hpp"

@interface MWMSearch : NSObject

+ (void)addObserver:(id<MWMSearchObserver>)observer;
+ (void)removeObserver:(id<MWMSearchObserver>)observer;

+ (void)saveQuery:(NSString *)query forInputLocale:(NSString *)inputLocale;
+ (void)searchQuery:(NSString *)query forInputLocale:(NSString *)inputLocale;

+ (void)showResult:(search::Result const &)result;

+ (MWMSearchItemType)resultTypeWithRow:(NSUInteger)row;
+ (NSUInteger)containerIndexWithRow:(NSUInteger)row;
+ (search::Result const &)resultWithContainerIndex:(NSUInteger)index;
+ (BOOL)isLocalAdsWithContainerIndex:(NSUInteger)index;
+ (id<MWMBanner>)adWithContainerIndex:(NSUInteger)index;

+ (void)update;
+ (void)clear;

+ (BOOL)isSearchOnMap;
+ (void)setSearchOnMap:(BOOL)searchOnMap;

+ (NSUInteger)suggestionsCount;
+ (NSUInteger)resultsCount;

+ (BOOL)isHotelResults;

+ (BOOL)hasFilter;
+ (MWMSearchFilterViewController *)getFilter;
+ (void)clearFilter;

- (instancetype)init __attribute__((unavailable("unavailable")));
- (instancetype)copy __attribute__((unavailable("unavailable")));
- (instancetype)copyWithZone:(NSZone *)zone __attribute__((unavailable("unavailable")));
+ (instancetype)alloc __attribute__((unavailable("unavailable")));
+ (instancetype)allocWithZone:(struct _NSZone *)zone __attribute__((unavailable("unavailable")));
+ (instancetype) new __attribute__((unavailable("unavailable")));

@end
