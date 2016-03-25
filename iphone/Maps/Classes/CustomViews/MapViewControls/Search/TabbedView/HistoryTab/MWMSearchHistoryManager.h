#import "MWMSearchTabbedCollectionViewCell.h"
#import "MWMSearchTabbedViewProtocol.h"

@interface MWMSearchHistoryManager : NSObject <UITableViewDataSource, UITableViewDelegate>

@property (weak, nonatomic) id<MWMSearchTabbedViewProtocol> _Nullable delegate;

- (void)attachCell:(MWMSearchTabbedCollectionViewCell *)cell;
- (BOOL)isRouteSearchMode;

@end
