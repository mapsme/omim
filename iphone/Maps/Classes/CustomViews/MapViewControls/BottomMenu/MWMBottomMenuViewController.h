#import "MWMBottomMenuView.h"

#include "platform/location.hpp"

@class MapViewController, MWMButton;

@protocol MWMBottomMenuControllerProtocol<NSObject>

- (void)actionDownloadMaps;
- (void)closeInfoScreens;
- (void)addPlace;
- (void)didFinishAddingPlace;

@end

@interface MWMBottomMenuViewController : UIViewController

@property(nonatomic) MWMBottomMenuState state;
@property(weak, nonatomic) IBOutlet MWMButton * _Nullable p2pButton;
@property(nonatomic) CGFloat leftBound;

- (instancetype _Nullable)initWithParentController:(MapViewController *)controller
                                delegate:(id<MWMBottomMenuControllerProtocol>)delegate;

- (void)setStreetName:(NSString *)streetName;
- (void)setInactive;
- (void)setPlanning;
- (void)setGo;
- (void)mwm_refreshUI;
- (void)refreshLayout;

- (void)processMyPositionStateModeEvent:(location::EMyPositionMode)mode;

@end
