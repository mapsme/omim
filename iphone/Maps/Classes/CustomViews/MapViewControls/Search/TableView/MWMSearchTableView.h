@interface MWMSearchTableView : UIView

@property (weak, nonatomic) IBOutlet UITableView * _Nullable tableView;

@property (weak, nonatomic) IBOutlet UIView * _Nullable noResultsView;
@property (weak, nonatomic) IBOutlet UIImageView * _Nullable noResultsImage;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable noResultsText;

@end
