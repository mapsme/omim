@interface MWMBottomMenuCollectionViewCell : UICollectionViewCell

@property (weak, nonatomic) IBOutlet UIImageView * _Nullable icon;
@property (nonatomic, readonly) BOOL isEnabled;

- (void)configureWithImageName:(NSString *)imageName
                         label:(NSString *)label
                    badgeCount:(NSUInteger)badgeCount
                      isEnabled:(BOOL)isEnabled;

@end
