#import "ElevationProfileData+Core.h"

static ElevationDifficulty convertDifficulty(uint8_t difficulty) {
  switch (difficulty) {
    case ElevationInfo::Difficulty::Easy:
      return ElevationDifficulty::ElevationDifficultyEasy;
    case ElevationInfo::Difficulty::Medium:
      return ElevationDifficulty::ElevationDifficultyMedium;
    case ElevationInfo::Difficulty::Hard:
      return ElevationDifficulty::ElevationDifficultyHard;
    case ElevationInfo::Difficulty::Unknown:
      return ElevationDifficulty::ElevationDifficultyDisabled;
  }
  return ElevationDifficulty::ElevationDifficultyDisabled;
}

@implementation ElevationProfileData

@end

@implementation ElevationProfileData (Core)

- (instancetype)initWithElevationInfo:(ElevationInfo const &)elevationInfo
                             serverId:(NSString *)serverId
                          activePoint:(double)activePoint
                           myPosition:(double)myPosition {
  self = [super init];
  if (self) {
    _serverId = serverId;
    _trackId = elevationInfo.GetId();
    _ascent = elevationInfo.GetAscent();
    _descent = elevationInfo.GetDescent();
    _maxAttitude = elevationInfo.GetMaxAltitude();
    _minAttitude = elevationInfo.GetMinAltitude();
    _difficulty = convertDifficulty(elevationInfo.GetDifficulty());
    _trackTime = elevationInfo.GetDuration();
    NSMutableArray *pointsArray = [NSMutableArray array];
    for (auto const &p : elevationInfo.GetPoints()) {
      ElevationHeightPoint *point = [[ElevationHeightPoint alloc] initWithDistance:p.m_distance
                                                                       andAltitude:p.m_altitude];
      [pointsArray addObject:point];
    }
    _points = [pointsArray copy];
    _activePoint = activePoint;
    _myPosition = myPosition;
  }
  return self;
}


@end
