#import "Macros.h"
#import "UIButton+RuntimeAttributes.h"
#import "UIColor+MapsMeColor.h"
#import "UIFont+MapsMeFonts.h"

@implementation UIButton (RuntimeAttributes)

- (void)setLocalizedText:(NSString *)localizedText {
  [self setTitle:L(localizedText) forState:UIControlStateNormal];
  [self setTitle:L(localizedText) forState:UIControlStateDisabled];
}

- (NSString *)localizedText {
  return L([self titleForState:UIControlStateNormal]);
}

- (void)setFontName:(NSString *)fontName
{
  self.titleLabel.font = [UIFont fontWithName:fontName];
}

- (void)setTextColorName:(NSString *)colorName
{
  [self setTitleColor:[UIColor colorWithName:colorName] forState:UIControlStateNormal];
}

- (void)setTextColorHighlightedName:(NSString *)colorName
{
  [self setTitleColor:[UIColor colorWithName:colorName] forState:UIControlStateHighlighted];
}

- (void)setTextColorDisabledName:(NSString *)colorName
{
  [self setTitleColor:[UIColor colorWithName:colorName] forState:UIControlStateDisabled];
}

- (void)setBackgroundColorName:(NSString *)colorName
{
  [self setBackgroundColor:[UIColor colorWithName:colorName] forState:UIControlStateNormal];
}

- (void)setBackgroundHighlightedColorName:(NSString *)colorName
{
  [self setBackgroundColor:[UIColor colorWithName:colorName] forState:UIControlStateHighlighted];
}

- (void)setBackgroundColor:(UIColor *)color forState:(UIControlState)state
{
  [self setBackgroundImage:[UIImage imageWithColor:color] forState:state];
}

@end