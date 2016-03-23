#include "drape/batcher.hpp"
#include "drape/batcher_helpers.hpp"
#include "drape/cpu_buffer.hpp"
#include "drape/glextensions_list.hpp"
#include "drape/index_storage.hpp"
#include "drape/vertex_array_buffer.hpp"

#include "base/assert.hpp"
#include "base/stl_add.hpp"

#include "std/bind.hpp"

namespace dp
{

class Batcher::CallbacksWrapper : public BatchCallbacks
{
public:
  CallbacksWrapper(GLState const & state, ref_ptr<OverlayHandle> overlay, ref_ptr<Batcher> batcher)
    : m_state(state)
    , m_overlay(overlay)
    , m_batcher(batcher)
  {
  }

  void SetVAO(ref_ptr<VertexArrayBuffer> buffer)
  {
    // invocation with non-null VAO will cause to invalid range of indices.
    // It means that VAO has been changed during batching
    if (m_buffer != nullptr)
      m_vaoChanged = true;

    m_buffer = buffer;
    m_indicesRange.m_idxStart = m_buffer->GetIndexCount();
  }

  void FlushData(BindingInfo const & info, void const * data, uint32_t count) override
  {
    if (m_overlay != nullptr && info.IsDynamic())
    {
      uint32_t offset = m_buffer->GetDynamicBufferOffset(info);
      m_overlay->AddDynamicAttribute(info, offset, count);
    }
    m_buffer->UploadData(info, data, count);
  }

  void * GetIndexStorage(uint32_t size, uint32_t & startIndex) override
  {
    startIndex = m_buffer->GetStartIndexValue();
    if (m_overlay == nullptr || !m_overlay->IndexesRequired())
    {
      m_indexStorage.Resize(size);
      return m_indexStorage.GetRaw();
    }
    else
      return m_overlay->IndexStorage(size);
  }

  void SubmitIndeces() override
  {
    if (m_overlay == nullptr || !m_overlay->IndexesRequired())
      m_buffer->UploadIndexes(m_indexStorage.GetRawConst(), m_indexStorage.Size());
  }

  uint32_t GetAvailableVertexCount() const override
  {
    return m_buffer->GetAvailableVertexCount();
  }

  uint32_t GetAvailableIndexCount() const override
  {
    return m_buffer->GetAvailableIndexCount();
  }

  void ChangeBuffer() override
  {
    m_batcher->ChangeBuffer(make_ref(this));
  }

  GLState const & GetState() const
  {
    return m_state;
  }

