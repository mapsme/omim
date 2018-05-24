#import "MWMiPhonePlacePageLayoutImpl.h"
#import "MWMPPPreviewLayoutHelper.h"
#import "MWMPlacePageLayout.h"
#import "SwiftBridge.h"

namespace
{
enum class ScrollDirection
{
  Up,
  Down
};

enum class State
{
  Bottom,
  Top,
  Expanded
};

CGFloat const kTopPlacePageStopValue = 0.7;
CGFloat const kExpandedPlacePageStopValue = 0.5;
CGFloat const kLuftDraggingOffset = 30;

// Minimal offset for collapse. If place page offset is below this value we should hide place page.
CGFloat const kMinOffset = 1;
}  // namespace

@interface MWMiPhonePlacePageLayoutImpl ()<UIScrollViewDelegate, UITableViewDelegate,
                                           MWMPPPreviewLayoutHelperDelegate>

@property(nonatomic) MWMPPScrollView * scrollView;
@property(nonatomic) ScrollDirection direction;
@property(nonatomic) State state;

@property(nonatomic) CGFloat lastContentOffset;
@property(weak, nonatomic) MWMPPPreviewLayoutHelper * previewLayoutHelper;

@property(nonatomic) CGRect availableArea;
@property(nonatomic) BOOL isOffsetAnimated;

@end

@implementation MWMiPhonePlacePageLayoutImpl

@synthesize ownerView = _ownerView;
@synthesize placePageView = _placePageView;
@synthesize delegate = _delegate;
@synthesize actionBar = _actionBar;

- (instancetype)initOwnerView:(UIView *)ownerView
                placePageView:(MWMPPView *)placePageView
                     delegate:(id<MWMPlacePageLayoutDelegate>)delegate
{
  self = [super init];
  if (self)
  {
    auto const & size = ownerView.size;
    _ownerView = ownerView;
    _availableArea = ownerView.frame;
    _placePageView = placePageView;
    placePageView.tableView.delegate = self;
    _delegate = delegate;
    [self setScrollView:[[MWMPPScrollView alloc] initWithFrame:ownerView.frame
                                                  inactiveView:placePageView]];
    placePageView.frame = {{0, size.height}, size};
  }
  return self;
}

- (void)onShow
{
  self.state = [self.delegate isExpandedOnShow] ? State::Expanded : State::Bottom;
  auto scrollView = self.scrollView;
  
  [scrollView setContentOffset:{ 0., kMinOffset }];

  dispatch_async(dispatch_get_main_queue(), ^{
    place_page_layout::animate(^{
      [self.actionBar setVisible:YES];
      auto const targetOffset =
          self.state == State::Expanded ? self.expandedContentOffset : self.bottomContentOffset;
      [self setAnimatedContentOffset:targetOffset];
    });
  });
}

- (void)onClose
{
  self.actionBar = nil;
  place_page_layout::animate(^{
    [self setAnimatedContentOffset:0];
  },^{
    id<MWMPlacePageLayoutDelegate> delegate = self.delegate;
    // Workaround for preventing a situation when the scroll view destroyed before an animation finished.
    [delegate onPlacePageTopBoundChanged:0];
    self.scrollView = nil;
    [delegate destroyLayout];
  });
}

- (void)updateAvailableArea:(CGRect)frame
{
  if (CGRectEqualToRect(self.availableArea, frame))
    return;
  self.availableArea = frame;

  UIScrollView * sv = self.scrollView;
  sv.delegate = nil;
  sv.frame = frame;
  sv.delegate = self;
  auto const size = frame.size;
  self.placePageView.minY = size.height;
  [self.delegate onPlacePageTopBoundChanged:self.scrollView.contentOffset.y];
  [self setAnimatedContentOffset:self.state == State::Top ? self.topContentOffset
                                                          : self.bottomContentOffset];
}

- (void)updateContentLayout
{
  auto const & size = self.availableArea.size;
  self.scrollView.contentSize = {size.width, size.height + self.placePageView.height};
}

- (void)setPreviewLayoutHelper:(MWMPPPreviewLayoutHelper *)previewLayoutHelper
{
  previewLayoutHelper.delegate = self;
  _previewLayoutHelper = previewLayoutHelper;
}

#pragma mark - MWMPPPreviewLayoutHelperDelegate

- (void)heightWasChanged
{
  dispatch_async(dispatch_get_main_queue(), ^{
    if (self.state == State::Bottom)
      [self setAnimatedContentOffset:self.bottomContentOffset];
  });
}

#pragma mark - UIScrollViewDelegate

- (BOOL)isPortrait
{
  auto const & s = self.ownerView.size;
  return s.height > s.width;
}

- (CGFloat)openContentOffset
{
  auto const & size = self.ownerView.size;
  auto const offset = self.isPortrait ? MAX(size.width, size.height) : MIN(size.width, size.height);
  return offset * kTopPlacePageStopValue;
}

- (CGFloat)topContentOffset
{
  auto const target = self.openContentOffset;
  auto const ppViewMaxY = self.placePageView.tableView.maxY;
  return MIN(target, ppViewMaxY);
}

- (CGFloat)expandedContentOffset
{
  auto const & size = self.ownerView.size;
  auto const offset = self.isPortrait ? MAX(size.width, size.height) : MIN(size.width, size.height);
  return offset * kExpandedPlacePageStopValue;
}

