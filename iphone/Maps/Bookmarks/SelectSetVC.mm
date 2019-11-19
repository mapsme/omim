#import "SelectSetVC.h"
#import "SwiftBridge.h"
#import "UIViewController+Navigation.h"

#import <CoreApi/CoreApi.h>

@interface SelectSetVC ()
{
  kml::MarkGroupId m_categoryId;
}

@property (copy, nonatomic) NSString * category;
@property (copy, nonatomic) NSArray<MWMCategory *> * categories;
@property (weak, nonatomic) id<MWMSelectSetDelegate> delegate;

@end

@implementation SelectSetVC

- (instancetype)initWithCategory:(NSString *)category
                      categoryId:(kml::MarkGroupId)categoryId
                        delegate:(id<MWMSelectSetDelegate>)delegate
{
  self = [super initWithStyle:UITableViewStyleGrouped];
  if (self)
  {
    _category = category;
    m_categoryId = categoryId;
    _delegate = delegate;
  }
  return self;
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  NSAssert(self.category, @"Category can't be nil!");
  NSAssert(self.delegate, @"Delegate can't be nil!");
  self.title = L(@"bookmark_sets");
  [self reloadData];
}

- (void)reloadData
{
  self.categories = [[MWMBookmarksManager sharedManager] userCategories];
  [self.tableView reloadData];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  // "Add new set" button
  if (section == 0)
    return 1;

  return self.categories.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  Class cls = [UITableViewCell class];
  auto cell = [tableView dequeueReusableCellWithCellClass:cls indexPath:indexPath];
  if (indexPath.section == 0)
  {
    cell.textLabel.text = L(@"add_new_set");
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  }
  else
  {
    MWMCategory * category = self.categories[indexPath.row];
    cell.textLabel.text = category.title;

    if (m_categoryId == category.categoryId)
      cell.accessoryType = UITableViewCellAccessoryCheckmark;
    else
      cell.accessoryType = UITableViewCellAccessoryNone;
  }
  return cell;
}

- (void)moveBookmarkToSetWithCategoryId:(kml::MarkGroupId)categoryId
{
  m_categoryId = categoryId;
  self.category = @(GetFramework().GetBookmarkManager().GetCategoryName(categoryId).c_str());
  [self reloadData];
  [self.delegate didSelectCategory:self.category withCategoryId:categoryId];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  if (indexPath.section == 0)
  {
    [self.alertController presentCreateBookmarkCategoryAlertWithMaxCharacterNum:60
                                                          minCharacterNum:0
                                                                 callback:^BOOL (NSString * name)
     {
       if (![[MWMBookmarksManager sharedManager] checkCategoryName:name])
         return false;

       auto const id = [[MWMBookmarksManager sharedManager] createCategoryWithName:name];
       [self moveBookmarkToSetWithCategoryId:id];
       return true;
    }];
  }
  else
  {
    auto const categoryId = self.categories[indexPath.row].categoryId;
    [self moveBookmarkToSetWithCategoryId:categoryId];
    [self.delegate didSelectCategory:self.category withCategoryId:categoryId];
    [self goBack];
  }
}

@end
