#import "MWMSearchBookmarksManager.h"
#import "MWMSearchBookmarksCell.h"
#import "MWMSearchNoResults.h"
#import "Macros.h"

#include "Framework.h"

extern NSString * const kBookmarksChangedNotification;

static NSString * const kBookmarksCellIdentifier = @"MWMSearchBookmarksCell";

@interface MWMSearchBookmarksManager ()

@property(weak, nonatomic) MWMSearchTabbedCollectionViewCell * cell;

@property(nonatomic) MWMSearchBookmarksCell * sizingCell;
@property(nonatomic) MWMSearchNoResults * noResultsView;

@end

@implementation MWMSearchBookmarksManager

- (instancetype)init
{
  self = [super init];
  if (self)
  {
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(updateCell)
                                               name:kBookmarksChangedNotification
                                             object:nil];
  }
  return self;
}

- (void)dealloc { [NSNotificationCenter.defaultCenter removeObserver:self]; }
- (void)attachCell:(MWMSearchTabbedCollectionViewCell *)cell
{
  self.cell = cell;
  [self updateCell];
}

- (void)updateCell
{
  MWMSearchTabbedCollectionViewCell * cell = self.cell;
  if (!cell)
    return;
  if (GetFramework().GetBmCategoriesCount() > 0)
  {
    [cell removeNoResultsView];
    UITableView * tableView = cell.tableView;
    tableView.hidden = NO;
    tableView.delegate = self;
    tableView.dataSource = self;
    [tableView registerNib:[UINib nibWithNibName:kBookmarksCellIdentifier bundle:nil]
        forCellReuseIdentifier:kBookmarksCellIdentifier];
    [tableView reloadData];
  }
  else
  {
    cell.tableView.hidden = YES;
    [cell addNoResultsView:self.noResultsView];
  }
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  return GetFramework().GetBmCategoriesCount();
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  return [tableView dequeueReusableCellWithIdentifier:kBookmarksCellIdentifier];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView *)tableView
    estimatedHeightForRowAtIndexPath:(NSIndexPath *)indexPath
{
  return MWMSearchBookmarksCell.defaultCellHeight;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
  [self.sizingCell configForIndex:indexPath.row];
  return self.sizingCell.cellHeight;
}

- (void)tableView:(UITableView *)tableView
      willDisplayCell:(MWMSearchBookmarksCell *)cell
    forRowAtIndexPath:(NSIndexPath *)indexPath
{
  [cell configForIndex:indexPath.row];
}

#pragma mark - Properties

- (MWMSearchBookmarksCell *)sizingCell
{
  if (!_sizingCell)
    _sizingCell = [self.cell.tableView dequeueReusableCellWithIdentifier:kBookmarksCellIdentifier];
  return _sizingCell;
}

- (MWMSearchNoResults *)noResultsView
{
  if (!_noResultsView)
  {
    _noResultsView = [MWMSearchNoResults viewWithImage:[UIImage imageNamed:@"img_bookmarks"]
                                                 title:L(@"search_bookmarks_no_results_title")
                                                  text:L(@"search_bookmarks_no_results_text")];
  }
  return _noResultsView;
}

@end
