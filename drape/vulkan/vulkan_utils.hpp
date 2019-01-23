#pragma once

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <vulkan_wrapper.h>
#include <vulkan/vulkan.h>

#include <string>

namespace dp
{
namespace vulkan
{
extern std::string GetVulkanResultString(VkResult result);
}  // namespace vulkan
}  // namespace dp

#define LOG_ERROR_VK_CALL(method, statusCode) \
  LOG(LDEBUG, ("Vulkan error:", #method, "finished with code", \
               dp::vulkan::GetVulkanResultString(statusCode)));

#define LOG_ERROR_VK(message) LOG(LDEBUG, ("Vulkan error:", message));

#define CHECK_VK_CALL(method) \
  do { \
    VkResult const statusCode = method; \
    CHECK(statusCode == VK_SUCCESS, ("Vulkan error:", #method, "finished with code", \
                                     dp::vulkan::GetVulkanResultString(statusCode))); \
  } while (false)
