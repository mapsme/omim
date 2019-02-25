#include "drape/vulkan/vulkan_object_manager.hpp"

#include "drape/drape_routine.hpp"

#include "base/macros.hpp"

#include <algorithm>
#include <cstring>

namespace dp
{
namespace vulkan
{
namespace
{
VkSamplerAddressMode GetVulkanSamplerAddressMode(TextureWrapping wrapping)
{
  switch (wrapping)
  {
  case TextureWrapping::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case TextureWrapping::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
  UNREACHABLE();
}

VkFilter GetVulkanFilter(TextureFilter filter)
{
  switch (filter)
  {
  case TextureFilter::Linear: return VK_FILTER_LINEAR;
  case TextureFilter::Nearest: return VK_FILTER_NEAREST;
  }
  UNREACHABLE();
}
}  // namespace

VulkanObjectManager::VulkanObjectManager(VkDevice device, VkPhysicalDeviceLimits const & deviceLimits,
                                         VkPhysicalDeviceMemoryProperties const & memoryProperties,
                                         uint32_t queueFamilyIndex)
  : m_device(device)
  , m_queueFamilyIndex(queueFamilyIndex)
  , m_memoryManager(device, deviceLimits, memoryProperties)
{
  for (size_t i = 0; i < RendererType::Count; ++i)
    m_queuesToDestroy[i].reserve(50);
  m_descriptorsToDestroy.reserve(50);
  CreateDescriptorPool();
}

VulkanObjectManager::~VulkanObjectManager()
{
  CollectDescriptorSetGroups();

  for (size_t i = 0; i < RendererType::Count; ++i)
    CollectObjectsImpl(m_queuesToDestroy[i]);

  for (auto const & s : m_samplers)
    vkDestroySampler(m_device, s.second, nullptr);
  m_samplers.clear();

  DestroyDescriptorPools();
}

void VulkanObjectManager::RegisterRendererThread(RendererType type)
{
  m_renderers[type] = std::this_thread::get_id();
}

VulkanObject VulkanObjectManager::CreateBuffer(VulkanMemoryManager::ResourceType resourceType,
                                               uint32_t sizeInBytes, uint64_t batcherHash)
{
  VulkanObject result;
  VkBufferCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.size = sizeInBytes;
  if (resourceType == VulkanMemoryManager::ResourceType::Geometry)
  {
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  else if (resourceType == VulkanMemoryManager::ResourceType::Uniform)
  {
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  else if (resourceType == VulkanMemoryManager::ResourceType::Staging)
  {
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  else
  {
    CHECK(false, ("Unsupported resource type."));
  }

  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 1;
  info.pQueueFamilyIndices = &m_queueFamilyIndex;
  CHECK_VK_CALL(vkCreateBuffer(m_device, &info, nullptr, &result.m_buffer));

  VkMemoryRequirements memReqs = {};
  vkGetBufferMemoryRequirements(m_device, result.m_buffer, &memReqs);

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    result.m_allocation = m_memoryManager.Allocate(resourceType, memReqs, batcherHash);
    CHECK_VK_CALL(vkBindBufferMemory(m_device, result.m_buffer, result.GetMemory(),
                                     result.GetAlignedOffset()));
  }

  return result;
}

VulkanObject VulkanObjectManager::CreateImage(VkImageUsageFlags usageFlags, VkFormat format,
                                              VkImageAspectFlags aspectFlags, uint32_t width, uint32_t height)
{
  VulkanObject result;
  VkImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.pNext = nullptr;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.format = format;
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateInfo.extent = { width, height, 1 };
  imageCreateInfo.usage = usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  CHECK_VK_CALL(vkCreateImage(m_device, &imageCreateInfo, nullptr, &result.m_image));

  VkMemoryRequirements memReqs = {};
  vkGetImageMemoryRequirements(m_device, result.m_image, &memReqs);

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    result.m_allocation = m_memoryManager.Allocate(VulkanMemoryManager::ResourceType::Image,
                                                   memReqs, 0 /* blockHash */);
    CHECK_VK_CALL(vkBindImageMemory(m_device, result.m_image,
                                    result.GetMemory(), result.GetAlignedOffset()));
  }

  VkImageViewCreateInfo viewCreateInfo = {};
  viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCreateInfo.pNext = nullptr;
  viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCreateInfo.format = format;
  if (usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
  {
    viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                 VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
  }
  else
  {
    viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                 VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  }
  viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
  viewCreateInfo.subresourceRange.baseMipLevel = 0;
  viewCreateInfo.subresourceRange.levelCount = 1;
  viewCreateInfo.subresourceRange.baseArrayLayer = 0;
  viewCreateInfo.subresourceRange.layerCount = 1;
  viewCreateInfo.image = result.m_image;
  CHECK_VK_CALL(vkCreateImageView(m_device, &viewCreateInfo, nullptr, &result.m_imageView));

  return result;
}

DescriptorSetGroup VulkanObjectManager::CreateDescriptorSetGroup(ref_ptr<VulkanGpuProgram> program)
{
  CHECK(std::this_thread::get_id() == m_renderers[RendererType::Frontend], ());

  CHECK(!m_descriptorPools.empty(), ());

  DescriptorSetGroup s;
  VkDescriptorSetLayout layout = program->GetDescriptorSetLayout();
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = s.m_descriptorPool = m_descriptorPools.front();
  allocInfo.pSetLayouts = &layout;
  allocInfo.descriptorSetCount = 1;

  auto result = vkAllocateDescriptorSets(m_device, &allocInfo, &s.m_descriptorSet);
  if (result != VK_SUCCESS)
  {
    for (size_t i = 1; i < m_descriptorPools.size(); ++i)
    {
      allocInfo.descriptorPool = s.m_descriptorPool = m_descriptorPools[i];
      result = vkAllocateDescriptorSets(m_device, &allocInfo, &s.m_descriptorSet);
      if (result == VK_SUCCESS)
        break;
    }

    if (s.m_descriptorSet == VK_NULL_HANDLE)
    {
      CreateDescriptorPool();
      allocInfo.descriptorPool = s.m_descriptorPool = m_descriptorPools.back();
      CHECK_VK_CALL(vkAllocateDescriptorSets(m_device, &allocInfo, &s.m_descriptorSet));
    }
  }
  return s;
}

void VulkanObjectManager::DestroyObject(VulkanObject object)
{
  auto const currentThreadId = std::this_thread::get_id();
  if (currentThreadId == m_renderers[RendererType::Frontend])
    m_queuesToDestroy[RendererType::Frontend].push_back(std::move(object));
  else if (currentThreadId == m_renderers[RendererType::Backend])
    m_queuesToDestroy[RendererType::Backend].push_back(std::move(object));
  else
    CHECK(false, ("Unknown thread."));
}

void VulkanObjectManager::DestroyDescriptorSetGroup(DescriptorSetGroup group)
{
  CHECK(std::this_thread::get_id() == m_renderers[RendererType::Frontend], ());
  m_descriptorsToDestroy.push_back(std::move(group));
}

void VulkanObjectManager::CollectDescriptorSetGroups()
{
  CHECK(std::this_thread::get_id() == m_renderers[RendererType::Frontend], ());
  for (auto const & d : m_descriptorsToDestroy)
  {
    CHECK_VK_CALL(vkFreeDescriptorSets(m_device, d.m_descriptorPool,
                                       1 /* count */, &d.m_descriptorSet));
  }
  m_descriptorsToDestroy.clear();
}

void VulkanObjectManager::CollectObjects()
{
  auto const currentThreadId = std::this_thread::get_id();
  if (currentThreadId == m_renderers[RendererType::Frontend])
    CollectObjectsForThread(RendererType::Frontend);
  else if (currentThreadId == m_renderers[RendererType::Backend])
    CollectObjectsForThread(RendererType::Backend);
  else
    CHECK(false, ("Unknown thread."));
}

void VulkanObjectManager::CollectObjectsForThread(RendererType type)
{
  if (m_queuesToDestroy[type].empty())
    return;

  std::vector<VulkanObject> queueToDestroy;
  std::swap(m_queuesToDestroy[type], queueToDestroy);
  DrapeRoutine::Run([this, queueToDestroy = std::move(queueToDestroy)]()
  {
    CollectObjectsImpl(queueToDestroy);
  });
}

void VulkanObjectManager::CollectObjectsImpl(std::vector<VulkanObject> const & objects)
{
  for (auto const & obj : objects)
  {
    if (obj.m_buffer != VK_NULL_HANDLE)
      vkDestroyBuffer(m_device, obj.m_buffer, nullptr);
    if (obj.m_imageView != VK_NULL_HANDLE)
      vkDestroyImageView(m_device, obj.m_imageView, nullptr);
    if (obj.m_image != VK_NULL_HANDLE)
      vkDestroyImage(m_device, obj.m_image, nullptr);
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_memoryManager.BeginDeallocationSession();
  for (auto const & obj : objects)
  {
    if (obj.m_allocation)
      m_memoryManager.Deallocate(obj.m_allocation);
  }
  m_memoryManager.EndDeallocationSession();
}

uint8_t * VulkanObjectManager::MapUnsafe(VulkanObject object)
{
  CHECK(!object.m_allocation->m_memoryBlock->m_isBlocked, ());

  CHECK(object.m_buffer != VK_NULL_HANDLE || object.m_image != VK_NULL_HANDLE, ());
  uint8_t * ptr = nullptr;
  CHECK_VK_CALL(vkMapMemory(m_device, object.GetMemory(), object.GetAlignedOffset(),
                            object.GetAlignedSize(), 0, reinterpret_cast<void **>(&ptr)));
  object.m_allocation->m_memoryBlock->m_isBlocked = true;
  return ptr;
}

void VulkanObjectManager::FlushUnsafe(VulkanObject object, uint32_t offset, uint32_t size)
{
  if (object.m_allocation->m_memoryBlock->m_isCoherent)
    return;

  CHECK(object.m_allocation->m_memoryBlock->m_isBlocked, ());

  VkMappedMemoryRange mappedRange = {};
  mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  mappedRange.memory = object.GetMemory();
  mappedRange.offset = object.GetAlignedOffset() + offset;
  if (size == 0)
    mappedRange.size = object.GetAlignedSize();
  else
    mappedRange.size = mappedRange.offset + size;
  CHECK_VK_CALL(vkFlushMappedMemoryRanges(m_device, 1, &mappedRange));
}

void VulkanObjectManager::UnmapUnsafe(VulkanObject object)
{
  CHECK(object.m_allocation->m_memoryBlock->m_isBlocked, ());
  vkUnmapMemory(m_device, object.GetMemory());
  object.m_allocation->m_memoryBlock->m_isBlocked = false;
}

void VulkanObjectManager::Fill(VulkanObject object, void const * data, uint32_t sizeInBytes)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  void * gpuPtr = MapUnsafe(object);
  if (data != nullptr)
    memcpy(gpuPtr, data, sizeInBytes);
  else
    memset(gpuPtr, 0, sizeInBytes);
  FlushUnsafe(object);
  UnmapUnsafe(object);
}

void VulkanObjectManager::CreateDescriptorPool()
{
  // Maximum uniform buffers descriptors count per frame.
  uint32_t constexpr kMaxUniformBufferDescriptorsCount = 500;
  // Maximum textures descriptors count per frame.
  uint32_t constexpr kMaxImageSamplerDescriptorsCount = 1000;

  std::vector<VkDescriptorPoolSize> poolSizes =
  {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kMaxUniformBufferDescriptorsCount},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxImageSamplerDescriptorsCount},
  };

  VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  descriptorPoolInfo.pPoolSizes = poolSizes.data();
  descriptorPoolInfo.maxSets = kMaxUniformBufferDescriptorsCount + kMaxImageSamplerDescriptorsCount;

  VkDescriptorPool descriptorPool;
  CHECK_VK_CALL(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &descriptorPool));
  m_descriptorPools.push_back(descriptorPool);
}

void VulkanObjectManager::DestroyDescriptorPools()
{
  for (auto & pool : m_descriptorPools)
    vkDestroyDescriptorPool(m_device, pool, nullptr);
}

VkSampler VulkanObjectManager::GetSampler(SamplerKey const & key)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto const it = m_samplers.find(key);
  if (it != m_samplers.end())
    return it->second;

  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = samplerCreateInfo.minFilter = GetVulkanFilter(key.GetTextureFilter());
  samplerCreateInfo.addressModeU = GetVulkanSamplerAddressMode(key.GetWrapSMode());
  samplerCreateInfo.addressModeV = GetVulkanSamplerAddressMode(key.GetWrapTMode());

  VkSampler sampler;
  CHECK_VK_CALL(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &sampler));

  m_samplers.insert(std::make_pair(key, sampler));
  return sampler;
}
}  // namespace vulkan
}  // namespace dp
