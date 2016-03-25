#import "MWMTableViewController.h"

@protocol MWMStreetEditorProtocol <NSObject>

- (NSString *)getStreet;
- (void)setStreet:(NSString *)street;

- (NSArray<NSString *> *)getNearbyStreets;

@end

@interface MWMStreetEditorViewController : MWMTableViewController

@property (weak, nonatomic) id<MWMStreetEditorProtocol> _Nullable delegate;

@end
