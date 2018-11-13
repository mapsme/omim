#import "MWMTag.h"

@interface MWMTagGroup : NSObject

@property (copy, nonatomic) NSString * name;
@property (nonatomic) NSArray<MWMTag *> * tags;

@end

