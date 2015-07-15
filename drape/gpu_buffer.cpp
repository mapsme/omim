#include "drape/gpu_buffer.hpp"
#include "drape/glfunctions.hpp"
#include "drape/glextensions_list.hpp"
#include "drape/utils/gpu_mem_tracker.hpp"

#include "base/assert.hpp"

#include "std/cstring.hpp"

//#define CHECK_VBO_BOUNDS

namespace dp
{

glConst glTarget(GPUBuffer::Target t)
{
  if (t == GPUBuffer::ElementBuffer)
    return gl_const::GLArrayBuffer;

  return gl_const::GLElementArrayBuffer;
}

GPUBuffer::GPUBuffer(Target t, void const * data, uint8_t elementSize, uint32_t capacity)
  : TBase(elementSize, capacity)
  , m_t(t)
#ifdef DEBUG
  , m_isMapped(false)
#endif
{
  m_bufferID = GLFunctions::glGenBuffer();
  Resize(data, capacity);
}
GPUBuffer::~GPUBuffer()
{
  GLFunctions::glBindBuffer(0, glTarget(m_t));
  GLFunctions::glDeleteBuffer(m_bufferID);

#if defined(TRACK_GPU_MEM)
  dp::GPUMemTracker::Inst().RemoveDeallocated("VBO", m_bufferID);
#endif
}

void GPUBuffer::UploadData(void const * data, uint32_t elementCount)
{
  ASSERT(m_isMapped == false, ());

  uint32_t currentSize = GetCurrentSize();
  uint8_t elementSize = GetElementSize();
  ASSERT(GetCapacity() >= elementCount + currentSize, ("Not enough memory to upload ", elementCount, " elements"));
  Bind();

#if defined(CHECK_VBO_BOUNDS)
  int32_t size = GLFunctions::glGetBufferParameter(glTarget(m_t), gl_const::GLBufferSize);
  ASSERT_EQUAL(GetCapacity() * elementSize, size, ());
  ASSERT_LESS_OR_EQUAL((elementCount + currentSize) * elementSize, size,());
#endif

  GLFunctions::glBufferSubData(glTarget(m_t), elementCount * elementSize, data, currentSize * elementSize);
  TBase::UploadData(elementCount);

#if defined(TRACK_GPU_MEM)
  dp::GPUMemTracker::Inst().SetUsed("VBO", m_bufferID, (currentSize + elementCount) * elementSize);
#endif
}

void GPUBuffer::Bind()
{
  GLFunctions::glBindBuffer(m_bufferID, glTarget(m_t));
}

void * GPUBuffer::Map(uint32_t elementOffset, uint32_t elementCount)
{
#ifdef DEBUG
  ASSERT(m_isMapped == false, ());
  m_isMapped = true;
#endif
  uint32_t const elementSize = GetElementSize();
  uint32_t const byteOffset = elementOffset * elementSize;
  uint32_t const byteCount = elementCount * elementSize;
  return GLFunctions::glMapBufferRange(glTarget(m_t), byteOffset, byteCount);
}

void GPUBuffer::UpdateData(void * gpuPtr, void const * data, uint32_t elementOffset, uint32_t elementCount)
{
  uint32_t const elementSize = GetElementSize();
  uint32_t const byteOffset = elementOffset * elementSize;
  uint32_t const byteCount = elementCount * elementSize;
  uint32_t const byteCapacity = GetCapacity() * elementSize;
  ASSERT(m_isMapped == true, ());

#if defined(CHECK_VBO_BOUNDS)
  int32_t size = GLFunctions::glGetBufferParameter(glTarget(m_t), gl_const::GLBufferSize);
  ASSERT_EQUAL(size, byteCapacity, ());
  ASSERT_LESS(byteOffset + byteCount, size, ());
#endif

  ASSERT(gpuPtr != NULL, ());
  memcpy((uint8_t *)gpuPtr + byteOffset, data, byteCount);
}

void GPUBuffer::Unmap()
{
#ifdef DEBUG
  ASSERT(m_isMapped == true, ());
  m_isMapped = false;
#endif
  GLFunctions::glUnmapBuffer(glTarget(m_t));
}

void GPUBuffer::Resize(void const * data, uint32_t elementCount)
{
  TBase::Resize(elementCount);
  Bind();
  GLFunctions::glBufferData(glTarget(m_t), GetCapacity() * GetElementSize(), data, gl_const::GLDynamicDraw);

  // if we have set up data already (in glBufferData), we have to call SetDataSize
  if (data != nullptr)
    SetDataSize(elementCount);

#if defined(TRACK_GPU_MEM)
  dp::GPUMemTracker & memTracker = dp::GPUMemTracker::Inst();
  memTracker.RemoveDeallocated("VBO", m_bufferID);
  memTracker.AddAllocated("VBO", m_bufferID, GetCapacity() * GetElementSize());
  if (data != nullptr)
    dp::GPUMemTracker::Inst().SetUsed("VBO", m_bufferID, GetCurrentSize() * GetElementSize());
#endif
}

} // namespace dp
