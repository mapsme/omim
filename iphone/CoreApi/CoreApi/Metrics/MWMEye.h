#import <UIKit/UIKit.h>

typedef NS_ENUM(NSUInteger, MWMTipType) {
  MWMTipTypeBookmarks,
  MWMTipTypeSearch,
  MWMTipTypeDiscovery,
  MWMTipTypeSubway,
  MWMTipTypeIsolines,
  MWMTipTypeNone
} NS_SWIFT_NAME(TipType);

typedef NS_ENUM(NSUInteger, MWMTipEvent) {
  MWMTipEventAction,
  MWMTipEventGotIt
} NS_SWIFT_NAME(TipEvent);

typedef NS_ENUM(NSUInteger, MWMEyeDiscoveryEvent) {
  MWMEyeDiscoveryEventHotels,
  MWMEyeDiscoveryEventAttractions,
  MWMEyeDiscoveryEventCafes,
  MWMEyeDiscoveryEventLocals,
  MWMEyeDiscoveryEventMoreHotels,
  MWMEyeDiscoveryEventMoreAttractions,
  MWMEyeDiscoveryEventMoreCafes,
  MWMEyeDiscoveryEventMoreLocals
};

@interface MWMEye: NSObject

+ (MWMTipType)getTipType;
+ (void)tipClickedWithType:(MWMTipType)type event:(MWMTipEvent)event;
+ (void)bookingFilterUsed;
+ (void)boomarksCatalogShown;
+ (void)discoveryShown;
+ (void)discoveryItemClickedWithEvent:(MWMEyeDiscoveryEvent)event;
+ (void)transitionToBookingWithPos:(CGPoint)pos;
+ (void)promoAfterBookingShownWithCityId:(NSString *)cityId;

@end
