/**
 * Copyright (c) 2015-present, Parse, LLC.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#import <Foundation/Foundation.h>

#import <Parse/PFFile.h>
#import <Parse/PFObject.h>
#import <Parse/PFSubclassing.h>

PF_OSX_UNAVAILABLE_WARNING
PF_WATCH_UNAVAILABLE_WARNING

NS_ASSUME_NONNULL_BEGIN

/**
 The `PFProduct` class represents an in-app purchase product on the Parse server.
 By default, products can only be created via the Data Browser. Saving a `PFProduct` will result in error.
 However, the products' metadata information can be queried and viewed.

 This class is currently for iOS only.
 */
PF_OSX_UNAVAILABLE PF_WATCH_UNAVAILABLE @interface PFProduct : PFObject<PFSubclassing>

///--------------------------------------
/// @name Product-specific Properties
///--------------------------------------

/**
 The product identifier of the product.

 This should match the product identifier in iTunes Connect exactly.
 */
@property (nullable, nonatomic, strong) NSString *productIdentifier;

/**
 The icon of the product.
 */
@property (nullable, nonatomic, strong) PFFile *icon;

/**
 The title of the product.
 */
@property (nullable, nonatomic, strong) NSString *title;

/**
 The subtitle of the product.
 */
@property (nullable, nonatomic, strong) NSString *subtitle;

/**
 The order in which the product information is displayed in `PFProductTableViewController`.

 The product with a smaller order is displayed earlier in the `PFProductTableViewController`.
 */
@property (nullable, nonatomic, strong) NSNumber *order;

/**
 The name of the associated download.

 If there is no downloadable asset, it should be `nil`.
 */
@property (nullable, nonatomic, strong, readonly) NSString *downloadName;

@end

NS_ASSUME_NONNULL_END
