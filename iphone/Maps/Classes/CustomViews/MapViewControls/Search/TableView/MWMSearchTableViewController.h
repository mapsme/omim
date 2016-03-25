#import "MWMSearchManager.h"
#import "MWMSearchTabbedViewProtocol.h"
#import "MWMSearchTextField.h"
#import "MWMViewController.h"

#include "Framework.h"

namespace search
{
  class Result;
}

@protocol MWMSearchTableViewProtocol <MWMSearchTabbedViewProtocol>

@property (weak, nonatomic) MWMSearchTextField * _Nullable searchTextField;

@property (nonatomic) MWMSearchManagerState state;

- (void)processSearchWithResult:(search::Result const &)result
                          query:(search::QuerySaver::TSearchRequest const &)query;

@end

@interface MWMSearchTableViewController : MWMViewController

@property (nonatomic) BOOL searchOnMap;

- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));
- (instancetype _Nullable)initWithDelegate:(id<MWMSearchTableViewProtocol> _Nonnull)delegate;

- (void)searchText:(NSString * _Nonnull)text forInputLocale:(NSString * _Nullable)locale;
- (search::SearchParams const &)searchParams;

@end
