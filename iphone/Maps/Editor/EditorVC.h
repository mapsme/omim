#import "TableViewController.h"

class UserMark;

@interface EditorVC : TableViewController <UITextFieldDelegate>
{
}

- (id)initWithUserMark:(UserMark const *)mark;

@end
