#import "CatalogPromoItem+Core.h"

@implementation CatalogPromoItem (Core)

- (instancetype)initWithCoreItem:(promo::CityGallery::Item const &)item {
  self = [super init];
  if (self) {
    self.placeTitle = @(item.m_place.m_name.c_str());
    self.placeDescription = @(item.m_place.m_description.c_str());
    self.imageUrl = @(item.m_imageUrl.c_str());
    self.catalogUrl = @(item.m_url.c_str());
    self.guideName = @(item.m_name.c_str());
    self.label = @(item.m_luxCategory.m_name.c_str());
    self.labelHexColor = @(item.m_luxCategory.m_color.c_str());
    if (!item.m_tourCategory.empty()) {
      self.guideAuthor = @(item.m_tourCategory.c_str());
    } else {
      self.guideAuthor = @(item.m_author.m_name.c_str());
    }
  }
  return self;
}

@end
