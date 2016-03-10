#ifndef __MAIN_H__
#define __MAIN_H__

// vulkan header includes
#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <memory>

//Screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

struct VulkanApp
{
   //The window we'll be rendering to
    GLFWwindow* window = NULL;

    VkInstance instance;

    VkPhysicalDevice gpu;
    VkDevice device;
    VkSurfaceKHR renderSurface;

    VkSwapchainKHR swapchain;
    uint32_t swapchainImageCount;//[MH][TODO]

    VkFormat color_format;
    VkFormat depth_format;

    VkRenderPass renderPass;
    std::vector<VkFramebuffer*> framebuffers;

    uint32_t queue_count;
    std::vector<VkQueueFamilyProperties> queue_props;
};

#endif //__MAIN_H__
