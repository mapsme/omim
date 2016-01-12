#import "MWMAlert.h"

#include "storage/storage.hpp"
#include "std/vector.hpp"

@interface MWMDownloadTransitMapAlert : MWMAlert

+ (instancetype)downloaderAlertWithMaps:(vector<storage::TCountryId> const &)maps
                                 routes:(vector<storage::TCountryId> const &)routes
                                   code:(routing::IRouter::ResultCode)code;
- (void)showDownloadDetail:(UIButton *)sender;

@end
