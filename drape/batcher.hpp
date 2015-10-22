#pragma once

#include "drape/pointers.hpp"
#include "drape/glstate.hpp"
#include "drape/render_bucket.hpp"
#include "drape/attribute_provider.hpp"
#include "drape/overlay_handle.hpp"

#include "base/macros.hpp"

#include "std/map.hpp"
#include "std/function.hpp"

namespace dp
{

class RenderBucket;
class AttributeProvider;
class OverlayHandle;

class Batcher
{
public:
  static uint32_t const IndexCountPerQuad;
  static uint32_t const VertexCountPerQuad;

  Batcher(uint32_t indexBufferSize = 9000, uint32_t vertexBufferSize = 10000);
  ~Batcher();

  uint32_t GetIndexBufferSize() const { return m_indexBufferSize; }
  uint32_t GetVertexBufferSize() const { return m_vertexBufferSize; }
  void SetIndexBufferSize(uint32_t indexBufferSize) { m_indexBufferSize = indexBufferSize; }
  void SetVertexBufferSize(uint32_t vertexBufferSize) { m_vertexBufferSize = vertexBufferSize; }

  void InsertTriangleList(GLState const & state, RefPointer<AttributeProvider> params);
  void InsertTriangleList(GLState const & state, RefPointer<AttributeProvider> params,
                          TransferPointer<OverlayHandle> handle);

  void InsertTriangleStrip(GLState const & state, RefPointer<AttributeProvider> params);
  void InsertTriangleStrip(GLState const & state, RefPointer<AttributeProvider> params,
                           TransferPointer<OverlayHandle> handle);

  void InsertTriangleFan(GLState const & state, RefPointer<AttributeProvider> params);
  void InsertTriangleFan(GLState const & state, RefPointer<AttributeProvider> params,
                         TransferPointer<OverlayHandle> handle);

  void InsertListOfStrip(GLState const & state, RefPointer<AttributeProvider> params, uint8_t vertexStride);
  void InsertListOfStrip(GLState const & state, RefPointer<AttributeProvider> params,
                         TransferPointer<OverlayHandle> handle, uint8_t vertexStride);

  typedef function<void (GLState const &, TransferPointer<RenderBucket> )> TFlushFn;
  void StartSession(TFlushFn const & flusher);
  void EndSession();

private:

  template<typename TBacher>
  void InsertTriangles(GLState const & state,
                       RefPointer<AttributeProvider> params,
                       TransferPointer<OverlayHandle> handle,
                       uint8_t vertexStride = 0);

  class CallbacksWrapper;
  void ChangeBuffer(RefPointer<CallbacksWrapper> wrapper, bool checkFilledBuffer);
  RefPointer<RenderBucket> GetBucket(GLState const & state);

  void FinalizeBucket(GLState const & state);
  void Flush();

private:
  TFlushFn m_flushInterface;

private:
  typedef map<GLState, MasterPointer<RenderBucket> > buckets_t;
  buckets_t m_buckets;

  uint32_t m_indexBufferSize;
  uint32_t m_vertexBufferSize;
};

class BatcherFactory
{
public:
  Batcher * GetNew() const { return new Batcher(); }
};

class SessionGuard
{
public:
  SessionGuard(Batcher & batcher, Batcher::TFlushFn const & flusher);
  ~SessionGuard();

  DISALLOW_COPY_AND_MOVE(SessionGuard)
private:
  Batcher & m_batcher;
};

} // namespace dp
