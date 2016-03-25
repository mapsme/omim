#import "MWMButton.h"

#include "std/vector.hpp"

typedef NS_ENUM(NSInteger, MWMCircularProgressState)
{
  MWMCircularProgressStateNormal,
  MWMCircularProgressStateSelected,
  MWMCircularProgressStateProgress,
  MWMCircularProgressStateSpinner,
  MWMCircularProgressStateFailed,
  MWMCircularProgressStateCompleted
};

using MWMCircularProgressStateVec = vector<MWMCircularProgressState>;

@class MWMCircularProgress;

@protocol MWMCircularProgressProtocol <NSObject>

- (void)progressButtonPressed:(MWMCircularProgress * _Nonnull)progress;

@end

@interface MWMCircularProgress : NSObject

+ (instancetype _Nullable)downloaderProgressForParentView:(UIView * _Nonnull)parentView;

@property (nonatomic) CGFloat progress;
@property (nonatomic) MWMCircularProgressState state;
@property (weak, nonatomic) id<MWMCircularProgressProtocol> _Nullable delegate;

- (void)setImage:(UIImage *  _Nonnull)image forStates:(MWMCircularProgressStateVec const &)states;
- (void)setColor:(UIColor * _Nonnull)color forStates:(MWMCircularProgressStateVec const &)states;
- (void)setColoring:(MWMButtonColoring)coloring forStates:(MWMCircularProgressStateVec const &)states;
- (void)setInvertColor:(BOOL)invertColor;

- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));
- (instancetype _Nullable)initWithParentView:(UIView * _Nonnull)parentView;

@end
