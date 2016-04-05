#import "MWMTableViewController.h"

#include "std/string.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

@protocol MWMStreetEditorProtocol <NSObject>

- (string const &)currentStreet;
- (void)setCurrentStreet:(string const &)street;
- (vector<pair<string, string>> const &)nearbyStreets;

@end

@interface MWMStreetEditorViewController : MWMTableViewController

@property (weak, nonatomic) id<MWMStreetEditorProtocol> delegate;

@end
