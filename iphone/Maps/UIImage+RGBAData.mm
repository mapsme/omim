#import "UIImage+RGBAData.h"

namespace
{
BOOL isBackground()
{
  return UIApplication.sharedApplication.applicationState == UIApplicationStateBackground;
}

}  // namespace

@implementation UIImage (RGBAData)

+ (UIImage *)imageWithRGBAData:(NSData *)data width:(size_t)width height:(size_t)height
{
  size_t constexpr bytesPerPixel = 4;
  size_t constexpr bitsPerComponent = 8;
  size_t constexpr bitsPerPixel = bitsPerComponent * bytesPerPixel;
  size_t const bytesPerRow = bytesPerPixel * width;
  bool constexpr shouldInterpolate = true;
  CGBitmapInfo constexpr bitmapInfo = kCGBitmapByteOrderDefault | kCGImageAlphaLast;
  if (isBackground())
    return nil;

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  if (isBackground())
    return nil;

  CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, data.bytes, height * bytesPerRow, NULL);
  if (isBackground())
    return nil;

  CGImageRef cgImage = CGImageCreate(width, height, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpace, bitmapInfo, provider, NULL, shouldInterpolate, kCGRenderingIntentDefault);
  if (isBackground())
    return nil;

  UIImage * image = [UIImage imageWithCGImage:cgImage];

  CGColorSpaceRelease(colorSpace);
  CGDataProviderRelease(provider);
  CGImageRelease(cgImage);

  return image;
}

@end
