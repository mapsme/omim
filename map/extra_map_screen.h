#pragma once

#include "../geometry/point2d.hpp"

#include "../std/vector.hpp"
#include "../std/function.hpp"
#include "../std/atomic.hpp"

class Framework;
// @todo(VB) It's a first appraximation of interface for using current render policy
// for async redering of any parts of map. Consider is as a prototype.
class ExtraMapScreen
{
public:
  // @todo(VB) This structure should be redesined: use OGL types; use array instead of vector
  struct MapImage
  {
    size_t m_width, m_height, m_bpp;
    vector<char> m_bytes;
  };

  typedef function<void (MapImage const &)> TImageReadyCallback;

  ExtraMapScreen(Framework * framework);

  void SetRenderAsyncCallback(TImageReadyCallback imageReady);
  void RenderAsync(m2::PointD const & center, size_t scale, size_t width, size_t height);

private:
  Framework * m_framework;
};
