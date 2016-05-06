#pragma once

#include "drape/index_buffer_mutator.hpp"
#include "drape/attribute_buffer_mutator.hpp"
#include "drape/pointers.hpp"
#include "drape/index_buffer.hpp"
#include "drape/data_buffer.hpp"
#include "drape/binding_info.hpp"
#include "drape/gpu_program.hpp"

#include "std/map.hpp"

namespace df
{
class BatchMergeHelper;
}

namespace dp
{

struct IndicesRange
{
  uint32_t m_idxStart;
  uint32_t m_idxCount;

  IndicesRange()
    : m_idxStart(0), m_idxCount(0)
  {}

  IndicesRange(uint32_t idxStart, uint32_t idxCount)
    : m_idxStart(idxStart), m_idxCount(idxCount)
  {}

  bool IsValid() const { return m_idxCount != 0; }
};

class VertexArrayBuffer
{
  typedef map<BindingInfo, drape_ptr<DataBuffer> > TBuffersMap;
  friend class df::BatchMergeHelper;
public:
  VertexArrayBuffer(uint32_t indexBufferSize, uint32_t dataBufferSize);
  ~VertexArrayBuffer();

  /// This method must be call on reading thread, before VAO will be transfer on render thread
  void Preflush();

  ///{@
  /// On devices where implemented OES_vertex_array_object extensions we use it for build VertexArrayBuffer
  /// OES_vertex_array_object create OpenGL resource that belong only one GL context (which was created by)
  /// by this reason Build/Bind and Render must be called only on Frontendrendere thread
  void Render();
  void RenderRange(IndicesRange const & range);
  void Build(ref_ptr<GpuProgram> program);
  ///@}

  uint32_t GetAvailableVertexCount() const;
  uint32_t GetAvailableIndexCount() const;
  uint32_t GetStartIndexValue() const;
  uint32_t GetDynamicBufferOffset(BindingInfo const & bindingInfo);
  uint32_t GetIndexCount() const;

  void UploadData(BindingInfo const & bindingInfo, void const * data, uint32_t count);
  void UploadIndexes(void const * data, uint32_t count);

  void ApplyMutation(ref_ptr<IndexBufferMutator> indexMutator,
                     ref_ptr<AttributeBufferMutator> attrMutator);

  void ResetChangingTracking() { m_isChanged = false; }
  bool IsChanged() const { return m_isChanged; }

private:
  ref_ptr<DataBuffer> GetOrCreateStaticBuffer(BindingInfo const & bindingInfo);
  ref_ptr<DataBuffer> GetOrCreateDynamicBuffer(BindingInfo const & bindingInfo);
  ref_ptr<DataBuffer> GetDynamicBuffer(BindingInfo const & bindingInfo) const;

  ref_ptr<DataBuffer> GetOrCreateBuffer(BindingInfo const & bindingInfo, bool isDynamic);
  ref_ptr<DataBuffer> GetBuffer(BindingInfo const & bindingInfo, bool isDynamic) const;
  void Bind() const;
  void BindStaticBuffers() const;
  void BindDynamicBuffers() const;
  void BindBuffers(TBuffersMap const & buffers) const;

  ref_ptr<DataBufferBase> GetIndexBuffer() const;

  void PreflushImpl();

private:
  /// m_VAO - VertexArrayObject name/identificator
  int m_VAO;
  TBuffersMap m_staticBuffers;
  TBuffersMap m_dynamicBuffers;

  drape_ptr<IndexBuffer> m_indexBuffer;
  uint32_t m_dataBufferSize;

  ref_ptr<GpuProgram> m_program;

  bool m_isPreflushed;
  bool m_moveToGpuOnBuild;
  bool m_isChanged;
};

} // namespace dp
