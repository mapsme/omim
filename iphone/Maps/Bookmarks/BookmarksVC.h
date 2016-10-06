#import "MWMTableViewController.h"

@interface BookmarksVC : MWMTableViewController <UITextFieldDelegate>
{
  NSInteger m_categoryIndex;
}

- (instancetype)initWithCategory:(NSInteger)index;

@end
