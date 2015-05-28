//
//  MWMLocationButtonView.h
//  Maps
//
//  Created by Ilya Grechuhin on 14.05.15.
//  Copyright (c) 2015 MapsWithMe. All rights reserved.
//

#import <UIKit/UIKit.h>

#include "platform/location.hpp"

@interface MWMLocationButtonView : UIButton

@property (nonatomic) location::EMyPositionMode myPositionMode;

- (instancetype)initWithFrame:(CGRect)frame __attribute__((unavailable("initWithFrame is not available")));
- (instancetype)init __attribute__((unavailable("init is not available")));

@end
