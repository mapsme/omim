#import "MWMMapDownloaderMode.h"
#import "MWMViewController.h"

#import <CoreApi/MWMUTM.h>

@class MWMWelcomePageController;
@class MWMMapViewControlsManager;
@class EAGLView;
@class MWMMapDownloadDialog;
@class BookmarksCoordinator;

@interface MapViewController : MWMViewController

+ (MapViewController *)sharedController;
// called when app is terminated by system
- (void)onTerminate;
- (void)onGetFocus:(BOOL)isOnFocus;

- (void)updateStatusBarStyle;

- (void)performAction:(NSString *)action;

- (void)openMapsDownloader:(MWMMapDownloaderMode)mode;
- (void)openEditor;
- (void)openBookmarkEditor;
- (void)openFullPlaceDescriptionWithHtml:(NSString *)htmlString;
- (void)showUGCAuth;
- (void)showBookmarksLoadedAlert:(UInt64)categoryId;
- (void)openCatalogAnimated:(BOOL)animated utm:(MWMUTM)utm;
- (void)openCatalogDeeplink:(NSURL *)deeplinkUrl animated:(BOOL)animated utm:(MWMUTM)utm;
- (void)openCatalogAbsoluteUrl:(NSURL *)url animated:(BOOL)animated utm:(MWMUTM)utm;
- (void)searchText:(NSString *)text;
- (void)openDrivingOptions;

- (void)showRemoveAds;

- (void)initialize;
- (void)enableCarPlayRepresentation;
- (void)disableCarPlayRepresentation;

- (void)dismissPlacePage;

@property(nonatomic, readonly) MWMMapViewControlsManager * controlsManager;
@property(nonatomic) MWMWelcomePageController * welcomePageController;
@property(nonatomic, readonly) MWMMapDownloadDialog * downloadDialog;
@property(nonatomic, readonly) BookmarksCoordinator * bookmarksCoordinator;

@property(nonatomic) MWMMyPositionMode currentPositionMode;
@property(strong, nonatomic) IBOutlet EAGLView * _Nonnull mapView;
@property(strong, nonatomic) IBOutlet UIView * _Nonnull controlsView;
@property(strong, nonatomic) IBOutlet UIView * _Nonnull searchViewContainer;

@end
