
#import "MWMBottomMenuView.h"

#include "platform/location.hpp"

@class MapViewController;

@protocol MWMBottomMenuControllerProtocol<NSObject>

- (void)actionDownloadMaps;
- (void)closeInfoScreens;

@end

@interface MWMBottomMenuViewController : UIViewController

@property(nonatomic) MWMBottomMenuState state;

@property(nonatomic) CGFloat leftBound;

- (instancetype)initWithParentController:(MapViewController *)controller
                                delegate:(id<MWMBottomMenuControllerProtocol>)delegate;

- (void)onEnterForeground;

- (void)setStreetName:(NSString *)streetName;
- (void)setPlanning;
- (void)setGo;

- (void)onLocationStateModeChanged:(location::EMyPositionMode)state;

@end
