#import "MWMMapViewControlsManager.h"

#include "Framework.h"

@class MWMViewController;

@protocol MWMActionBarProtocol<NSObject>

- (void)routeFrom;
- (void)routeTo;

- (void)share;

- (void)addBookmark;
- (void)removeBookmark;

- (void)call;
- (void)book:(BOOL)isDecription;

- (void)apiBack;
- (void)downloadSelectedArea;

@end

@protocol MWMPlacePageButtonsProtocol<NSObject>

- (void)editPlace;
- (void)addPlace;
- (void)addBusiness;
- (void)book:(BOOL)isDescription;
- (void)editBookmark;
- (void)taxiTo;

@end

struct FeatureID;

@protocol MWMFeatureHolder<NSObject>

- (FeatureID const &)featureId;

@end

@protocol MWMPlacePageProtocol<MWMActionBarProtocol, MWMPlacePageButtonsProtocol, MWMFeatureHolder>

@property(nonatomic) CGFloat topBound;
@property(nonatomic) CGFloat leftBound;

- (void)show:(place_page::Info const &)info;
- (void)close;
- (void)mwm_refreshUI;
- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)orientation;
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator;
- (void)addSubviews:(NSArray *)views withNavigationController:(UINavigationController *)controller;

@end
