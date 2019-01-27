#include "drape/vulkan/vulkan_gpu_buffer_impl.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

#include "base/macros.hpp"

#include <cstring>

namespace dp
{
namespace vulkan
{
VulkanGPUBuffer::VulkanGPUBuffer(ref_ptr<VulkanBaseContext> context, void const * data,
                                 uint8_t elementSize, uint32_t capacity, uint64_t batcherHash)
  : BufferBase(elementSize, capacity)
  , m_batcherHash(batcherHash)
{
  m_regionsToCopy.reserve(5);
  m_barriers.reserve(5);
  Resize(context, data, capacity);
}

void * VulkanGPUBuffer::Map(ref_ptr<VulkanBaseContext> context, uint32_t elementOffset,
                            uint32_t elementCount)
{
  CHECK(m_objectManager != nullptr, ());
  VkDevice device = context->GetDevice();

  uint32_t const elementSize = GetElementSize();
  uint32_t const mappingSizeInBytes = elementCount * elementSize;
  m_mappingByteOffset = elementOffset * elementSize;

  VkCommandBuffer commandBuffer = context->GetCurrentCommandBuffer();
  CHECK(commandBuffer != nullptr, ());

  // Copy to default or temporary staging buffer.
  m_defaultStagingBuffer = m_objectManager->GetDefaultStagingBuffer(mappingSizeInBytes);
  void * gpuPtr;
  if (m_defaultStagingBuffer.m_stagingData)
  {
    gpuPtr = m_defaultStagingBuffer.m_pointer;
  }
  else
  {
    m_temporaryStagingBuffer = m_objectManager->CreateBuffer(VulkanMemoryManager::ResourceType::Staging,
                                                             mappingSizeInBytes, 0 /* batcherHash */);

    CHECK_VK_CALL(vkMapMemory(device, m_temporaryStagingBuffer.m_allocation->m_memory,
                              m_temporaryStagingBuffer.m_allocation->m_alignedOffset,
                              m_temporaryStagingBuffer.m_allocation->m_alignedSize, 0, &gpuPtr));
  }
  return gpuPtr;
}

void VulkanGPUBuffer::UpdateData(void * gpuPtr, void const * data,
                                 uint32_t elementOffset, uint32_t elementCount)
{
  CHECK(gpuPtr != nullptr, ());
  uint32_t const elementSize = GetElementSize();
  uint32_t const byteOffset = elementOffset * elementSize;
  uint32_t const byteCount = elementCount * elementSize;
  memcpy(static_cast<uint8_t *>(gpuPtr) + byteOffset, data, byteCount);

  VkBufferCopy copyRegion = {};
  copyRegion.dstOffset = m_mappingByteOffset + byteOffset;
  copyRegion.srcOffset = m_defaultStagingBuffer.m_stagingData ?
                         (m_defaultStagingBuffer.m_stagingData.m_offset + byteOffset) : byteOffset;
  copyRegion.size = byteCount;

  VkBufferMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.pNext = nullptr;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer = m_geometryBuffer.m_buffer;
  barrier.offset = copyRegion.dstOffset;
  barrier.size = copyRegion.size;

  m_regionsToCopy.push_back(std::move(copyRegion));
  m_barriers.push_back(std::move(barrier));
}

void VulkanGPUBuffer::Unmap(ref_ptr<VulkanBaseContext> context)
{
  VkDevice device = context->GetDevice();
  VkCommandBuffer commandBuffer = context->GetCurrentCommandBuffer();
  CHECK(commandBuffer != nullptr, ());

  VkBuffer stagingBuffer;
  if (m_defaultStagingBuffer.m_stagingData)
  {
    stagingBuffer = m_defaultStagingBuffer.m_stagingData.m_stagingBuffer;
  }
  else
  {
    CHECK(m_temporaryStagingBuffer.m_buffer != 0, ());
    stagingBuffer = m_temporaryStagingBuffer.m_buffer;

    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = m_temporaryStagingBuffer.m_allocation->m_memory;
    mappedRange.offset = m_temporaryStagingBuffer.m_allocation->m_alignedOffset;
    mappedRange.size = m_temporaryStagingBuffer.m_allocation->m_alignedSize;
    CHECK_VK_CALL(vkFlushMappedMemoryRanges(device, 1, &mappedRange));
    vkUnmapMemory(device, m_temporaryStagingBuffer.m_allocation->m_memory);
    CHECK_VK_CALL(vkBindBufferMemory(device, m_temporaryStagingBuffer.m_buffer,
                                     m_temporaryStagingBuffer.m_allocation->m_memory,
                                     m_temporaryStagingBuffer.m_allocation->m_alignedOffset));
  }

  // Schedule command to copy from the staging buffer to the geometry buffer.
  vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_geometryBuffer.m_buffer,
                  static_cast<uint32_t>(m_regionsToCopy.size()), m_regionsToCopy.data());

  // Set up barriers to prevent data collisions.
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr,
                       static_cast<uint32_t>(m_barriers.size()), m_barriers.data(),
                       0, nullptr);

  m_defaultStagingBuffer = {};
  m_objectManager->DestroyObject(m_temporaryStagingBuffer);
  m_mappingByteOffset = 0;
  m_regionsToCopy.clear();
  m_barriers.clear();
}

void VulkanGPUBuffer::Resize(ref_ptr<VulkanBaseContext> context, void const * data,
                             uint32_t elementCount)
{
  BufferBase::Resize(elementCount);

  m_objectManager = context->GetObjectManager();
  VkDevice device = context->GetDevice();

  uint32_t const sizeInBytes = GetCapacity() * GetElementSize();

  m_geometryBuffer = m_objectManager->CreateBuffer(VulkanMemoryManager::ResourceType::Geometry,
                                                   sizeInBytes, 0 /* batcherHash */);
  if (data != nullptr)
  {
    void * gpuPtr = nullptr;
    CHECK_VK_CALL(vkMapMemory(device, m_geometryBuffer.m_allocation->m_memory,
                              m_geometryBuffer.m_allocation->m_alignedOffset,
                              m_geometryBuffer.m_allocation->m_alignedSize, 0, &gpuPtr));
    memcpy(gpuPtr, data, sizeInBytes);
    if (!m_geometryBuffer.m_allocation->m_isCoherent)
    {
      VkMappedMemoryRange mappedRange = {};
      mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      mappedRange.memory = m_geometryBuffer.m_allocation->m_memory;
      mappedRange.offset = m_geometryBuffer.m_allocation->m_alignedOffset;
      mappedRange.size = m_geometryBuffer.m_allocation->m_alignedSize;
      CHECK_VK_CALL(vkFlushMappedMemoryRanges(device, 1, &mappedRange));
    }
    vkUnmapMemory(device, m_geometryBuffer.m_allocation->m_memory);
  }

  CHECK_VK_CALL(vkBindBufferMemory(device, m_geometryBuffer.m_buffer,
                                   m_geometryBuffer.m_allocation->m_memory,
                                   m_geometryBuffer.m_allocation->m_alignedOffset));

  // If we have already set up data, we have to call SetDataSize.
  if (data != nullptr)
    SetDataSize(elementCount);
}
}  // namespace vulkan
  
drape_ptr<DataBufferBase> DataBuffer::CreateImplForVulkan(ref_ptr<GraphicsContext> context, void const * data,
                                                          uint8_t elementSize, uint32_t capacity,
                                                          uint64_t batcherHash)
{
  return make_unique_dp<vulkan::VulkanGpuBufferImpl>(context, data, elementSize, capacity, batcherHash);
}
}  // namespace dp
