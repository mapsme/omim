#import "MWMMapDownloaderTableViewCell.h"
#import "ViewController.h"

@interface MWMMapCountryDownloaderViewController : ViewController <UITableViewDelegate, UITableViewDataSource>

@property (weak, nonatomic) IBOutlet UILabel * allMapsLabel;

@property (nonatomic) NSMutableDictionary * offscreenCells;

@property (nonatomic) BOOL showAllMapsView;

- (void)configTable;
- (void)configAllMapsView;

@end
