#import "ViewController.h"

@protocol MWMStreetEditorProtocol <NSObject>

- (NSString *)getStreet;
- (void)setStreet:(NSString *)street;

- (NSArray<NSString *> *)getNearbyStreets;

@end

@interface MWMStreetEditorViewController : ViewController

@property (weak, nonatomic) id<MWMStreetEditorProtocol> delegate;

@end
