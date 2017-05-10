#pragma once

#include <cstdint>
#include <functional>


class IntervalIndexIFace
{
public:
  virtual ~IntervalIndexIFace() {}

  typedef std::function<void (uint32_t)> FunctionT;

  virtual void DoForEach(FunctionT const & f, uint64_t beg, uint64_t end) = 0;
};
