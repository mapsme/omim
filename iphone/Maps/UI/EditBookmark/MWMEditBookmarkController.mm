#import "MWMEditBookmarkController.h"
#import "MWMBookmarkColorViewController.h"
#import "MWMBookmarkTitleCell.h"
#import "MWMButtonCell.h"
#import "MWMNoteCell.h"
#import "SelectSetVC.h"
#import "SwiftBridge.h"

#import <CoreApi/PlacePageData.h>
#import <CoreApi/PlacePageBookmarkData+Core.h>
#import <CoreApi/PlacePagePreviewData.h>

#include <CoreApi/Framework.h>

namespace
{
enum Sections
{
  MetaInfo,
  Description,
  Delete,
  SectionsCount
};

enum RowInMetaInfo
{
  Title,
  Color,
  Categori,
  RowsInMetaInfoCount
};
}  // namespace

@interface MWMEditBookmarkController () <MWMButtonCellDelegate, MWMNoteCelLDelegate, MWMBookmarkColorDelegate,
                                         MWMSelectSetDelegate, MWMBookmarkTitleDelegate>
{
  kml::MarkId m_cachedBookmarkId;
  kml::MarkGroupId m_cachedBookmarkCatId;
}

@property (nonatomic) MWMNoteCell * cachedNote;
@property (copy, nonatomic) NSString * cachedDescription;
@property (copy, nonatomic) NSString * cachedTitle;
@property (nonatomic) kml::PredefinedColor cachedColor;
@property (copy, nonatomic) NSString * cachedCategory;
@property(nonatomic) kml::MarkGroupId cachedNewBookmarkCatId;

@end

@implementation MWMEditBookmarkController

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.cachedNewBookmarkCatId = kml::kInvalidMarkGroupId;
  self.cachedDescription = self.placePageData.bookmarkData.bookmarkDescription;
  self.cachedTitle = self.placePageData.previewData.title;
  self.cachedCategory = self.placePageData.bookmarkData.bookmarkCategory;
  self.cachedColor = [self.placePageData.bookmarkData kmlColor].m_predefinedColor;
  m_cachedBookmarkId = self.placePageData.bookmarkData.bookmarkId;
  m_cachedBookmarkCatId = self.placePageData.bookmarkData.bookmarkGroupId;
  [self configNavBar];
  [self registerCells];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  [self.cachedNote updateTextViewForHeight:self.cachedNote.textViewContentHeight];
}

- (void)configNavBar
{
  self.title = L(@"bookmark").capitalizedString;
  self.navigationItem.rightBarButtonItem =
  [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemSave
                                                target:self
                                                action:@selector(onSave)];
}

- (void)registerCells
{
  UITableView * tv = self.tableView;
  [tv registerNibWithCellClass:[MWMButtonCell class]];
  [tv registerNibWithCellClass:[MWMBookmarkTitleCell class]];
  [tv registerNibWithCellClass:[MWMNoteCell class]];
}

- (void)onSave
{
  [self.view endEditing:YES];
  auto & f = GetFramework();
  BookmarkManager & bmManager = f.GetBookmarkManager();
  auto editSession = bmManager.GetEditSession();
  if (self.cachedNewBookmarkCatId != kml::kInvalidMarkGroupId)
  {
    editSession.MoveBookmark(m_cachedBookmarkId, m_cachedBookmarkCatId, self.cachedNewBookmarkCatId);
    m_cachedBookmarkCatId = self.cachedNewBookmarkCatId;
  }

  auto bookmark = editSession.GetBookmarkForEdit(m_cachedBookmarkId);
  if (!bookmark)
    return;

  if (self.cachedColor != bookmark->GetColor())
    bmManager.SetLastEditedBmColor(self.cachedColor);
  
  bookmark->SetColor(self.cachedColor);
  bookmark->SetDescription(self.cachedDescription.UTF8String);
  if (self.cachedTitle.UTF8String != bookmark->GetPreferredName())
    bookmark->SetCustomName(self.cachedTitle.UTF8String);
  
  f.UpdatePlacePageInfoForCurrentSelection();
  [self.placePageData updateBookmarkStatus];
  [self goBack];
}

