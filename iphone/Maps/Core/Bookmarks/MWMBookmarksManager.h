#import "MWMBookmarksObserver.h"
#import "MWMTypes.h"
#import "MWMCatalogCommon.h"

@class MWMCatalogCategory;

NS_ASSUME_NONNULL_BEGIN
@interface MWMBookmarksManager : NSObject

+ (MWMBookmarksManager *)sharedManager;

- (void)addObserver:(id<MWMBookmarksObserver>)observer;
- (void)removeObserver:(id<MWMBookmarksObserver>)observer;

- (BOOL)areBookmarksLoaded;
- (void)loadBookmarks;

- (MWMGroupIDCollection)groupsIdList;
- (NSString *)getCategoryName:(MWMMarkGroupID)groupId;
- (uint64_t)getCategoryMarksCount:(MWMMarkGroupID)groupId;
- (uint64_t)getCategoryTracksCount:(MWMMarkGroupID)groupId;

- (MWMMarkGroupID)createCategoryWithName:(NSString *)name;
- (void)setCategory:(MWMMarkGroupID)groupId name:(NSString *)name;
- (BOOL)isCategoryVisible:(MWMMarkGroupID)groupId;
- (void)setCategory:(MWMMarkGroupID)groupId isVisible:(BOOL)isVisible;
- (void)setUserCategoriesVisible:(BOOL)isVisible;
- (void)setCatalogCategoriesVisible:(BOOL)isVisible;
- (void)deleteCategory:(MWMMarkGroupID)groupId;

- (void)deleteBookmark:(MWMMarkID)bookmarkId;
- (BOOL)checkCategoryName:(NSString *)name;

- (void)shareCategory:(MWMMarkGroupID)groupId;
- (NSURL *)shareCategoryURL;
- (void)finishShareCategory;

- (NSDate * _Nullable)lastSynchronizationDate;
- (BOOL)isCloudEnabled;
- (void)setCloudEnabled:(BOOL)enabled;
- (void)requestRestoring;
- (void)applyRestoring;
- (void)cancelRestoring;

- (NSUInteger)filesCountForConversion;
- (void)convertAll;

- (void)setNotificationsEnabled:(BOOL)enabled;
- (BOOL)areNotificationsEnabled;

- (NSURL * _Nullable)catalogFrontendUrl;
- (NSURL * _Nullable)sharingUrlForCategoryId:(MWMMarkGroupID)groupId;
- (void)downloadItemWithId:(NSString *)itemId
                      name:(NSString *)name
                  progress:(_Nullable ProgressBlock)progress
                completion:(_Nullable CompletionBlock)completion;
- (BOOL)isCategoryFromCatalog:(MWMMarkGroupID)groupId;
- (NSArray<MWMCatalogCategory *> *)categoriesFromCatalog;
- (NSInteger)getCatalogDownloadsCount;
- (BOOL)isCategoryDownloading:(NSString *)itemId;
- (BOOL)hasCategoryDownloaded:(NSString *)itemId;

@end
NS_ASSUME_NONNULL_END
