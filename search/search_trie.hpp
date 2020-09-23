#pragma once

#include "coding/string_utf8_multilang.hpp"

#include <cstdint>

namespace search
{
static const uint8_t kBigramsLang = 64;
static const uint8_t kCategoriesLang = 128;
static const uint8_t kPostcodesLang = 129;
static_assert(kBigramsLang >= StringUtf8Multilang::kMaxSupportedLanguages, "");
static_assert(kCategoriesLang >= StringUtf8Multilang::kMaxSupportedLanguages, "");
static_assert(kPostcodesLang >= StringUtf8Multilang::kMaxSupportedLanguages, "");
}  // namespace search
