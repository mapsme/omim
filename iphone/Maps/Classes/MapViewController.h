#import "MWMMapDownloaderMode.h"
#import "MWMViewController.h"
#import "MWMMyPositionMode.h"
#import "MWMUTM.h"

@class MWMWelcomePageController;
@class MWMMapViewControlsManager;
@class MWMAPIBar;
@class MWMPlacePageData;
@class EAGLView;
@class MWMMapDownloadDialog;
@protocol MWMLocationModeListener;

@interface MapViewController : MWMViewController

+ (MapViewController *)sharedController;
- (void)addListener:(id<MWMLocationModeListener>)listener;
- (void)removeListener:(id<MWMLocationModeListener>)listener;

// called when app is terminated by system
- (void)onTerminate;
- (void)onGetFocus:(BOOL)isOnFocus;

- (void)updateStatusBarStyle;

- (void)performAction:(NSString *)action;

- (void)openMigration;
- (void)openBookmarks;
- (void)openMapsDownloader:(MWMMapDownloaderMode)mode;
- (void)openEditor;
- (void)openHotelFacilities;
- (void)openBookmarkEditorWithData:(MWMPlacePageData *)data;
- (void)openFullPlaceDescriptionWithHtml:(NSString *)htmlString;
- (void)showUGCAuth;
- (void)showBookmarksLoadedAlert:(UInt64)categoryId;
- (void)openCatalogAnimated:(BOOL)animated utm:(MWMUTM)utm;
- (void)openCatalogDeeplink:(NSURL *)deeplinkUrl animated:(BOOL)animated utm:(MWMUTM)utm;
- (void)openCatalogAbsoluteUrl:(NSURL *)url animated:(BOOL)animated utm:(MWMUTM)utm;
- (void)searchText:(NSString *)text;
- (void)openDrivingOptions;

- (void)showRemoveAds;
- (void)setPlacePageTopBound:(CGFloat)bound;

+ (void)setViewport:(double)lat lon:(double)lon zoomLevel:(int)zoomlevel;

- (void)initialize;
- (void)enableCarPlayRepresentation;
- (void)disableCarPlayRepresentation;

@property(nonatomic, readonly) MWMMapViewControlsManager * controlsManager;
@property(nonatomic) MWMAPIBar * apiBar;
@property(nonatomic) MWMWelcomePageController * welcomePageController;
@property(nonatomic, readonly) MWMMapDownloadDialog * downloadDialog;


@property(nonatomic) MWMMyPositionMode currentPositionMode;
@property(strong, nonatomic) IBOutlet EAGLView * _Nonnull mapView;
@property(strong, nonatomic) IBOutlet UIView * _Nonnull controlsView;

@end
