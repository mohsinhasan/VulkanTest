#ifndef __MAIN_H__
#define __MAIN_H__

// vulkan header includes
#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <memory>

//Screen dimension constants
const uint SCREEN_WIDTH = 1280;
const uint SCREEN_HEIGHT = 720;

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

    struct {
        VkFormat format;
        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
    } depth;
    
    VkFormat colorFormat;

    struct {
		VkBuffer buffer;
		VkDeviceMemory memory;
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

   	struct {
		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	} indices;

    struct {
		VkBuffer buffer;
		VkDeviceMemory memory;
		VkDescriptorBufferInfo descriptor;
	}  uniformDataVS;

    struct {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    } uboVS;

   	// The descriptor set layout describes the shader binding points without referencing
	// the actual buffers. 
	// Like the pipeline layout it's pretty much a blueprint and can be used with
	// different descriptor sets as long as the binding points (and shaders) match
	VkDescriptorSetLayout descriptorSetLayout;

   	// Descriptor set pool
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

   	// The descriptor set stores the resources bound to the binding points in a shader
	// It connects the binding points of the different shaders with the buffers and images
	// used for those bindings
	VkDescriptorSet descriptorSet;

   	// The pipeline layout defines the resource binding slots to be used with a pipeline
	// This includes bindings for buffes (ubos, ssbos), images and sampler
	// A pipeline layout can be used for multiple pipeline (state objects) as long as 
	// their shaders use the same binding layout
	VkPipelineLayout pipelineLayout;

    VkPipelineCache pipelineCache;

   	// The pipeline (state objects) is a static store for the 3D pipeline states (including shaders)
	// Other than OpenGL this makes you setup the render states up-front
	// If different render states are required you need to setup multiple pipelines
	// and switch between them
	// Note that there are a few dynamic states (scissor, viewport, line width) that
	// can be set from a command buffer and does not have to be part of the pipeline
	// This basic example only uses one pipeline
	VkPipeline pipeline;

    std::vector<VkShaderModule> shaderModules;

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
