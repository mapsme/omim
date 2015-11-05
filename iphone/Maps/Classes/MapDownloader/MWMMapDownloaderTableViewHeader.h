@class MWMMapDownloaderTableViewHeader;

@protocol MWMMapDownloaderTableViewHeaderProtocol <NSObject>

- (void)expandButtonPressed:(MWMMapDownloaderTableViewHeader *)sender;

@end

@interface MWMMapDownloaderTableViewHeader : UIView

+ (CGFloat)height;

@property (weak, nonatomic) IBOutlet UILabel * title;

@property (weak, nonatomic) id<MWMMapDownloaderTableViewHeaderProtocol> delegate;
@property (nonatomic) BOOL expanded;
@property (nonatomic) NSInteger section;
@property (nonatomic) BOOL lastSection;

@end
