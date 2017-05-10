#import "MWMViewController.h"

#include "indexer/editable_map_object.hpp"

#include <string>

@protocol MWMObjectsCategorySelectorDelegate <NSObject>

- (void)reloadObject:(osm::EditableMapObject const &)object;

@end

@interface MWMObjectsCategorySelectorController : MWMViewController

@property (weak, nonatomic) id<MWMObjectsCategorySelectorDelegate> delegate;

- (void)setSelectedCategory:(std::string const &)category;

@end
