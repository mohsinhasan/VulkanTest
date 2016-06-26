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
    VulkanApp()
    {
        shouldExit = 0;
    }

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
        VkDeviceMemory memory;
        VkImageView view;
    } depth;
    
    VkFormat colorFormat;
    VkFormat depthFormat;

    struct 
    {
		VkBuffer buffer;
		VkDeviceMemory memory;
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

   	struct 
    {
		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	} indices;

   	// The descriptor set layout describes the shader binding points without referencing
	// the actual buffers. 
	// Like the pipeline layout it's pretty much a blueprint and can be used with
	// different descriptor sets as long as the binding points (and shaders) match
	VkDescriptorSetLayout descriptorSetLayout;

   	// The pipeline layout defines the resource binding slots to be used with a pipeline
	// This includes bindings for buffes (ubos, ssbos), images and sampler
	// A pipeline layout can be used for multiple pipeline (state objects) as long as 
	// their shaders use the same binding layout
	VkPipelineLayout pipelineLayout;

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    uint32_t queueCount;
    std::vector<VkQueueFamilyProperties> queueProperties;
    uint32_t graphicsQueueFamilyIndex;

    VkPhysicalDeviceProperties          gpuProps;
    VkPhysicalDeviceMemoryProperties    memoryProperties;

    VkCommandPool   cmdPool;
    std::vector<VkCommandBuffer> drawCmdBuffers; // Buffer for initialization commands
    VkCommandBuffer postPresentCmdBuffer; // Buffer for initialization commands

    VkQueue queue;

    VkSemaphore    ImageAvailableSemaphore;
    VkSemaphore    RenderingFinishedSemaphore;

    bool shouldExit;
};

#endif //__MAIN_H__
