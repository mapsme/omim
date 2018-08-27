#import "EAGLView.h"
#import "iosOGLContextFactory.h"
#import "MetalContextFactory.h"
#import "MetalView.h"
#import "MWMDirectionView.h"
#import "MWMMapWidgets.h"

#import "3party/Alohalytics/src/alohalytics_objc.h"

#include "Framework.h"

#include "drape/visual_scale.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

@implementation EAGLView

namespace
{
// Returns DPI as exact as possible. It works for iPhone, iPad and iWatch.
double getExactDPI(double contentScaleFactor)
{
  float const iPadDPI = 132.f;
  float const iPhoneDPI = 163.f;
  float const mDPI = 160.f;

  switch (UI_USER_INTERFACE_IDIOM())
  {
    case UIUserInterfaceIdiomPhone:
      return iPhoneDPI * contentScaleFactor;
    case UIUserInterfaceIdiomPad:
      return iPadDPI * contentScaleFactor;
    default:
      return mDPI * contentScaleFactor;
  }
}
} //  namespace

// You must implement this method
+ (Class)layerClass
{
  return [CAEAGLLayer class];
}

// The GL view is stored in the nib file. When it's unarchived it's sent -initWithCoder:
- (id)initWithCoder:(NSCoder *)coder
{
  NSLog(@"EAGLView initWithCoder Started");
  self = [super initWithCoder:coder];
  if (self)
    [self initialize];

  NSLog(@"EAGLView initWithCoder Ended");
  return self;
}

- (dp::ApiVersion)getSupportedApiVersion
{
  id<MTLDevice> tempDevice = MTLCreateSystemDefaultDevice();
  if (tempDevice)
    return dp::ApiVersion::Metal;
  
  EAGLContext * tempContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  if (tempContext != nil)
    return dp::ApiVersion::OpenGLES3;
  
  return dp::ApiVersion::OpenGLES2;;
}

- (void)initialize
{
  m_presentAvailable = false;
  m_lastViewSize = CGRectZero;
  m_apiVersion = [self getSupportedApiVersion];

  // Correct retina display support in renderbuffer.
  self.contentScaleFactor = [[UIScreen mainScreen] nativeScale];
  
  if (m_apiVersion != dp::ApiVersion::Metal)
  {
    // Setup Layer Properties
    CAEAGLLayer * eaglLayer = (CAEAGLLayer *)self.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = @{kEAGLDrawablePropertyRetainedBacking : @NO,
                                     kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGBA8};
  }
}

- (void)createDrapeEngine
{
  if (m_apiVersion == dp::ApiVersion::Metal)
  {
    CHECK(self.metalView != nil, ());
    CHECK_EQUAL(self.bounds.size.width, self.metalView.bounds.size.width, ());
    CHECK_EQUAL(self.bounds.size.height, self.metalView.bounds.size.height, ());
    m_factory = make_unique_dp<MetalContextFactory>(self.metalView);
  }
  else
  {
    CAEAGLLayer * eaglLayer = (CAEAGLLayer *)self.layer;
    m_factory = make_unique_dp<dp::ThreadSafeFactory>(
                  new iosOGLContextFactory(eaglLayer, m_apiVersion, m_presentAvailable));
  }
  
  m2::PointU const s = [self pixelSize];
  [self createDrapeEngineWithWidth:s.x height:s.y];
}

- (void)createDrapeEngineWithWidth:(int)width height:(int)height
{
  LOG(LINFO, ("CreateDrapeEngine Started", width, height));
  CHECK(m_factory != nullptr, ());
  
  Framework::DrapeCreationParams p;
  p.m_apiVersion = m_apiVersion;
  p.m_surfaceWidth = width;
  p.m_surfaceHeight = height;
  p.m_visualScale = dp::VisualScale(getExactDPI(self.contentScaleFactor));
  p.m_hints.m_isFirstLaunch = [Alohalytics isFirstSession];
  p.m_hints.m_isLaunchByDeepLink = self.isLaunchByDeepLink;

  [self.widgetsManager setupWidgets:p];
  GetFramework().CreateDrapeEngine(make_ref(m_factory), move(p));

  self->_drapeEngineCreated = YES;
  LOG(LINFO, ("CreateDrapeEngine Finished"));
}

- (void)addSubview:(UIView *)view
{
  [super addSubview:view];
  for (UIView * v in self.subviews)
  {
    if ([v isKindOfClass:[MWMDirectionView class]])
    {
      [self bringSubviewToFront:v];
      break;
    }
  }
}

- (m2::PointU)pixelSize
{
  CGSize const s = self.bounds.size;
  uint32_t const w = static_cast<uint32_t>(s.width * self.contentScaleFactor);
  uint32_t const h = static_cast<uint32_t>(s.height * self.contentScaleFactor);
  return m2::PointU(w, h);
}

- (void)layoutSubviews
{
  if (!CGRectEqualToRect(m_lastViewSize, self.frame))
  {
    m_lastViewSize = self.frame;
    m2::PointU const s = [self pixelSize];
    GetFramework().OnSize(s.x, s.y);
    [self.widgetsManager resize:CGSizeMake(s.x, s.y)];
  }
  [super layoutSubviews];
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

- (void)setPresentAvailable:(BOOL)available
{
  m_presentAvailable = available;
  if (m_factory != nullptr)
    m_factory->SetPresentAvailable(m_presentAvailable);
}

- (MWMMapWidgets *)widgetsManager
{
  if (!_widgetsManager)
    _widgetsManager = [[MWMMapWidgets alloc] init];
  return _widgetsManager;
}

@end
