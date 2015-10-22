#import "Common.h"
#import "EAGLView.h"

#import "../Platform/opengl/iosOGLContextFactory.h"

#include "Framework.h"
#include "../../indexer/classificator_loader.hpp"

#include "platform/platform.hpp"

#include "std/bind.hpp"

@implementation EAGLView

// You must implement this method
+ (Class)layerClass
{
  return [CAEAGLLayer class];
}

// The GL view is stored in the nib file. When it's unarchived it's sent -initWithCoder:
- (id)initWithCoder:(NSCoder *)coder
{
  NSLog(@"EAGLView initWithCoder Started");

  if ((self = [super initWithCoder:coder]))
  {
    lastViewSize = CGRectZero;
    _widgetsManager = [[MWMMapWidgets alloc] init];

    // Setup Layer Properties
    CAEAGLLayer * eaglLayer = (CAEAGLLayer *)self.layer;

    eaglLayer.opaque = YES;
    // ColorFormat : RGB565
    // Backbuffer : YES, (to prevent from loosing content when mixing with ordinary layers).
    eaglLayer.drawableProperties = @{kEAGLDrawablePropertyRetainedBacking : @NO, kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGB565};
    
    // Correct retina display support in opengl renderbuffer
    self.contentScaleFactor = [self correctContentScale];

    m_factory = make_unique_dp<dp::ThreadSafeFactory>(new iosOGLContextFactory(eaglLayer));
  }

  NSLog(@"EAGLView initWithCoder Ended");
  return self;
}

- (void)createDrapeEngineWithWidth:(int)width height:(int)height
{
  NSLog(@"EAGLView createDrapeEngine Started");

  Framework::DrapeCreationParams p;
  p.m_surfaceWidth = width;
  p.m_surfaceHeight = height;
  p.m_visualScale = self.contentScaleFactor;

  [self.widgetsManager setupWidgets:p];
  GetFramework().CreateDrapeEngine(make_ref<dp::OGLContextFactory>(m_factory), move(p));

  NSLog(@"EAGLView createDrapeEngine Ended");
}

- (void)onSize:(int)width withHeight:(int)height
{
  int w = width * self.contentScaleFactor;
  int h = height * self.contentScaleFactor;

  if (GetFramework().GetDrapeEngine() == nullptr)
  {
    [self createDrapeEngineWithWidth:w height:h];
    GetFramework().LoadState();
    return;
  }

  GetFramework().OnSize(w, h);

  [self.widgetsManager resize:CGSizeMake(w, h)];
}

- (double)correctContentScale
{
  UIScreen * uiScreen = [UIScreen mainScreen];
  if (isIOSVersionLessThan(8))
    return uiScreen.scale;
  else
    return uiScreen.nativeScale;
}

- (void)layoutSubviews
{
  if (!CGRectEqualToRect(lastViewSize, self.frame))
  {
    lastViewSize = self.frame;
    CGSize const s = self.bounds.size;
    [self onSize:s.width withHeight:s.height];
  }
}

- (void)deallocateNative
{
  GetFramework().PrepareToShutdown();
  m_factory.reset();
}

- (CGPoint)viewPoint2GlobalPoint:(CGPoint)pt
{
  CGFloat const scaleFactor = self.contentScaleFactor;
  m2::PointD const ptG = GetFramework().PtoG(m2::PointD(pt.x * scaleFactor, pt.y * scaleFactor));
  return CGPointMake(ptG.x, ptG.y);
}

- (CGPoint)globalPoint2ViewPoint:(CGPoint)pt
{
  CGFloat const scaleFactor = self.contentScaleFactor;
  m2::PointD const ptP = GetFramework().GtoP(m2::PointD(pt.x, pt.y));
  return CGPointMake(ptP.x / scaleFactor, ptP.y / scaleFactor);
}

@end
