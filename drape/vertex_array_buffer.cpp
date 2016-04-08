#include "drape/vertex_array_buffer.hpp"
#include "drape/glfunctions.hpp"
#include "drape/glextensions_list.hpp"
#include "drape/index_storage.hpp"
#include "drape/support_manager.hpp"

#include "base/stl_add.hpp"
#include "base/assert.hpp"

namespace dp
{

VertexArrayBuffer::VertexArrayBuffer(uint32_t indexBufferSize, uint32_t dataBufferSize)
  : m_VAO(0)
  , m_dataBufferSize(dataBufferSize)
  , m_program()
  , m_isPreflushed(false)
  , m_moveToGpuOnBuild(false)
  , m_isChanged(false)
{
  m_indexBuffer = make_unique_dp<IndexBuffer>(indexBufferSize);

  // Adreno 200 GPUs aren't able to share OpenGL resources between 2 OGL-contexts correctly,
  // so we have to create and destroy VBO on one context.
  m_moveToGpuOnBuild = SupportManager::Instance().IsAdreno200Device();
}

VertexArrayBuffer::~VertexArrayBuffer()
{
  m_indexBuffer.reset();
  m_staticBuffers.clear();
  m_dynamicBuffers.clear();

  if (m_VAO != 0)
  {
    /// Build called only when VertexArrayBuffer fulled and transfer to FrontendRenderer
    /// but if user move screen before all geometry readed from MWM we delete VertexArrayBuffer on BackendRenderer
    /// in this case m_VAO will be equal a 0
    /// also m_VAO == 0 will be on device that not support OES_vertex_array_object extension
    GLFunctions::glDeleteVertexArray(m_VAO);
  }
}

void VertexArrayBuffer::Preflush()
{
  if (!m_moveToGpuOnBuild)
    PreflushImpl();
}

void VertexArrayBuffer::PreflushImpl()
{
  ASSERT(!m_isPreflushed, ());

  // Buffers are ready, so moving them from CPU to GPU.
  for(auto & buffer : m_staticBuffers)
    buffer.second->MoveToGPU(GPUBuffer::ElementBuffer);

  for(auto & buffer : m_dynamicBuffers)
    buffer.second->MoveToGPU(GPUBuffer::ElementBuffer);

  ASSERT(m_indexBuffer != nullptr, ());
  m_indexBuffer->MoveToGPU(GPUBuffer::IndexBuffer);

  GLFunctions::glBindBuffer(0, gl_const::GLElementArrayBuffer);
  GLFunctions::glBindBuffer(0, gl_const::GLArrayBuffer);

  m_isPreflushed = true;
}

void VertexArrayBuffer::Render()
{
  RenderRange(IndicesRange(0, GetIndexBuffer()->GetCurrentSize()));
}

void VertexArrayBuffer::RenderRange(IndicesRange const & range)
{
  if (!(m_staticBuffers.empty() && m_dynamicBuffers.empty()) && GetIndexCount() > 0)
  {
    ASSERT(m_program != nullptr, ("Somebody not call Build. It's very bad. Very very bad"));
    /// if OES_vertex_array_object is supported than all bindings already saved in VAO
    /// and we need only bind VAO.
    if (GLExtensionsList::Instance().IsSupported(GLExtensionsList::VertexArrayObject))
      Bind();
    else
      BindStaticBuffers();

    BindDynamicBuffers();
    GetIndexBuffer()->Bind();
    GLFunctions::glDrawElements(dp::IndexStorage::SizeOfIndex(), range.m_idxCount, range.m_idxStart);
  }
}

void VertexArrayBuffer::Build(ref_ptr<GpuProgram> program)
{
  if (m_moveToGpuOnBuild && !m_isPreflushed)
    PreflushImpl();

  if (m_VAO != 0 && m_program == program)
      return;

  m_program = program;
  /// if OES_vertex_array_object not supported, than buffers will be bind on each Render call
  if (!GLExtensionsList::Instance().IsSupported(GLExtensionsList::VertexArrayObject))
    return;

  if (m_staticBuffers.empty())
    return;

  if (m_VAO != 0)
    GLFunctions::glDeleteVertexArray(m_VAO);
  m_VAO = GLFunctions::glGenVertexArray();
  Bind();
  BindStaticBuffers();
}

void VertexArrayBuffer::UploadData(BindingInfo const & bindingInfo, void const * data, uint32_t count)
{
  ref_ptr<DataBuffer> buffer;
  if (!bindingInfo.IsDynamic())
    buffer = GetOrCreateStaticBuffer(bindingInfo);
  else
    buffer = GetOrCreateDynamicBuffer(bindingInfo);

  if (count > 0)
    m_isChanged = true;
  buffer->GetBuffer()->UploadData(data, count);
}

ref_ptr<DataBuffer> VertexArrayBuffer::GetOrCreateDynamicBuffer(BindingInfo const & bindingInfo)
{
  return GetOrCreateBuffer(bindingInfo, true);
}

ref_ptr<DataBuffer> VertexArrayBuffer::GetDynamicBuffer(BindingInfo const & bindingInfo) const
{
  return GetBuffer(bindingInfo, true);
}

ref_ptr<DataBuffer> VertexArrayBuffer::GetOrCreateStaticBuffer(BindingInfo const & bindingInfo)
{
  return GetOrCreateBuffer(bindingInfo, false);
}

ref_ptr<DataBuffer> VertexArrayBuffer::GetBuffer(BindingInfo const & bindingInfo, bool isDynamic) const
{
  TBuffersMap const * buffers = nullptr;
  if (isDynamic)
    buffers = &m_dynamicBuffers;
  else
    buffers = &m_staticBuffers;

  TBuffersMap::const_iterator it = buffers->find(bindingInfo);
  if (it == buffers->end())
    return nullptr;

  return make_ref(it->second);
}

ref_ptr<DataBuffer> VertexArrayBuffer::GetOrCreateBuffer(BindingInfo const & bindingInfo, bool isDynamic)
{
  TBuffersMap * buffers = nullptr;
  if (isDynamic)
    buffers = &m_dynamicBuffers;
  else
    buffers = &m_staticBuffers;

  TBuffersMap::iterator it = buffers->find(bindingInfo);
  if (it == buffers->end())
  {
    drape_ptr<DataBuffer> dataBuffer = make_unique_dp<DataBuffer>(bindingInfo.GetElementSize(), m_dataBufferSize);
    ref_ptr<DataBuffer> result = make_ref(dataBuffer);
    (*buffers).insert(make_pair(bindingInfo, move(dataBuffer)));
    return result;
  }

  return make_ref(it->second);
}

uint32_t VertexArrayBuffer::GetAvailableIndexCount() const
{
  return GetIndexBuffer()->GetAvailableSize();
}

uint32_t VertexArrayBuffer::GetAvailableVertexCount() const
{
  if (m_staticBuffers.empty())
    return m_dataBufferSize;

#ifdef DEBUG
  TBuffersMap::const_iterator it = m_staticBuffers.begin();
  uint32_t prev = it->second->GetBuffer()->GetAvailableSize();
  for (; it != m_staticBuffers.end(); ++it)
    ASSERT_EQUAL(prev, it->second->GetBuffer()->GetAvailableSize(), ());
#endif

  return m_staticBuffers.begin()->second->GetBuffer()->GetAvailableSize();
}

uint32_t VertexArrayBuffer::GetStartIndexValue() const
{
  if (m_staticBuffers.empty())
    return 0;

#ifdef DEBUG
  TBuffersMap::const_iterator it = m_staticBuffers.begin();
  uint32_t prev = it->second->GetBuffer()->GetCurrentSize();
  for (; it != m_staticBuffers.end(); ++it)
    ASSERT(prev == it->second->GetBuffer()->GetCurrentSize(), ());
#endif

  return m_staticBuffers.begin()->second->GetBuffer()->GetCurrentSize();
}

uint32_t VertexArrayBuffer::GetDynamicBufferOffset(BindingInfo const & bindingInfo)
{
  return GetOrCreateDynamicBuffer(bindingInfo)->GetBuffer()->GetCurrentSize();
}

uint32_t VertexArrayBuffer::GetIndexCount() const
{
  return GetIndexBuffer()->GetCurrentSize();
}

void VertexArrayBuffer::UploadIndexes(void const * data, uint32_t count)
{
  ASSERT(count <= GetIndexBuffer()->GetAvailableSize(), ());
  GetIndexBuffer()->UploadData(data, count);
}

void VertexArrayBuffer::ApplyMutation(ref_ptr<IndexBufferMutator> indexMutator,
                                      ref_ptr<AttributeBufferMutator> attrMutator)
{
  /// We need to bind current VAO before calling glBindBuffer if OES_vertex_array_object is supported.
  /// Otherwise we risk affecting a previously binded VAO.
  if (GLExtensionsList::Instance().IsSupported(GLExtensionsList::VertexArrayObject))
    Bind();

  if (indexMutator != nullptr)
  {
    ASSERT(m_indexBuffer != nullptr, ());
    if (indexMutator->GetCapacity() > m_indexBuffer->GetBuffer()->GetCapacity())
    {
      m_indexBuffer = make_unique_dp<IndexBuffer>(indexMutator->GetCapacity());
      m_indexBuffer->MoveToGPU(GPUBuffer::IndexBuffer);
    }
    m_indexBuffer->UpdateData(indexMutator->GetIndexes(), indexMutator->GetIndexCount());
  }

  if (attrMutator == nullptr)
    return;

  typedef AttributeBufferMutator::TMutateData TMutateData;
  typedef AttributeBufferMutator::TMutateNodes TMutateNodes;
  TMutateData const & data = attrMutator->GetMutateData();
  for (TMutateData::const_iterator it = data.begin(); it != data.end(); ++it)
  {
    ref_ptr<DataBuffer> buffer = GetDynamicBuffer(it->first);
    ASSERT(buffer != nullptr, ());
    DataBufferMapper mapper(buffer);
    TMutateNodes const & nodes = it->second;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
      MutateNode const & node = nodes[i];
      ASSERT_GREATER(node.m_region.m_count, 0, ());
      mapper.UpdateData(node.m_data.get(), node.m_region.m_offset, node.m_region.m_count);
    }
  }
}

