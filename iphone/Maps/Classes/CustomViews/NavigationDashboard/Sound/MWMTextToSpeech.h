#include "std/string.hpp"
#include "std/vector.hpp"

@interface MWMTextToSpeech : NSObject

+ (instancetype _Nullable)tts;
+ (void)activateAudioSession;
// Returns a list of available languages in the following format:
// * name in bcp47;
// * localized name;
- (vector<std::pair<string, string>>)availableLanguages;
- (NSString *)savedLanguage;
- (void)setNotificationsLocale:(NSString *)locale;
- (BOOL)isNeedToEnable;
- (void)setNeedToEnable:(BOOL)need;
- (BOOL)isEnable;
- (void)enable;
- (void)disable;
- (void)playTurnNotifications;

- (instancetype _Nullable)init __attribute__((unavailable("call tts instead")));
- (instancetype _Nullable)copy __attribute__((unavailable("call tts instead")));
- (instancetype _Nullable)copyWithZone:(NSZone *)zone __attribute__((unavailable("call tts instead")));
+ (instancetype _Nullable)alloc __attribute__((unavailable("call tts instead")));
+ (instancetype _Nullable)allocWithZone:(struct _NSZone *)zone __attribute__((unavailable("call tts instead")));
+ (instancetype _Nullable)new __attribute__((unavailable("call tts instead")));

@end

namespace tts
{

string bcp47ToTwineLanguage(NSString const * bcp47LangName);
string translatedTwine(string const & twine);

} // namespace tts
