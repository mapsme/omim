#import "MWMBookmarksObserver.h"
#import "MWMTypes.h"
#import "MWMCatalogCommon.h"
#import "MWMUTM.h"

@class MWMCategory;
@class MWMCarPlayBookmarkObject;
@class MWMTagGroup;
@class MWMTag;

NS_ASSUME_NONNULL_BEGIN

typedef void (^LoadTagsCompletionBlock)(NSArray<MWMTagGroup *> * _Nullable tags, NSInteger maxTagsNumber);

@interface MWMBookmarksManager : NSObject

+ (MWMBookmarksManager *)sharedManager;

- (void)addObserver:(id<MWMBookmarksObserver>)observer;
- (void)removeObserver:(id<MWMBookmarksObserver>)observer;

- (BOOL)areBookmarksLoaded;
- (void)loadBookmarks;

- (BOOL)isCategoryEditable:(MWMMarkGroupID)groupId;
- (BOOL)isCategoryNotEmpty:(MWMMarkGroupID)groupId;
- (NSString *)getCategoryName:(MWMMarkGroupID)groupId;
- (uint64_t)getCategoryMarksCount:(MWMMarkGroupID)groupId;
- (uint64_t)getCategoryTracksCount:(MWMMarkGroupID)groupId;
- (MWMCategoryAccessStatus)getCategoryAccessStatus:(MWMMarkGroupID)groupId;
- (NSString *)getCategoryDescription:(MWMMarkGroupID)groupId;
- (NSString *)getCategoryAuthorName:(MWMMarkGroupID)groupId;

- (MWMMarkGroupID)createCategoryWithName:(NSString *)name;
- (void)setCategory:(MWMMarkGroupID)groupId name:(NSString *)name;
- (void)setCategory:(MWMMarkGroupID)groupId description:(NSString *)name;
- (BOOL)isCategoryVisible:(MWMMarkGroupID)groupId;
- (void)setCategory:(MWMMarkGroupID)groupId isVisible:(BOOL)isVisible;
- (void)setUserCategoriesVisible:(BOOL)isVisible;
- (void)setCatalogCategoriesVisible:(BOOL)isVisible;
- (void)deleteCategory:(MWMMarkGroupID)groupId;
- (BOOL)checkCategoryName:(NSString *)name;

- (NSArray<MWMCarPlayBookmarkObject *> *)bookmarksForCategory:(MWMMarkGroupID)categoryId;
- (void)deleteBookmark:(MWMMarkID)bookmarkId;

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

- (NSURL * _Nullable)catalogFrontendUrl:(MWMUTM)utm;
- (NSURL * _Nullable)catalogFrontendUrlPlusPath:(NSString *)path
                                            utm:(MWMUTM)utm;
- (NSURL * _Nullable)sharingUrlForCategoryId:(MWMMarkGroupID)groupId;
- (NSURL * _Nullable)webEditorUrlForCategoryId:(MWMMarkGroupID)groupId;
- (void)downloadItemWithId:(NSString *)itemId
                      name:(NSString *)name
                  progress:(_Nullable ProgressBlock)progress
                completion:(_Nullable DownloadCompletionBlock)completion;
- (BOOL)isCategoryFromCatalog:(MWMMarkGroupID)groupId;
- (NSArray<MWMCategory *> *)userCategories;
- (NSArray<MWMCategory *> *)categoriesFromCatalog;
- (MWMCategory *)categoryWithId:(MWMMarkGroupID)groupId;
- (NSInteger)getCatalogDownloadsCount;
- (BOOL)isCategoryDownloading:(NSString *)itemId;
- (BOOL)hasCategoryDownloaded:(NSString *)itemId;

- (void)loadTags:(LoadTagsCompletionBlock)completionBlock;
- (void)setCategory:(MWMMarkGroupID)groupId tags:(NSArray<MWMTag *> *)tags;
- (void)setCategory:(MWMMarkGroupID)groupId authorType:(MWMCategoryAuthorType)author;

- (void)uploadAndGetDirectLinkCategoryWithId:(MWMMarkGroupID)itemId
                                    progress:(_Nullable ProgressBlock)progress
                                  completion:(UploadCompletionBlock)completion;

- (void)uploadAndPublishCategoryWithId:(MWMMarkGroupID)itemId
                              progress:(_Nullable ProgressBlock)progress
                            completion:(UploadCompletionBlock)completion;

- (void)uploadCategoryWithId:(MWMMarkGroupID)itemId
                    progress:(_Nullable ProgressBlock)progress
                  completion:(UploadCompletionBlock)completion;

- (instancetype)init __attribute__((unavailable("call +manager instead")));
- (instancetype)copy __attribute__((unavailable("call +manager instead")));
- (instancetype)copyWithZone:(NSZone *)zone __attribute__((unavailable("call +manager instead")));
+ (instancetype)allocWithZone:(struct _NSZone *)zone
__attribute__((unavailable("call +manager instead")));
+ (instancetype) new __attribute__((unavailable("call +manager instead")));

@end
NS_ASSUME_NONNULL_END
