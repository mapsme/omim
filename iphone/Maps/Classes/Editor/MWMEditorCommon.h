@class MWMEditorTextTableViewCell;

@protocol MWMEditorCellProtocol <NSObject>

@required
- (void)cellAtIndexPath:(NSIndexPath *)indexPath changedText:(NSString *)changeText;
- (void)cell:(UITableViewCell *)cell changeSwitch:(BOOL)changeSwitch;
- (void)cellSelect:(UITableViewCell *)cell;
- (void)tryToChangeInvalidStateForCell:(MWMEditorTextTableViewCell *)cell;

@end
