typedef void (^TPredictionBlock)(CLLocation *);

@interface MWMLocationPredictor : NSObject

- (instancetype)initWithOnPredictionBlock:(TPredictionBlock)onPredictBlock;
- (void)reset:(CLLocation *)info;
- (void)setMyPositionMode:(MWMMyPositionMode)mode;

@end
