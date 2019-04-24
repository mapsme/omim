@interface MWMFrameworkHelper : NSObject

+ (void)processFirstLaunch;

+ (void)setVisibleViewport:(CGRect)rect;

+ (void)setTheme:(MWMTheme)theme;

+ (MWMDayTime)daytime;

+ (void)checkConnectionAndPerformAction:(MWMVoidBlock)action cancelAction:(MWMVoidBlock)cancel;

+ (void)createFramework;

+ (BOOL)canUseNetwork;

+ (BOOL)isNetworkConnected;

+ (MWMMarkGroupID)invalidCategoryId;

@end
