#pragma once

#include <string>

namespace ads
{
struct Banner
{
  enum class Type : uint8_t
  {
    None = 0,
    Facebook = 1,
    RB = 2,
    Mopub = 3
  };

  Banner() = default;
  Banner(Type t, std::string const & id) : m_type(t), m_bannerId(id) {}

  Type m_type = Type::None;
  std::string m_bannerId;
};

inline std::string DebugPrint(Banner::Type type)
{
  switch (type)
  {
  case Banner::Type::None: return "None";
  case Banner::Type::Facebook: return "Facebook";
  case Banner::Type::RB: return "RB";
  case Banner::Type::Mopub: return "Mopub";
  }
}
}  // namespace ads
