#import "Common.h"
#import "MWMMapDownloaderDataSource.h"
#import "MWMMapDownloaderLargeCountryTableViewCell.h"
#import "MWMMapDownloaderPlaceTableViewCell.h"
#import "MWMMapDownloaderSubplaceTableViewCell.h"

#include "Framework.h"

using namespace storage;

@interface MWMMapDownloaderDataSource ()

@property (weak, nonatomic) id<MWMMapDownloaderProtocol> _Nullable delegate;

@property (nonatomic, readwrite) BOOL needFullReload;

@end

@implementation MWMMapDownloaderDataSource

- (instancetype _Nullable)initWithDelegate:(id<MWMMapDownloaderProtocol>)delegate
{
  self = [super init];
  if (self)
  {
    _delegate = delegate;
    _reloadSections = [NSMutableIndexSet indexSet];
  }
  return self;
}

#pragma mark - Fill cells with data

- (void)fillCell:(MWMMapDownloaderTableViewCell *)cell atIndexPath:(NSIndexPath *)indexPath
{
  NSString * countryId = [self countryIdForIndexPath:indexPath];
  [cell setCountryId:countryId searchQuery:[self searchQuery]];

  if ([cell isKindOfClass:[MWMMapDownloaderPlaceTableViewCell class]])
  {
    MWMMapDownloaderPlaceTableViewCell * placeCell = static_cast<MWMMapDownloaderPlaceTableViewCell *>(cell);
    placeCell.needDisplayArea = self.isParentRoot;
  }

  if ([cell isKindOfClass:[MWMMapDownloaderSubplaceTableViewCell class]])
  {
    MWMMapDownloaderSubplaceTableViewCell * subplaceCell = static_cast<MWMMapDownloaderSubplaceTableViewCell *>(cell);
    [subplaceCell setSubplaceText:[self searchMatchedResultForCountryId:countryId]];
  }
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  return 0;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  NSString * reuseIdentifier = [self cellIdentifierForIndexPath:indexPath];
  MWMMapDownloaderTableViewCell * cell = [tableView dequeueReusableCellWithIdentifier:reuseIdentifier];
  cell.delegate = self.delegate;
  [self fillCell:cell atIndexPath:indexPath];
  return cell;
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
  return NO;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
  if (editingStyle == UITableViewCellEditingStyleDelete)
    [self.delegate deleteNode:[self countryIdForIndexPath:indexPath].UTF8String];
}

#pragma mark - MWMMapDownloaderDataSource

- (BOOL)isParentRoot
{
  return YES;
}

- (NSString *)parentCountryId
{
  return @(kInvalidCountryId.c_str());
}

- (NSString *)countryIdForIndexPath:(NSIndexPath *)indexPath
{
  return @(kInvalidCountryId.c_str());
}

- (NSString *)cellIdentifierForIndexPath:(NSIndexPath *)indexPath
{
  return nil;
}

- (NSString *)searchMatchedResultForCountryId:(NSString *)countryId
{
  return nil;
}

- (NSString *)searchQuery
{
  return nil;
}

- (void)setNeedFullReload:(BOOL)needFullReload
{
  _needFullReload = needFullReload;
  if (needFullReload)
    [self.reloadSections removeAllIndexes];
}

@end
