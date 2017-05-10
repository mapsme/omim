#pragma once

#include "drape/pointers.hpp"
#include "drape_frontend/render_group.hpp"

namespace dp
{
class VertexArrayBuffer;
}

namespace df
{

class BatchMergeHelper
{
public:
  static bool IsMergeSupported();

  static void MergeBatches(std::vector<drape_ptr<RenderGroup>> & batches,
                           std::vector<drape_ptr<RenderGroup>> & mergedBatches,
                           bool isPerspective);
};

}
