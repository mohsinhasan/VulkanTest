#ifndef __MAIN_H__
#define __MAIN_H__

// vulkan header includes
#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <memory>

//Screen dimension constants
const uint SCREEN_WIDTH = 640;
const uint SCREEN_HEIGHT = 480;

const uint BUFFER_COUNT = 2;

/* Amount of time, in nanoseconds, to wait for a command buffer to complete */
const uint FENCE_TIMEOUT = 100000000;

struct VulkanApp
{
   //The window we'll be rendering to
    GLFWwindow* window = NULL;

    VkInstance instance;
    VkDevice device;

    std::vector<VkPhysicalDevice> gpu;
    uint32_t gpuCount;
    
    VkSurfaceKHR renderSurface;
    VkSwapchainKHR swapchain;
    uint32_t swapchainImageCount;

    struct _swapChainBuffer 
    {
        VkImage image;
        VkImageView view;
    };

    std::vector<_swapChainBuffer> swapBuffers; 

    struct
    {
        VkFormat format;
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } depth;
    
    VkFormat colorFormat;
    VkFormat depthFormat;

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    uint32_t queueCount;
    std::vector<VkQueueFamilyProperties> queueProperties;
    uint32_t graphicsQueueFamilyIndex;

    VkPhysicalDeviceProperties gpuProps;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    VkCommandPool cmdPool;
    VkCommandBuffer cmd; // Buffer for initialization commands
    VkQueue queue;
};

#endif //__MAIN_H__