void VertexArrayBuffer::Bind() const
{
  ASSERT(m_VAO != 0, ("You need to call Build method before bind it and render"));
  GLFunctions::glBindVertexArray(m_VAO);
}

void VertexArrayBuffer::BindStaticBuffers() const
{
  BindBuffers(m_staticBuffers);
}

void VertexArrayBuffer::BindDynamicBuffers() const
{
  BindBuffers(m_dynamicBuffers);
}

void VertexArrayBuffer::BindBuffers(TBuffersMap const & buffers) const
{
  TBuffersMap::const_iterator it = buffers.begin();
  for (; it != buffers.end(); ++it)
  {
    BindingInfo const & binding = it->first;
    ref_ptr<DataBuffer> buffer = make_ref(it->second);
    buffer->GetBuffer()->Bind();

    for (uint16_t i = 0; i < binding.GetCount(); ++i)
    {
      BindingDecl const & decl = binding.GetBindingDecl(i);
      int8_t attributeLocation = m_program->GetAttributeLocation(decl.m_attributeName);
      assert(attributeLocation != -1);
      GLFunctions::glEnableVertexAttribute(attributeLocation);
      GLFunctions::glVertexAttributePointer(attributeLocation,
                                            decl.m_componentCount,
                                            decl.m_componentType,
                                            false,
                                            decl.m_stride,
                                            decl.m_offset);
    }
  }
}

ref_ptr<DataBufferBase> VertexArrayBuffer::GetIndexBuffer() const
{
  ASSERT(m_indexBuffer != nullptr, ());
  return m_indexBuffer->GetBuffer();
}

} // namespace dp
