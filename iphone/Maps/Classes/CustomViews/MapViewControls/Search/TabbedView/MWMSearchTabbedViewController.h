#import "MWMSearchTabbedViewProtocol.h"
#import "MWMSearchTabButtonsView.h"
#import "MWMViewController.h"

@interface MWMSearchTabbedViewController : MWMViewController

@property (copy, nonatomic) NSArray * tabButtons;
@property (weak, nonatomic) NSLayoutConstraint * _Nullable scrollIndicatorOffset;
@property (weak, nonatomic) UIView * _Nullable scrollIndicator;
@property (weak, nonatomic) id<MWMSearchTabbedViewProtocol> _Nullable delegate;

- (void)tabButtonPressed:(MWMSearchTabButtonsView *)sender;
- (void)resetSelectedTab;

@end
