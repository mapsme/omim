#import "UIColor+PartnerColor.h"

@interface UIColor (MapsMeColor)

+ (UIColor *)primaryDark;
+ (UIColor *)primary;
+ (UIColor *)secondary;
+ (UIColor *)primaryLight;
+ (UIColor *)fadeBackground;
+ (UIColor *)menuBackground;
+ (UIColor *)downloadBadgeBackground;
+ (UIColor *)pressBackground;
+ (UIColor *)yellow;
+ (UIColor *)green;
+ (UIColor *)red;
+ (UIColor *)errorPink;
+ (UIColor *)orange;
+ (UIColor *)linkBlue;
+ (UIColor *)linkBlueHighlighted;
+ (UIColor *)linkBlueDark;
+ (UIColor *)buttonRed;
+ (UIColor *)buttonRedHighlighted;
+ (UIColor *)blackPrimaryText;
+ (UIColor *)blackSecondaryText;
+ (UIColor *)blackStatusBarBackground;
+ (UIColor *)blackHintText;
+ (UIColor *)blackDividers;
+ (UIColor *)white;
+ (UIColor *)whitePrimaryText;
+ (UIColor *)whitePrimaryTextHighlighted;
+ (UIColor *)whiteSecondaryText;
+ (UIColor *)whiteHintText;
+ (UIColor *)whiteDividers;
+ (UIColor *)buttonDisabledBlueText;
+ (UIColor *)buttonHighlightedBlueText;
+ (UIColor *)alertBackground;
+ (UIColor *)blackOpaque;
+ (UIColor *)bookingBackground;
+ (UIColor *)opentableBackground;
+ (UIColor *)bannerBackground;
+ (UIColor *)bannerButtonBackground;
+ (UIColor *)toastBackground;
+ (UIColor *)statusBarBackground;
+ (UIColor *)transparentGreen;
+ (UIColor *)ratingRed;
+ (UIColor *)ratingOrange;
+ (UIColor *)ratingYellow;
+ (UIColor *)ratingLightGreen;
+ (UIColor *)ratingGreen;
+ (UIColor *)border;
+ (UIColor *)speedLimitRed;
+ (UIColor *)speedLimitGeen;
+ (UIColor *)speedLimitWhite;
+ (UIColor *)speedLimitLightGray;
+ (UIColor *)speedLimitDarkGray;
+ (UIColor *)shadowColorBlue;
+ (UIColor *)shadowColorPurple;
+ (UIColor *)subscriptionCellBorder;
+ (UIColor *)subscriptionCellBackground;
+ (UIColor *)subscriptionCellTitle;

+ (UIColor *)colorWithName:(NSString *)colorName;
+ (UIColor *)colorFromHexString:(NSString *)hexString;

+ (void)setNightMode:(BOOL)mode;
+ (BOOL)isNightMode;

- (UIColor *)opposite;

+ (UIColor *)facebookButtonBackground;
+ (UIColor *)facebookButtonBackgroundDisabled;

@end