#pragma mark - UITableView

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return SectionsCount;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  switch (section)
  {
  case MetaInfo:
    return RowsInMetaInfoCount;
  case Description:
  case Delete:
    return 1;
  default:
    NSAssert(false, @"Incorrect section!");
    return 0;
  }
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  switch (indexPath.section)
  {
  case MetaInfo:
    switch (indexPath.row)
    {
    case Title:
    {
      Class cls = [MWMBookmarkTitleCell class];
      auto cell = static_cast<MWMBookmarkTitleCell *>(
          [tableView dequeueReusableCellWithCellClass:cls indexPath:indexPath]);
      [cell configureWithName:self.cachedTitle delegate:self];
      return cell;
    }
    case Color:
    {
      Class cls = [UITableViewCell class];
      auto cell = [tableView dequeueReusableCellWithCellClass:cls indexPath:indexPath];
      cell.textLabel.text = ios_bookmark_ui_helper::LocalizedTitleForBookmarkColor(self.cachedColor);
      cell.imageView.image = ios_bookmark_ui_helper::ImageForBookmarkColor(self.cachedColor, YES);
      cell.imageView.layer.cornerRadius = cell.imageView.width / 2;
      cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      return cell;
    }
    case Categori:
    {
      Class cls = [UITableViewCell class];
      auto cell = [tableView dequeueReusableCellWithCellClass:cls indexPath:indexPath];
      cell.textLabel.text = self.cachedCategory;
      cell.imageView.image = [UIImage imageNamed:@"ic_folder"];
      cell.imageView.styleName = @"MWMBlack";
      cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      return cell;
    }
    default:
      NSAssert(false, @"Incorrect row!");
      return nil;
    }
  case Description:
  {
    NSAssert(indexPath.row == 0, @"Incorrect row!");
    if (self.cachedNote)
    {
      return self.cachedNote;
    }
    else
    {
      Class cls = [MWMNoteCell class];
      self.cachedNote = static_cast<MWMNoteCell *>(
          [tableView dequeueReusableCellWithCellClass:cls indexPath:indexPath]);
      [self.cachedNote configWithDelegate:self noteText:self.cachedDescription
                              placeholder:L(@"placepage_personal_notes_hint")];
      return self.cachedNote;
    }
  }
  case Delete:
  {
    NSAssert(indexPath.row == 0, @"Incorrect row!");
    Class cls = [MWMButtonCell class];
    auto cell = static_cast<MWMButtonCell *>(
        [tableView dequeueReusableCellWithCellClass:cls indexPath:indexPath]);
    [cell configureWithDelegate:self title:L(@"placepage_delete_bookmark_button")];
    return cell;
  }
  default:
    NSAssert(false, @"Incorrect section!");
    return nil;
  }
  return nil;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
  if (indexPath.section == Description)
  {
    NSAssert(indexPath.row == 0, @"Incorrect row!");
    return self.cachedNote ? self.cachedNote.cellHeight : [MWMNoteCell minimalHeight];
  }
  return self.tableView.rowHeight;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  switch (indexPath.row)
  {
  case Color:
  {
    MWMBookmarkColorViewController * cvc = [[MWMBookmarkColorViewController alloc] initWithColor:self.cachedColor
                                                                                        delegate:self];
    [self.navigationController pushViewController:cvc animated:YES];
    break;
  }
  case Categori:
  {
    kml::MarkGroupId categoryId = self.cachedNewBookmarkCatId;
    if (categoryId == kml::kInvalidMarkGroupId)
      categoryId = m_cachedBookmarkCatId;
    SelectSetVC * svc = [[SelectSetVC alloc] initWithCategory:self.cachedCategory
                                                   categoryId:categoryId
                                                     delegate:self];
    [self.navigationController pushViewController:svc animated:YES];
    break;
  }
  default:
    break;
  }
}

#pragma mark - MWMButtonCellDelegate

- (void)cellSelect:(UITableViewCell *)cell
{
  [[MWMBookmarksManager sharedManager] deleteBookmark:m_cachedBookmarkId];
  GetFramework().UpdatePlacePageInfoForCurrentSelection();
  [self goBack];
}

#pragma mark - MWMNoteCellDelegate

- (void)cell:(MWMNoteCell *)cell didFinishEditingWithText:(NSString *)text
{
  self.cachedDescription = text;
}

- (void)cellShouldChangeSize:(MWMNoteCell *)cell text:(NSString *)text
{
  self.cachedDescription = text;
  [self.tableView refresh];
  NSIndexPath * ip = [self.tableView indexPathForCell:cell];
  [self.tableView scrollToRowAtIndexPath:ip
                        atScrollPosition:UITableViewScrollPositionBottom
                                animated:YES];
}

#pragma mark - MWMBookmarkColorDelegate

- (void)didSelectColor:(kml::PredefinedColor)color
{
  self.cachedColor = color;
  [self.tableView reloadRowsAtIndexPaths:@[[NSIndexPath indexPathForRow:Color inSection:MetaInfo]] withRowAnimation:UITableViewRowAnimationAutomatic];
}

#pragma mark - MWMSelectSetDelegate

- (void)didSelectCategory:(NSString *)category withCategoryId:(kml::MarkGroupId)categoryId
{
  self.cachedCategory = category;
  self.cachedNewBookmarkCatId = categoryId;
  [self.tableView reloadRowsAtIndexPaths:@[[NSIndexPath indexPathForRow:Categori inSection:MetaInfo]] withRowAnimation:UITableViewRowAnimationAutomatic];
}

#pragma mark - MWMBookmarkTitleDelegate

- (void)didFinishEditingBookmarkTitle:(NSString *)title
{
  self.cachedTitle = title;
  [self.tableView reloadRowsAtIndexPaths:@[[NSIndexPath indexPathForRow:Title inSection:MetaInfo]] withRowAnimation:UITableViewRowAnimationAutomatic];
}

@end
