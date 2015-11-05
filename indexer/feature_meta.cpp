#include "indexer/feature_meta.hpp"

#include "std/iomanip.hpp"

namespace
{
// Special URL encoding for wikipedia:
// Replaces special characters with %HH codes
// And spaces with underscores.
string WikiUrlEncode(string const & value)
{
  ostringstream escaped;
  escaped.fill('0');
  escaped << hex;

  for (auto const & c : value)
  {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      escaped << c;
    else if (c == ' ')
      escaped << '_';
    else
      escaped << '%' << std::uppercase << setw(2)
              << static_cast<int>(static_cast<unsigned char>(c));
  }

  return escaped.str();
}
}  // namespace

namespace feature
{
string Metadata::GetWikiURL() const
{
  string value = this->Get(FMD_WIKIPEDIA);
  string::size_type i = value.find(':');
  if (i == string::npos)
    return string();
  return "https://" + value.substr(0, i) + ".wikipedia.org/wiki/" +
         WikiUrlEncode(value.substr(i + 1));
}
}  // namespace feature
