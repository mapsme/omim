#import "MWMAlert.h"

#include "storage/storage.hpp"
#include "std/vector.hpp"

@interface MWMDownloadTransitMapAlert : MWMAlert

+ (instancetype)downloaderAlertWithMaps:(storage::TCountriesVec const &)maps
                                 routes:(storage::TCountriesVec const &)routes
                                   code:(routing::IRouter::ResultCode)code
                                okBlock:(TMWMVoidBlock)okBlock;
- (void)showDownloadDetail:(UIButton *)sender;

@end
