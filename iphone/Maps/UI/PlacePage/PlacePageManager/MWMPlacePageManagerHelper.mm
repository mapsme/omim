#import "MWMPlacePageManagerHelper.h"
#import "MWMMapViewControlsManager.h"
#import "MWMPlacePageManager.h"

@interface MWMMapViewControlsManager ()

@property(nonatomic) MWMPlacePageManager * placePageManager;

@end

@interface MWMPlacePageManager ()

- (void)updateAvailableArea:(CGRect)frame;
- (void)showUGCAddReview:(PlacePageData *)data rating:(UgcSummaryRatingType)value fromSource:(MWMUGCReviewSource)source;
- (void)searchSimilar:(PlacePageData *)data;
- (void)editPlace;
- (void)addBusiness;
- (void)addPlace:(CLLocationCoordinate2D)coordinate;
- (void)orderTaxi:(PlacePageData *)data;
- (void)openLocalAdsURL:(PlacePageData *)data;
- (void)openWebsite:(PlacePageData *)data;
- (void)call:(PlacePageData *)data;
- (void)showAllFacilities:(PlacePageData *)data;
- (void)showPlaceDescription:(NSString *)htmlString;
- (void)openMoreUrl:(PlacePageData *)data;
- (void)openReviewUrl:(PlacePageData *)data;
- (void)openDescriptionUrl:(PlacePageData *)data;
- (void)openCatalogSingleItem:(PlacePageData *)data atIndex:(NSInteger)index;
- (void)openCatalogMoreItems:(PlacePageData *)data;
- (void)addBookmark:(PlacePageData *)data;
- (void)removeBookmark:(PlacePageData *)data;
- (void)editBookmark:(PlacePageData *)data;
- (void)openPartner:(PlacePageData *)data;
- (void)book:(PlacePageData *)data;
- (void)share:(PlacePageData *)data;
- (void)routeFrom:(PlacePageData *)data;
- (void)routeTo:(PlacePageData *)data;
- (void)routeAddStop:(PlacePageData *)data;
- (void)routeRemoveStop:(PlacePageData *)data;
- (void)avoidDirty;
- (void)avoidFerry;
- (void)avoidToll;
- (void)showRemoveAds;
- (void)openElevationDifficultPopup:(PlacePageData *)data;

@end

@implementation MWMPlacePageManagerHelper

+ (void)updateAvailableArea:(CGRect)frame
{
  [[MWMMapViewControlsManager manager].placePageManager updateAvailableArea:frame];
}

+ (void)showUGCAddReview:(PlacePageData *)data rating:(UgcSummaryRatingType)value fromSource:(MWMUGCReviewSource)source {
  [[MWMMapViewControlsManager manager].placePageManager showUGCAddReview:data rating:value fromSource:source];
}

+ (void)searchSimilar:(PlacePageData *)data
{
  [[MWMMapViewControlsManager manager].placePageManager searchSimilar:data];
}

+ (void)editPlace {
  [[MWMMapViewControlsManager manager].placePageManager editPlace];
}

+ (void)addBusiness {
  [[MWMMapViewControlsManager manager].placePageManager addBusiness];
}

+ (void)addPlace:(CLLocationCoordinate2D)coordinate {
  [[MWMMapViewControlsManager manager].placePageManager addPlace:coordinate];
}

+ (void)orderTaxi:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager orderTaxi:data];
}

+ (void)openLocalAdsURL:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openLocalAdsURL:data];
}

+ (void)openWebsite:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openWebsite:data];
}

+ (void)call:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager call:data];
}

+ (void)showAllFacilities:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager showAllFacilities:data];
}

+ (void)showPlaceDescription:(NSString *)htmlString {
  [[MWMMapViewControlsManager manager].placePageManager showPlaceDescription:htmlString];
}

+ (void)openMoreUrl:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openMoreUrl:data];
}

+ (void)openReviewUrl:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openReviewUrl:data];
}

+ (void)openDescriptionUrl:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openDescriptionUrl:data];
}

+ (void)openCatalogSingleItem:(PlacePageData *)data atIndex:(NSInteger)index {
  [[MWMMapViewControlsManager manager].placePageManager openCatalogSingleItem:data atIndex:index];
}

+ (void)openCatalogMoreItems:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openCatalogMoreItems:data];
}

+ (void)addBookmark:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager addBookmark:data];
}

+ (void)removeBookmark:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager removeBookmark:data];
}

+ (void)editBookmark:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager editBookmark:data];
}

+ (void)openPartner:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openPartner:data];
}

+ (void)book:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager book:data];
}

+ (void)share:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager share:data];
}

+ (void)routeFrom:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager routeFrom:data];
}

+ (void)routeTo:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager routeTo:data];
}

+ (void)routeAddStop:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager routeAddStop:data];
}

+ (void)routeRemoveStop:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager routeRemoveStop:data];
}

+ (void)avoidDirty {
  [[MWMMapViewControlsManager manager].placePageManager avoidDirty];
}

+ (void)avoidFerry {
  [[MWMMapViewControlsManager manager].placePageManager avoidFerry];
}

+ (void)avoidToll {
  [[MWMMapViewControlsManager manager].placePageManager avoidToll];
}

+ (void)showRemoveAds {
  [[MWMMapViewControlsManager manager].placePageManager showRemoveAds];
}

+ (void)openElevationDifficultPopup:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openElevationDifficultPopup:data];
}

@end
