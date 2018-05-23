typedef void (^MWMVoidBlock)(void);
typedef void (^MWMStringBlock)(NSString *);
typedef void (^MWMURLBlock)(NSURL *);
typedef BOOL (^MWMCheckStringBlock)(NSString *);

typedef NS_ENUM(NSUInteger, MWMDayTime) { MWMDayTimeDay, MWMDayTimeNight };

typedef NS_ENUM(NSUInteger, MWMUnits) { MWMUnitsMetric, MWMUnitsImperial };

typedef NS_ENUM(NSUInteger, MWMTheme) {
  MWMThemeDay,
  MWMThemeNight,
  MWMThemeVehicleDay,
  MWMThemeVehicleNight,
  MWMThemeAuto
};

typedef uint64_t MWMMarkID;
typedef uint64_t MWMLineID;
typedef uint64_t MWMMarkGroupID;
typedef NSArray<NSNumber *> * MWMGroupIDCollection;

typedef NS_ENUM(NSUInteger, MWMBookmarksShareStatus) {
  MWMBookmarksShareStatusSuccess,
  MWMBookmarksShareStatusEmptyCategory,
  MWMBookmarksShareStatusArchiveError,
  MWMBookmarksShareStatusFileError
};
