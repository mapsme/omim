#include "feature_loader_base.hpp"

#include "../coding/byte_stream.hpp"


namespace feature
{

LoaderBase::LoaderBase(SharedLoadInfo const & info)
  : m_Info(info), m_pF(0), m_Data(0)
{
}

void LoaderBase::Init(BufferT data)
{
  m_Data = data;
  m_pF = 0;

  m_CommonOffset = m_Header2Offset = 0;
  m_ptsSimpMask = 0;

  m_ptsOffsets.clear();
  m_trgOffsets.clear();
}

uint32_t LoaderBase::CalcOffset(ArrayByteSource const & source) const
{
  return static_cast<uint32_t>(source.PtrC() - DataPtr());
}

void LoaderBase::ReadOffsets(ArrayByteSource & src, uint8_t mask, offsets_t & offsets) const
{
  ASSERT ( offsets.empty(), () );
  ASSERT_GREATER ( mask, 0, () );

  offsets.resize(m_Info.GetScalesCount(), s_InvalidOffset);
  size_t ind = 0;

  while (mask > 0)
  {
    if (mask & 0x01)
      offsets[ind] = ReadVarUint<uint32_t>(src);

    ++ind;
    mask = mask >> 1;
  }
}

}
