#import <CoreLocation/CoreLocation.h>
#import <UIKit/UIKit.h>

#import "MWMTypes.h"

@class MWMMapSearchResult;

typedef NS_ENUM(NSUInteger, MWMZoomMode) { MWMZoomModeIn = 0, MWMZoomModeOut };

typedef NS_ENUM(NSUInteger, MWMMyPositionMode) {
  MWMMyPositionModePendingPosition,
  MWMMyPositionModeNotFollowNoPosition,
  MWMMyPositionModeNotFollow,
  MWMMyPositionModeFollow,
  MWMMyPositionModeFollowAndRotate
} NS_SWIFT_NAME(MyPositionMode);

NS_ASSUME_NONNULL_BEGIN

typedef void (^SearchInDownloaderCompletions)(NSArray<MWMMapSearchResult *> *results, BOOL finished);

NS_SWIFT_NAME(LocationModeListener)
@protocol MWMLocationModeListener
- (void)processMyPositionStateModeEvent:(MWMMyPositionMode)mode;
- (void)processMyPositionPendingTimeout;
@end

NS_SWIFT_NAME(FrameworkHelper)
@interface MWMFrameworkHelper : NSObject

+ (MWMFrameworkHelper *)sharedHelper;
- (void)processFirstLaunch:(BOOL)hasLocation;
- (void)setVisibleViewport:(CGRect)rect scaleFactor:(CGFloat)scale;
- (void)setTheme:(MWMTheme)theme;
- (MWMDayTime)daytimeAtLocation:(nullable CLLocation *)location;
- (void)createFramework;
- (BOOL)canUseNetwork;
- (BOOL)isNetworkConnected;
- (BOOL)isWiFiConnected;
- (MWMMarkID)invalidBookmarkId;
- (MWMMarkGroupID)invalidCategoryId;
- (void)zoomMap:(MWMZoomMode)mode;
- (void)moveMap:(UIOffset)offset;
- (void)deactivateMapSelection:(BOOL)notifyUI NS_SWIFT_NAME(deactivateMapSelection(notifyUI:));
- (void)switchMyPositionMode;
- (void)stopLocationFollow;
- (NSArray<NSString *> *)obtainLastSearchQueries;
- (void)rotateMap:(double)azimuth animated:(BOOL)isAnimated;
- (void)updatePositionArrowOffset:(BOOL)useDefault offset:(int)offsetY;
- (void)uploadUGC:(void (^)(UIBackgroundFetchResult))completionHandler;
- (NSString *)userAccessToken;
- (NSString *)userAgent;
- (int64_t)dataVersion;
- (void)searchInDownloader:(NSString *)query
               inputLocale:(NSString *)locale
                completion:(SearchInDownloaderCompletions)completion;
- (BOOL)canEditMap;
- (void)showOnMap:(MWMMarkGroupID)categoryId;
- (void)showBookmark:(MWMMarkID)bookmarkId;
- (void)showTrack:(MWMTrackID)trackId;
- (void)updatePlacePageData;
- (void)setPlacePageSelectedCallback:(MWMVoidBlock)selected
                  deselectedCallback:(MWMBoolBlock)deselected
                     updatedCallback:(MWMVoidBlock)updated;

- (void)addLocationModeListener:(id<MWMLocationModeListener>)listener NS_SWIFT_NAME(addLocationModeListener(_:));
- (void)removeLocationModeListener:(id<MWMLocationModeListener>)listener NS_SWIFT_NAME(removeLocationModeListener(_:));
- (void)setViewportCenter:(CLLocationCoordinate2D)center zoomLevel:(int)zoomLevelb;

@end

NS_ASSUME_NONNULL_END
