#import "MWMSearchTabbedCollectionViewCell.h"
#import "MWMSearchTabbedViewProtocol.h"

@interface MWMSearchBookmarksManager : NSObject <UITableViewDataSource, UITableViewDelegate>

@property (weak, nonatomic) id<MWMSearchTabbedViewProtocol> _Nullable delegate;

- (void)attachCell:(MWMSearchTabbedCollectionViewCell *)cell;

@end
