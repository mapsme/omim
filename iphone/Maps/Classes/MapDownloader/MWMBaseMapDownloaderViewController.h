#import "MWMMapDownloaderTableViewCell.h"
#import "ViewController.h"

#include "storage/index.hpp"

@interface MWMBaseMapDownloaderViewController : ViewController <UITableViewDelegate, UITableViewDataSource>

@property (weak, nonatomic) IBOutlet UILabel * allMapsLabel;

@property (nonatomic) NSMutableDictionary * offscreenCells;

@property (nonatomic) BOOL showAllMapsView;

- (void)configTable;
- (void)configAllMapsView;

- (storage::TCountryId)GetRootCountryId;
- (void)SetRootCountryId:(storage::TCountryId)rootId;

@end
