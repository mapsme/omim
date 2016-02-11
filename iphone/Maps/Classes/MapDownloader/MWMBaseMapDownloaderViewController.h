#import "MWMMapDownloaderTableViewCell.h"
#import "ViewController.h"

#include "storage/index.hpp"

@interface MWMBaseMapDownloaderViewController : ViewController <UITableViewDelegate>

@property (weak, nonatomic) IBOutlet UILabel * allMapsLabel;

@property (nonatomic) NSMutableDictionary * offscreenCells;

@property (nonatomic) BOOL showAllMapsView;

@property (nonatomic) storage::TCountryId parentCountryId;

- (void)configTable;
- (void)configAllMapsView;

@end
