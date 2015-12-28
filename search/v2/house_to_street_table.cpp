#include "search/v2/house_to_street_table.hpp"

#include "search/mwm_traits.hpp"

#include "indexer/index.hpp"

#include "coding/fixed_bits_ddvector.hpp"
#include "coding/reader.hpp"

#include "base/assert.hpp"

#include "defines.hpp"

namespace search
{
namespace v2
{
namespace
{
class Fixed3BitsTable : public HouseToStreetTable
{
public:
  using TVector = FixedBitsDDVector<3, ModelReaderPtr>;

  Fixed3BitsTable(MwmValue & value)
    : m_vector(TVector::Create(value.m_cont.GetReader(SEARCH_ADDRESS_FILE_TAG)))
  {
    ASSERT(m_vector.get(), ("Can't instantiate FixedBitsDDVector."));
  }

  uint32_t Get(uint32_t houseId) const override { return m_vector->Get(houseId); }

private:
  unique_ptr<TVector> m_vector;
};

class DummyTable : public HouseToStreetTable
{
public:
  uint32_t Get(uint32_t /* houseId */) const override { return 0; }
};
}  // namespace

unique_ptr<HouseToStreetTable> HouseToStreetTable::Load(MwmValue & value)
{
  MwmTraits traits(value.GetMwmVersion().format);
  auto const format = traits.GetHouseToStreetTableFormat();

  unique_ptr<HouseToStreetTable> result;

  try
  {
    if (format == MwmTraits::HouseToStreetTableFormat::Fixed3BitsDDVector)
      result.reset(new Fixed3BitsTable(value));
  }
  catch (Reader::OpenException & e)
  {
    LOG(LWARNING, (e.Msg()));
  }

  if (!result)
    result.reset(new DummyTable());
  return result;
}
}  // namespace v2
}  // namespace search
