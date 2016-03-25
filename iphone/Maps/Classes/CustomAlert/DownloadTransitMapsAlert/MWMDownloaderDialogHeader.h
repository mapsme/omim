#import <UIKit/UIKit.h>
@class MWMDownloadTransitMapAlert;

@interface MWMDownloaderDialogHeader : UIView

@property (weak, nonatomic) IBOutlet UIButton * _Nullable headerButton;
@property (weak, nonatomic) IBOutlet UIImageView * _Nullable expandImage;

+ (instancetype _Nullable)headerForOwnerAlert:(MWMDownloadTransitMapAlert *)alert title:(NSString *)title size:(NSString *)size;
- (void)layoutSizeLabel;

@end
