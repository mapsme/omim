#import "TableViewController.h"

class PoiMarkPoint;

@interface AddressEditorVC : TableViewController <UITextFieldDelegate>
{
}

- (id)initWithPoiMark:(PoiMarkPoint const *)mark;

@end
