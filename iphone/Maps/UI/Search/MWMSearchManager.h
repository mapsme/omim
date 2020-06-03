#import "MWMAlertViewController.h"
#import "MWMSearchManagerObserver.h"
#import "MWMSearchManagerState.h"

typedef NS_ENUM(NSInteger, MWMSearchManagerRoutingTooltipSearch) {
  MWMSearchManagerRoutingTooltipSearchNone,
  MWMSearchManagerRoutingTooltipSearchStart,
  MWMSearchManagerRoutingTooltipSearchFinish
};
@class MWMSearchTextField;

@interface MWMSearchManager : NSObject

+ (nonnull MWMSearchManager *)manager NS_SWIFT_NAME(manager());
+ (void)addObserver:(nonnull id<MWMSearchManagerObserver>)observer;
+ (void)removeObserver:(nonnull id<MWMSearchManagerObserver>)observer;

@property(nullable, weak, nonatomic) IBOutlet MWMSearchTextField *searchTextField;

@property(nonatomic) MWMSearchManagerState state;
@property(nonatomic) MWMSearchManagerRoutingTooltipSearch routingTooltipSearch;

@property(nonnull, nonatomic) IBOutletCollection(UIView) NSArray *topViews;

- (void)searchText:(nonnull NSString *)text forInputLocale:(nullable NSString *)locale;

#pragma mark - Layout

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(nonnull id<UIViewControllerTransitionCoordinator>)coordinator;

@end
