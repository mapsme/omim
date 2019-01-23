#include "drape/vulkan/vulkan_object_manager.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

namespace dp
{
namespace vulkan
{
VulkanObjectManager::VulkanObjectManager(VkDevice device, VkPhysicalDeviceLimits const & deviceLimits,
                                         VkPhysicalDeviceMemoryProperties const & memoryProperties,
                                         uint32_t queueFamilyIndex)
  : m_device(device)
  , m_queueFamilyIndex(queueFamilyIndex)
  , m_memoryManager(device, deviceLimits, memoryProperties)
{
  m_queueToDestroy.reserve(50);
}

VulkanObjectManager::~VulkanObjectManager()
{
  CollectObjects();
}

VulkanObject VulkanObjectManager::CreateBuffer(VulkanMemoryManager::ResourceType resourceType,
                                               uint32_t sizeInBytes, uint64_t batcherHash)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  VulkanObject result;
  VkBufferCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.size = sizeInBytes;
  if (resourceType == VulkanMemoryManager::ResourceType::Geometry)
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  else if (resourceType == VulkanMemoryManager::ResourceType::Uniform)
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  else if (resourceType == VulkanMemoryManager::ResourceType::Staging)
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  else
    CHECK(false, ("Unsupported resource type."));

  info.usage = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 1;
  info.pQueueFamilyIndices = &m_queueFamilyIndex;
  CHECK_VK_CALL(vkCreateBuffer(m_device, &info, nullptr, &result.m_buffer));

  VkMemoryRequirements memReqs = {};
  vkGetBufferMemoryRequirements(m_device, result.m_buffer, &memReqs);

  result.m_allocation = m_memoryManager.Allocate(resourceType, memReqs, batcherHash);
  return result;
}

VulkanObject VulkanObjectManager::CreateImage(VkImageUsageFlagBits usageFlagBits, VkFormat format,
                                              VkImageAspectFlags aspectFlags, uint32_t width, uint32_t height)
{
  std::lock_guard<std::mutex> lock(m_mutex);

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
  imageCreateInfo.usage = usageFlagBits | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  CHECK_VK_CALL(vkCreateImage(m_device, &imageCreateInfo, nullptr, &result.m_image));

  VkImageViewCreateInfo viewCreateInfo = {};
  viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCreateInfo.pNext = nullptr;
  viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCreateInfo.format = format;
  viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                               VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
  viewCreateInfo.subresourceRange.baseMipLevel = 0;
  viewCreateInfo.subresourceRange.levelCount = 1;
  viewCreateInfo.subresourceRange.baseArrayLayer = 0;
  viewCreateInfo.subresourceRange.layerCount = 1;
  viewCreateInfo.image = result.m_image;
  CHECK_VK_CALL(vkCreateImageView(m_device, &viewCreateInfo, nullptr, &result.m_imageView));

  VkMemoryRequirements memReqs = {};
  vkGetImageMemoryRequirements(m_device, result.m_image, &memReqs);

  result.m_allocation = m_memoryManager.Allocate(VulkanMemoryManager::ResourceType::Image,
                                                 memReqs, 0 /* blockHash */);
  return result;
}

void VulkanObjectManager::DestroyObject(VulkanObject object)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_queueToDestroy.push_back(std::move(object));
}

void VulkanObjectManager::CollectObjects()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_queueToDestroy.empty())
    return;

  m_memoryManager.BeginDeallocationSession();
  for (size_t i = 0; i < m_queueToDestroy.size(); ++i)
  {
    if (m_queueToDestroy[i].m_buffer != 0)
      vkDestroyBuffer(m_device, m_queueToDestroy[i].m_buffer, nullptr);
    if (m_queueToDestroy[i].m_imageView != 0)
      vkDestroyImageView(m_device, m_queueToDestroy[i].m_imageView, nullptr);
    if (m_queueToDestroy[i].m_image != 0)
      vkDestroyImage(m_device, m_queueToDestroy[i].m_image, nullptr);

    if (m_queueToDestroy[i].m_allocation)
      m_memoryManager.Deallocate(m_queueToDestroy[i].m_allocation);
  }
  m_memoryManager.EndDeallocationSession();
  m_queueToDestroy.clear();
}
}  // namespace vulkan
}  // namespace dp
