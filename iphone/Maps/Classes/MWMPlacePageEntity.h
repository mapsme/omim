#import <Foundation/Foundation.h>

#include "Framework.h"

#include "map/user_mark.hpp"

typedef NS_ENUM (NSUInteger, MWMPlacePageMetadataType)
{
  MWMPlacePageMetadataTypePostcode,
  MWMPlacePageMetadataTypePhoneNumber,
  MWMPlacePageMetadataTypeWebsite,
  MWMPlacePageMetadataTypeURL,
  MWMPlacePageMetadataTypeEmail,
  MWMPlacePageMetadataTypeOpenHours,
  MWMPlacePageMetadataTypeCoordinate,
  MWMPlacePageMetadataTypeWiFi,
  MWMPlacePageMetadataTypeBookmark,
  MWMPlacePageMetadataTypeEditButton
};

typedef NS_ENUM (NSUInteger, MWMPlacePageEntityType)
{
  MWMPlacePageEntityTypeRegular,
  MWMPlacePageEntityTypeBookmark,
  MWMPlacePageEntityTypeEle,
  MWMPlacePageEntityTypeHotel,
  MWMPlacePageEntityTypeAPI,
  MWMPlacePageEntityTypeMyPosition
};

@class MWMPlacePageViewManager;

@interface MWMPlacePageEntity : NSObject

@property (copy, nonatomic) NSString * title;
@property (copy, nonatomic) NSString * category;
@property (copy, nonatomic) NSString * bookmarkTitle;
@property (copy, nonatomic) NSString * bookmarkCategory;
@property (copy, nonatomic) NSString * bookmarkDescription;
@property (nonatomic, readonly) BOOL isHTMLDescription;
@property (copy, nonatomic) NSString * bookmarkColor;

@property (nonatomic) MWMPlacePageEntityType type;

@property (nonatomic) int typeDescriptionValue;

@property (nonatomic) BookmarkAndCategory bac;
@property (weak, nonatomic) MWMPlacePageViewManager * manager;

@property (nonatomic, readonly) ms::LatLon latlon;

- (void)enableEditing;
- (void)insertBookmarkInTypes;
- (void)removeBookmarkFromTypes;

- (instancetype)initWithUserMark:(UserMark const *)mark;
- (void)synchronize;

- (void)toggleCoordinateSystem;

- (NSUInteger)getFeatureTypesCount;
- (MWMPlacePageMetadataType)getFeatureType:(NSUInteger)index;
- (NSString *)getFeatureValue:(MWMPlacePageMetadataType)type;

@end
