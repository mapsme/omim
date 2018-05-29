//
//  MPImageDownloadQueue.m
//
//  Copyright (c) 2014 MoPub. All rights reserved.
//

#import "MPImageDownloadQueue.h"
#import "MPNativeAdError.h"
#import "MPLogging.h"
#import "MPNativeCache.h"
#import "MPHTTPNetworkSession.h"
#import "MPURLRequest.h"

@interface MPImageDownloadQueue ()

@property (atomic, strong) NSOperationQueue *imageDownloadQueue;
@property (atomic, assign) BOOL isCanceled;

@end

@implementation MPImageDownloadQueue

- (id)init
{
    self = [super init];

    if (self != nil) {
        _imageDownloadQueue = [[NSOperationQueue alloc] init];
        [_imageDownloadQueue setMaxConcurrentOperationCount:1]; // serial queue
    }

    return self;
}

- (void)dealloc
{
    [_imageDownloadQueue cancelAllOperations];
}

- (void)addDownloadImageURLs:(NSArray *)imageURLs completionBlock:(MPImageDownloadQueueCompletionBlock)completionBlock
{
    [self addDownloadImageURLs:imageURLs completionBlock:completionBlock useCachedImage:YES];
}

- (void)addDownloadImageURLs:(NSArray *)imageURLs completionBlock:(MPImageDownloadQueueCompletionBlock)completionBlock useCachedImage:(BOOL)useCachedImage
{
    __block NSMutableArray *errors = nil;

    for (NSURL *imageURL in imageURLs) {
        [self.imageDownloadQueue addOperationWithBlock:^{
            @autoreleasepool {
                if (![[MPNativeCache sharedCache] cachedDataExistsForKey:imageURL.absoluteString] || !useCachedImage) {
                    MPLogDebug(@"Downloading %@", imageURL);

                    __block NSError *error = nil;
                    __block NSData *data = nil;

                    // Synchronous attempt to fetch the image.
                    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
                    [MPHTTPNetworkSession startTaskWithHttpRequest:[MPURLRequest requestWithURL:imageURL] responseHandler:^(NSData * _Nonnull responseData, NSHTTPURLResponse * _Nonnull response) {
                        data = responseData;
                        dispatch_semaphore_signal(semaphore);
                    } errorHandler:^(NSError * _Nonnull networkError) {
                        error = networkError;
                        dispatch_semaphore_signal(semaphore);
                    }];
                    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

                    BOOL validImageDownloaded = data != nil;
                    if (validImageDownloaded) {
                        UIImage *downloadedImage = [UIImage imageWithData:data];
                        if (downloadedImage != nil) {
                            [[MPNativeCache sharedCache] storeData:data forKey:imageURL.absoluteString];
                        } else {
                            if (downloadedImage == nil) {
                                MPLogDebug(@"Error: invalid image data downloaded");
                            }

                            validImageDownloaded = NO;
                        }
                    }

                    if (!validImageDownloaded) {
                        if (error == nil) {
                            error = MPNativeAdNSErrorForImageDownloadFailure();
                        }

                        if (errors == nil) {
                            errors = [NSMutableArray array];
                        }

                        [errors addObject:error];
                    }
                }
            }
        }];
    }

    // after all images have been downloaded, invoke callback on main thread
    [self.imageDownloadQueue addOperationWithBlock:^{
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!self.isCanceled) {
                completionBlock(errors);
            }
        });
    }];
}

- (void)cancelAllDownloads
{
    self.isCanceled = YES;
    [self.imageDownloadQueue cancelAllOperations];
}

@end
