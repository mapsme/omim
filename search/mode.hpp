#pragma once

#include <string>

namespace search
{
enum class Mode
{
  Everywhere,
  Viewport,
  Downloader,
  Count
};

std::string DebugPrint(Mode mode);
}  // namespace search
