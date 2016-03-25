#import "MWMViewController.h"

#include "storage/index.hpp"

@class MapViewController;

@interface MWMMapDownloadDialog : UIView

+ (instancetype _Nullable)dialogForController:(MapViewController *)controller;

- (void)processViewportCountryEvent:(storage::TCountryId const &)countryId;

@end