- (CGFloat)bottomContentOffset
{
  return self.previewLayoutHelper.height + self.actionBar.height - self.placePageView.top.height;
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
  dispatch_async(dispatch_get_main_queue(), ^{
    self.isOffsetAnimated = NO;
  });
}

- (void)scrollViewDidScroll:(MWMPPScrollView *)scrollView
{
  if (self.isOffsetAnimated)
    return;
  auto ppView = self.placePageView;
  if ([scrollView isEqual:ppView.tableView])
    return;

  auto const & offsetY = scrollView.contentOffset.y;
  id<MWMPlacePageLayoutDelegate> delegate = self.delegate;
  if (offsetY <= 0)
  {
    [delegate onPlacePageTopBoundChanged:0];
    [delegate closePlacePage];
    return;
  }

  auto const bounded = ppView.height + kLuftDraggingOffset;
  if (offsetY > bounded)
  {
    [scrollView setContentOffset:{0, bounded}];
    [delegate onPlacePageTopBoundChanged:bounded];
  }
  else
  {
    [delegate onPlacePageTopBoundChanged:offsetY];
  }

  self.direction = self.lastContentOffset < offsetY ? ScrollDirection::Up : ScrollDirection::Down;
  self.lastContentOffset = offsetY;
}

- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint *)targetContentOffset
{
  auto const actualOffset = scrollView.contentOffset.y;
  auto const openOffset = self.openContentOffset;
  auto const targetOffset = (*targetContentOffset).y;

  if (actualOffset > self.bottomContentOffset && actualOffset < openOffset)
  {
    auto const isDirectionUp = self.direction == ScrollDirection::Up;
    self.state = isDirectionUp ? State::Top : State::Bottom;
    (*targetContentOffset).y = isDirectionUp ? openOffset : self.bottomContentOffset;
  }
  else if (actualOffset > openOffset && targetOffset < openOffset)
  {
    self.state = State::Top;
    (*targetContentOffset).y = openOffset;
  }
  else if (actualOffset < self.bottomContentOffset)
  {
    (*targetContentOffset).y = 0;
  }
  else
  {
    self.state = State::Top;
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)decelerate
{
  if (decelerate)
    return;

  auto const actualOffset = scrollView.contentOffset.y;
  auto const openOffset = self.openContentOffset;

  if (actualOffset < self.bottomContentOffset + kLuftDraggingOffset)
  {
    self.state = State::Bottom;
    place_page_layout::animate(^{
      [self setAnimatedContentOffset:self.bottomContentOffset];
    });
  }
  else if (actualOffset < openOffset)
  {
    auto const isDirectionUp = self.direction == ScrollDirection::Up;
    self.state = isDirectionUp ? State::Top : State::Bottom;
    place_page_layout::animate(^{
      [self setAnimatedContentOffset:isDirectionUp ? openOffset : self.bottomContentOffset];
    });
  }
  else
  {
    self.state = State::Top;
  }
}

- (void)setState:(State)state
{
  _state = state;
  BOOL const isTop = state == State::Top;
  self.placePageView.anchorImage.transform = isTop ? CGAffineTransformMakeRotation(M_PI)
  : CGAffineTransformIdentity;
  [self.previewLayoutHelper layoutInOpenState:isTop];
  if (isTop)
    [self.delegate onExpanded];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  if (indexPath.section != 0)
    return;

  auto cell = [tableView cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[MWMAdBanner class]])
  {
    return;
  }

  CGFloat offset = 0;
  if (self.state == State::Top)
  {
    self.state = State::Bottom;
    offset = self.bottomContentOffset;
  }
  else
  {
    self.state = State::Top;
    offset = self.topContentOffset;
  }

  place_page_layout::animate(^{
    [self setAnimatedContentOffset:offset];
  });
}

#pragma mark - Properties

- (void)setAnimatedContentOffset:(CGFloat)offset
{
  self.isOffsetAnimated = YES;
  [self.scrollView setContentOffset:{0, offset} animated:YES];
}

- (void)setScrollView:(MWMPPScrollView *)scrollView
{
  if (scrollView)
  {
    scrollView.delegate = self;
    [scrollView addSubview:self.placePageView];
    [self.ownerView addSubview:scrollView];
  }
  else
  {
    _scrollView.delegate = nil;
    [_scrollView.subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];
    [_scrollView removeFromSuperview];
  }
  _scrollView = scrollView;
}

- (void)setActionBar:(MWMPlacePageActionBar *)actionBar
{
  if (actionBar)
  {
    auto superview = self.ownerView;
    [superview addSubview:actionBar];
    NSLayoutXAxisAnchor * leadingAnchor = superview.leadingAnchor;
    NSLayoutXAxisAnchor * trailingAnchor = superview.trailingAnchor;
    if (@available(iOS 11.0, *))
    {
      UILayoutGuide * safeAreaLayoutGuide = superview.safeAreaLayoutGuide;
      leadingAnchor = safeAreaLayoutGuide.leadingAnchor;
      trailingAnchor = safeAreaLayoutGuide.trailingAnchor;
    }
    [actionBar.leadingAnchor constraintEqualToAnchor:leadingAnchor].active = YES;
    [actionBar.trailingAnchor constraintEqualToAnchor:trailingAnchor].active = YES;
    [actionBar setVisible:NO];
  }
  else
  {
    [_actionBar removeFromSuperview];
  }
  _actionBar = actionBar;
}

@end

