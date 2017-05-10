#pragma once

#include "gui_text.hpp"
#include "shape.hpp"

#include <vector>

namespace gui
{

using TUpdateDebugLabelFn = function<bool(ScreenBase const & screen, std::string & content)>;

class DebugInfoLabels : public Shape
{
public:
  DebugInfoLabels(gui::Position const & position)
    : Shape(position)
  {}

  void AddLabel(ref_ptr<dp::TextureManager> tex, std::string const & caption, TUpdateDebugLabelFn const & onUpdateFn);
  drape_ptr<ShapeRenderer> Draw(ref_ptr<dp::TextureManager> tex);

private:
  std::vector<MutableLabelDrawer::Params> m_labelsParams;
  uint32_t m_labelsCount = 0;
};

} // namespace gui
