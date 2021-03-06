#define VK_FUNCTION( fun ) PFN_##fun fun;
VK_FUNCTION(vkGetInstanceProcAddr)
VK_FUNCTION(vkGetDeviceProcAddr)
VK_FUNCTION(vkEnumerateInstanceLayerProperties)
VK_FUNCTION(vkEnumerateInstanceExtensionProperties)
VK_FUNCTION(vkCreateInstance)
VK_FUNCTION(vkEnumeratePhysicalDevices)
VK_FUNCTION(vkGetPhysicalDeviceProperties)
VK_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
VK_FUNCTION(vkGetPhysicalDeviceFeatures)
VK_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)
VK_FUNCTION(vkEnumerateDeviceExtensionProperties)
VK_FUNCTION(vkCreateDevice)
#ifdef VK_USE_PLATFORM_XLIB_KHR
    VK_FUNCTION(vkCreateXlibSurfaceKHR)
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    VK_FUNCTION(vkCreateXcbSurfaceKHR)
#endif
VK_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)
VK_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR)
VK_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR)
VK_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
VK_FUNCTION(vkCreateSwapchainKHR)
VK_FUNCTION(vkGetDeviceQueue)
VK_FUNCTION(vkCreateRenderPass)
VK_FUNCTION(vkCreatePipelineLayout)
VK_FUNCTION(vkCreateGraphicsPipelines)
VK_FUNCTION(vkGetSwapchainImagesKHR)
VK_FUNCTION(vkCreateImageView)
VK_FUNCTION(vkCreateShaderModule)
VK_FUNCTION(vkCreateCommandPool)
VK_FUNCTION(vkAllocateCommandBuffers)
VK_FUNCTION(vkBeginCommandBuffer)
VK_FUNCTION(vkCmdBeginRenderPass)
VK_FUNCTION(vkCmdBindPipeline)
VK_FUNCTION(vkCmdDraw)
VK_FUNCTION(vkCmdEndRenderPass)
VK_FUNCTION(vkEndCommandBuffer)
VK_FUNCTION(vkCreateFramebuffer)
VK_FUNCTION(vkCreateSemaphore)
VK_FUNCTION(vkAcquireNextImageKHR)
VK_FUNCTION(vkQueueSubmit)
VK_FUNCTION(vkQueuePresentKHR)
VK_FUNCTION(vkDeviceWaitIdle)
VK_FUNCTION(vkQueueWaitIdle)
VK_FUNCTION(vkDestroyCommandPool)
VK_FUNCTION(vkDestroyPipeline)
VK_FUNCTION(vkDestroyPipelineLayout)
VK_FUNCTION(vkDestroyRenderPass)
VK_FUNCTION(vkDestroyShaderModule)
VK_FUNCTION(vkDestroyImageView)
VK_FUNCTION(vkDestroySwapchainKHR)
VK_FUNCTION(vkDestroyDevice)
VK_FUNCTION(vkDestroySurfaceKHR)
VK_FUNCTION(vkDestroyInstance)
VK_FUNCTION(vkDestroySemaphore)
VK_FUNCTION(vkDestroyFramebuffer)