  IndicesRange const & Finish()
  {
    if (!m_vaoChanged)
      m_indicesRange.m_idxCount = m_buffer->GetIndexCount() - m_indicesRange.m_idxStart;
    else
      m_indicesRange = IndicesRange();

    return m_indicesRange;
  }

private:
  GLState const & m_state;
  ref_ptr<OverlayHandle> m_overlay;
  ref_ptr<Batcher> m_batcher;
  ref_ptr<VertexArrayBuffer> m_buffer;
  IndexStorage m_indexStorage;
  IndicesRange m_indicesRange;
  bool m_vaoChanged = false;
};

////////////////////////////////////////////////////////////////

Batcher::Batcher(uint32_t indexBufferSize, uint32_t vertexBufferSize)
  : m_indexBufferSize(indexBufferSize)
  , m_vertexBufferSize(vertexBufferSize)
{
}

Batcher::~Batcher()
{
  m_buckets.clear();
}

void Batcher::InsertTriangleList(GLState const & state, ref_ptr<AttributeProvider> params)
{
  InsertTriangleList(state, params, nullptr);
}

IndicesRange Batcher::InsertTriangleList(GLState const & state, ref_ptr<AttributeProvider> params,
                                         drape_ptr<OverlayHandle> && handle)
{
  return InsertTriangles<TriangleListBatch>(state, params, move(handle));
}

void Batcher::InsertTriangleStrip(GLState const & state, ref_ptr<AttributeProvider> params)
{
  InsertTriangleStrip(state, params, nullptr);
}

IndicesRange Batcher::InsertTriangleStrip(GLState const & state, ref_ptr<AttributeProvider> params,
                                          drape_ptr<OverlayHandle> && handle)
{
  return InsertTriangles<TriangleStripBatch>(state, params, move(handle));
}

void Batcher::InsertTriangleFan(GLState const & state, ref_ptr<AttributeProvider> params)
{
  InsertTriangleFan(state, params, nullptr);
}

IndicesRange Batcher::InsertTriangleFan(GLState const & state, ref_ptr<AttributeProvider> params,
                                        drape_ptr<OverlayHandle> && handle)
{
  return InsertTriangles<TriangleFanBatch>(state, params, move(handle));
}

void Batcher::InsertListOfStrip(GLState const & state, ref_ptr<AttributeProvider> params,
                                uint8_t vertexStride)
{
  InsertListOfStrip(state, params, nullptr, vertexStride);
}

IndicesRange Batcher::InsertListOfStrip(GLState const & state, ref_ptr<AttributeProvider> params,
                                        drape_ptr<OverlayHandle> && handle, uint8_t vertexStride)
{
  return InsertTriangles<TriangleListOfStripBatch>(state, params, move(handle), vertexStride);
}

void Batcher::StartSession(TFlushFn const & flusher)
{
  m_flushInterface = flusher;
}

void Batcher::EndSession()
{
  Flush();
  m_flushInterface = TFlushFn();
}

void Batcher::ChangeBuffer(ref_ptr<CallbacksWrapper> wrapper)
{
  GLState const & state = wrapper->GetState();
  FinalizeBucket(state);

  ref_ptr<RenderBucket> bucket = GetBucket(state);
  wrapper->SetVAO(bucket->GetBuffer());
}

ref_ptr<RenderBucket> Batcher::GetBucket(GLState const & state)
{
  TBuckets::iterator it = m_buckets.find(state);
  if (it != m_buckets.end())
    return make_ref(it->second);

  drape_ptr<VertexArrayBuffer> vao = make_unique_dp<VertexArrayBuffer>(m_indexBufferSize, m_vertexBufferSize);
  drape_ptr<RenderBucket> buffer = make_unique_dp<RenderBucket>(move(vao));
  ref_ptr<RenderBucket> result = make_ref(buffer);
  m_buckets.emplace(state, move(buffer));

  return result;
}

void Batcher::FinalizeBucket(GLState const & state)
{
  TBuckets::iterator it = m_buckets.find(state);
  ASSERT(it != m_buckets.end(), ("Have no bucket for finalize with given state"));
  drape_ptr<RenderBucket> bucket = move(it->second);
  m_buckets.erase(state);
  bucket->GetBuffer()->Preflush();
  m_flushInterface(state, move(bucket));
}

void Batcher::Flush()
{
  ASSERT(m_flushInterface != NULL, ());
  for_each(m_buckets.begin(), m_buckets.end(), [this](TBuckets::value_type & bucket)
  {
    ASSERT(bucket.second != nullptr, ());
    bucket.second->GetBuffer()->Preflush();
    m_flushInterface(bucket.first, move(bucket.second));
  });

  m_buckets.clear();
}

template <typename TBatcher>
IndicesRange Batcher::InsertTriangles(GLState const & state, ref_ptr<AttributeProvider> params,
                                      drape_ptr<OverlayHandle> && transferHandle, uint8_t vertexStride)
{
  ref_ptr<VertexArrayBuffer> vao = GetBucket(state)->GetBuffer();
  IndicesRange range;

  drape_ptr<OverlayHandle> handle = move(transferHandle);

  {
    Batcher::CallbacksWrapper wrapper(state, make_ref(handle), make_ref(this));
    wrapper.SetVAO(vao);

    TBatcher batch(wrapper);
    batch.SetIsCanDevideStreams(handle == nullptr);
    batch.SetVertexStride(vertexStride);
    batch.BatchData(params);

    range = wrapper.Finish();
  }

  if (handle != nullptr)
    GetBucket(state)->AddOverlayHandle(move(handle));

  return range;
}

Batcher * BatcherFactory::GetNew() const
{
  uint32_t const kIndexBufferSize = 5000;
  uint32_t const kVertexBufferSize = 5000;
  return new Batcher(kIndexBufferSize, kVertexBufferSize);
}

SessionGuard::SessionGuard(Batcher & batcher, const Batcher::TFlushFn & flusher)
  : m_batcher(batcher)
{
  m_batcher.StartSession(flusher);
}

SessionGuard::~SessionGuard()
{
  m_batcher.EndSession();
}

} // namespace dp
