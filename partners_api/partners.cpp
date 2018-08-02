#include "partners_api/partners.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_data.hpp"

#include "base/string_utils.hpp"

#include "std/target_os.hpp"

#include <algorithm>
#include <vector>
#include <utility>

std::vector<PartnerInfo> const kPartners = {
    PartnerInfo(1, "McArthurGlen", true /* m_hasButton */,
                "https://localads.maps.me/redirects/mcarthurglen", 180705),
    PartnerInfo(2, "LuggageHero"),
    PartnerInfo(3, "BurgerKing", true /* m_hasButton */),
    PartnerInfo(4, "Adidas", true /* m_hasButton */),
    PartnerInfo(6, "AdidasOriginal", true /* m_hasButton */),
    PartnerInfo(7, "AdidasKids", true /* m_hasButton */),
    PartnerInfo(8, "CostaCoffee", true /* m_hasButton */,
                "https://localads.maps.me/redirects/costa_coffee"),
    PartnerInfo(9, "TGIFridays", true /* m_hasButton */,
                "https://localads.maps.me/redirects/tgi_fridays"),
    PartnerInfo(10, "Sportmaster", true /* m_hasButton */,
                "https://localads.maps.me/redirects/sportmaster"),
    PartnerInfo(11, "KFC", true /* m_hasButton */),
    PartnerInfo(12, "AzbukaVkusa", true /* m_hasButton */,
                "https://localads.maps.me/redirects/azbuka_vkusa"),
    PartnerInfo(13, "Shokoladnitsa", true /* m_hasButton */),
    PartnerInfo(14, "Yakitoriya", true /* m_hasButton */),
    PartnerInfo(15, "Menza", true /* m_hasButton */),
    PartnerInfo(16, "YanPrimus", true /* m_hasButton */),
    PartnerInfo(17, "GinNo", true /* m_hasButton */),
    PartnerInfo(18, "MegafonGreenDot", true /* m_hasButton */,
                "https://localads.maps.me/redirects/megafon_green"),
    PartnerInfo(19, "MegafonPurpleDot", true /* m_hasButton */,
                "https://localads.maps.me/redirects/megafon_purple"),
    PartnerInfo(20, "Megafon", true /* m_hasButton */,
                "https://localads.maps.me/redirects/megafon_airport"),
};

namespace
{
int constexpr kFakePartnerIndex = -1;
PartnerInfo const kFakePartner(kFakePartnerIndex, {});
}  // namespace

PartnerInfo::PartnerInfo(int partnerIndex, std::string && name, bool hasButton,
                         std::string && defaultBannerUrl, uint64_t minMapVersion,
                         std::string && iosBannerPlacementId,
                         std::string && androidBannerPlacementId)
  : m_partnerIndex(partnerIndex)
  , m_type("partner" + strings::to_string(partnerIndex))
  , m_name(std::move(name))
  , m_hasButton(hasButton)
  , m_defaultBannerUrl(std::move(defaultBannerUrl))
  , m_minMapVersion(minMapVersion)
  , m_iosBannerPlacementId(std::move(iosBannerPlacementId))
  , m_androidBannerPlacementId(std::move(androidBannerPlacementId))
{}

PartnerInfo::PartnerInfo(int partnerIndex, std::string && name, bool hasButton,
                         std::string && defaultBannerUrl, uint64_t minMapVersion)
  : PartnerInfo(partnerIndex, std::move(name), hasButton,
                std::move(defaultBannerUrl), minMapVersion,
                {} /* m_iosBannerPlacementId */,
                {} /* m_androidBannerPlacementId */)
{}

PartnerInfo::PartnerInfo(int partnerIndex, std::string && name, bool hasButton,
                         std::string && defaultBannerUrl)
  : PartnerInfo(partnerIndex, std::move(name), hasButton, std::move(defaultBannerUrl),
                0 /* m_minMapVersion */)
{}

PartnerInfo::PartnerInfo(int partnerIndex, std::string && name, bool hasButton)
  : PartnerInfo(partnerIndex, std::move(name), hasButton, {} /* m_defaultBannerUrl */)
{}

PartnerInfo::PartnerInfo(int partnerIndex, std::string && name)
  : PartnerInfo(partnerIndex, std::move(name), false /* hasButton */)
{}

std::string const & PartnerInfo::GetBannerPlacementId() const
{
#if defined(OMIM_OS_IPHONE)
  return m_iosBannerPlacementId;
#elif defined(OMIM_OS_ANDROID)
  return m_androidBannerPlacementId;
#endif
  static std::string kEmptyStr;
  return kEmptyStr;
}

PartnerChecker::PartnerChecker()
{
  Classificator const & c = classif();
  for (auto const & p : kPartners)
    m_types.push_back(c.GetTypeByPath({"sponsored", p.m_type}));
}

int PartnerChecker::GetPartnerIndex(FeatureType const & ft) const
{
  auto const types = feature::TypesHolder(ft);
  int index = 0;
  CHECK_EQUAL(kPartners.size(), m_types.size(), ());
  for (auto t : m_types)
  {
    if (std::find(types.begin(), types.end(), PrepareToMatch(t, 2 /* level */)) != types.end())
      return kPartners[index].m_partnerIndex;
    index++;
  }
  return kFakePartnerIndex;
}

std::vector<PartnerInfo> const & GetPartners()
{
  return kPartners;
}

PartnerInfo const & GetPartnerByIndex(int partnerIndex)
{
  auto const it = std::find_if(kPartners.cbegin(), kPartners.cend(),
                               [partnerIndex](PartnerInfo const & partherInfo)
  {
    return partnerIndex == partherInfo.m_partnerIndex;
  });

  if (it == kPartners.cend())
    return kFakePartner;
  return *it;
}
