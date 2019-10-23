#import "UIImage+RGBAData.h"

#include "Framework.h"

static void releaseCallback(void *info, const void *data, size_t size) {
  CFDataRef cfData = (CFDataRef)info;
  CFRelease(cfData);
  LOG(LINFO, ("UIImage releaseCallback called, size: ", size));
}

@implementation UIImage (RGBAData)

+ (UIImage *)imageWithRGBAData:(NSData *)data width:(size_t)width height:(size_t)height
{
  LOG(LINFO, ("[UIImage imageWithRGBAData:] begin, size: ", data.length, ", width: ", width, ", height: ", height));
  size_t constexpr bytesPerPixel = 4;
  size_t constexpr bitsPerComponent = 8;
  size_t constexpr bitsPerPixel = bitsPerComponent * bytesPerPixel;
  size_t const bytesPerRow = bytesPerPixel * width;
  CGBitmapInfo constexpr bitmapInfo = kCGBitmapByteOrderDefault | kCGImageAlphaLast;

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CFDataRef cfData = (__bridge_retained CFDataRef)data;
  CGDataProviderRef provider = CGDataProviderCreateWithData((void *)cfData,
                                                            data.bytes,
                                                            height * bytesPerRow,
                                                            releaseCallback);

  CGImageRef cgImage = CGImageCreate(width, height, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpace, bitmapInfo, provider, NULL, YES, kCGRenderingIntentDefault);

  UIImage * image = [UIImage imageWithCGImage:cgImage];

  CGColorSpaceRelease(colorSpace);
  CGDataProviderRelease(provider);
  CGImageRelease(cgImage);

  LOG(LINFO, ("[UIImage imageWithRGBAData:] end"));
  return image;
}

@end
