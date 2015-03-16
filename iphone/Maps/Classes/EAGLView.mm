#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>
#import "Common.h"
#import "EAGLView.h"

#include "Framework.h"
#include "../../indexer/classificator_loader.hpp"
#import "../Platform/opengl/iosOGLContextFactory.h"

#include "../../platform/platform.hpp"

#include "../../std/bind.hpp"


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
    // Setup Layer Properties
    CAEAGLLayer * eaglLayer = (CAEAGLLayer *)self.layer;

    eaglLayer.opaque = YES;
    // ColorFormat : RGB565
    // Backbuffer : YES, (to prevent from loosing content when mixing with ordinary layers).
    eaglLayer.drawableProperties = @{kEAGLDrawablePropertyRetainedBacking : @NO, kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGB565};
    
    // Correct retina display support in opengl renderbuffer
    self.contentScaleFactor = [self correctContentScale];

    dp::ThreadSafeFactory * factory = new dp::ThreadSafeFactory(new iosOGLContextFactory(eaglLayer));
    m_factory.Reset(factory);
  }

  NSLog(@"EAGLView initWithCoder Ended");
  return self;
}

- (void)initRenderPolicy
{
  NSLog(@"EAGLView initRenderPolicy Started");

  CGRect frameRect = [UIScreen mainScreen].applicationFrame;
  GetFramework().CreateDrapeEngine(m_factory.GetRefPointer(), self.contentScaleFactor, frameRect.size.width, frameRect.size.height);

  NSLog(@"EAGLView initRenderPolicy Ended");
}

- (void)setMapStyle:(MapStyle)mapStyle
{
  //@TODO UVR
}

- (void)onSize:(int)width withHeight:(int)height
{
  GetFramework().OnSize(width * self.contentScaleFactor, height * self.contentScaleFactor);
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
  m_factory.Destroy();
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
