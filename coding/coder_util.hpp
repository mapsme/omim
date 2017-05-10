#pragma once
#include "coding/coder.hpp"
#include "base/base.hpp"
#include "base/exception.hpp"
#include <string>

template <typename FixedSizeCoderT, typename SrcCharT>
void FixedDstSizeCodeToString(FixedSizeCoderT coder, SrcCharT * pSrc, size_t srcSize, std::string & dst)
{
  dst.resize(1024);
  while (true)
  {
    size_t dstUsed = coder(pSrc, srcSize, &dst[0], dst.size());
    if (dstUsed != -1)
    {
      dst.resize(dstUsed);
      return;
    }
    else
    {
      // Double dst string size.
      try { dst.resize(dst.size() * 2); }
      catch (std::exception & e)
      {
        dst.clear();
        MYTHROW(DstOutOfMemoryStringCodingException, (e.what()));
      }
    }
  }
}
