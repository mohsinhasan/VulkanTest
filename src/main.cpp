/*
    A simple program to test out Vulkan
*/

#include "main.h"

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <limits.h>
#include <memory>

#include <cstring>

/// function forward definitions
void updateUniformBuffers();
VkPipelineShaderStageCreateInfo loadShader(std::string filename, VkShaderStageFlagBits shaderStage);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
VkShaderModule loadShaderGLSL(const char *filename, VkShaderStageFlagBits shaderStage);
///

/// Gloabl app instance
VulkanApp g_app;
///

///
void executeBeginCommandBuffer(uint16_t iBuffer) 
{
    /* DEPENDS on init_command_buffer() */
    VkResult result;
 
    VkCommandBufferBeginInfo cmd_buf_info = {};
    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_info.pNext = NULL;
    cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    cmd_buf_info.pInheritanceInfo = NULL;

    result = vkBeginCommandBuffer(g_app.drawCmdBuffers[iBuffer], &cmd_buf_info);

    assert(result == VK_SUCCESS);
}   

void executeEndCommandBuffer(uint16_t iBuffer) 
{
    VkResult result;

    result = vkEndCommandBuffer(g_app.drawCmdBuffers[iBuffer]);

    assert(result == VK_SUCCESS);
}

void executeQueueCommandBuffer(uint16_t iBuffer) 
{
    VkResult result;

    /* Queue the command buffer for execution */
    VkFenceCreateInfo fenceInfo;
    VkFence drawFence;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = NULL;
    fenceInfo.flags = 0;
    vkCreateFence(g_app.device, &fenceInfo, NULL, &drawFence);

    VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkSubmitInfo submit_info[1] = {};
    submit_info[0].pNext = NULL;
    submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info[0].waitSemaphoreCount = 0;
    submit_info[0].pWaitSemaphores = NULL;
    submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
    submit_info[0].commandBufferCount = 1;
    submit_info[0].pCommandBuffers = &g_app.drawCmdBuffers[iBuffer];
    submit_info[0].signalSemaphoreCount = 0;
    submit_info[0].pSignalSemaphores = NULL;

    result = vkQueueSubmit(g_app.queue, 1, submit_info, drawFence);
    assert(result == VK_SUCCESS);

    do 
    {
        result = vkWaitForFences(g_app.device, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
    } while (result == VK_TIMEOUT);

    assert(result == VK_SUCCESS);

    vkDestroyFence(g_app.device, drawFence, NULL);
}
///


void fatalError(char *message)
{
  fprintf(stderr, "main: %s\n", message);
  exit(1);
}

void redraw(void)
{

}

bool initVKInstance()
{
    bool vkDeviceInitSuccess = false;

    unsigned int extCount;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extCount);

    VkInstanceCreateInfo inst_info;
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = nullptr;
    inst_info.pApplicationInfo = nullptr;
    inst_info.enabledLayerCount = 0;
    inst_info.ppEnabledLayerNames = nullptr;
    inst_info.enabledExtensionCount = extCount;
    inst_info.ppEnabledExtensionNames = extensions;

    vkDeviceInitSuccess = (vkCreateInstance(&inst_info, nullptr, &g_app.instance) == VK_SUCCESS);

    return vkDeviceInitSuccess;
}

bool initWindow()
{
    //Initialize GLFW
    if (!glfwInit() || !glfwVulkanSupported())
    {
        printf( "GLFW could not initialize! GLFW error");
    }
    else
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        g_app.window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Vulkan Test", NULL, NULL);
    }

    glfwSetKeyCallback(g_app.window, key_callback);

    return (g_app.window != nullptr);
}

bool initVKSurface()
{
    VkResult err = glfwCreateWindowSurface(g_app.instance, g_app.window, NULL, &g_app.renderSurface);
    return ( err == VK_SUCCESS);
}

bool initVKDevice()
{
    vkEnumeratePhysicalDevices(g_app.instance, &g_app.gpuCount, nullptr);
    assert(g_app.gpuCount > 0);

    g_app.gpu.resize(g_app.gpuCount);
    vkEnumeratePhysicalDevices(g_app.instance, &g_app.gpuCount, g_app.gpu.data());

    // init gpu and memory properties
    vkGetPhysicalDeviceMemoryProperties(g_app.gpu[0], &g_app.memoryProperties);
    vkGetPhysicalDeviceProperties(g_app.gpu[0], &g_app.gpuProps);
    
    /* Call with nullptr data to get count */
    vkGetPhysicalDeviceQueueFamilyProperties(g_app.gpu[0], &g_app.queueCount, nullptr);
    assert(g_app.queueCount >= 1);

    g_app.queueProperties.resize(g_app.queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(g_app.gpu[0], &g_app.queueCount, g_app.queueProperties.data());

    // identify Graphics and present queue
    // Iterate over each queue to learn whether it supports presenting:
    std::vector<VkBool32> supportsPresent;
    supportsPresent.resize(g_app.queueCount);

    for (uint32_t i = 0; i < g_app.queueCount; i++) 
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(g_app.gpu[0], i, g_app.renderSurface, &supportsPresent[i]);
    }

    // Search for a graphics queue and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < g_app.queueCount; i++) 
    {
        if ((g_app.queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) 
        {
            if (supportsPresent[i] == VK_TRUE) 
            {
                graphicsQueueNodeIndex = i;
                break;
            }
        }
    }

    // Generate error if could not find a queue that supports both a graphics and present
    if (graphicsQueueNodeIndex == UINT32_MAX) 
    {
        printf("Could not find a queue that supports both graphics and present\n");
        assert(false);
    }

    g_app.graphicsQueueFamilyIndex = graphicsQueueNodeIndex;

    float queue_priorities[1] = {1.0};

    VkDeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = nullptr;
    queueCreateInfo.flags = 0;
    queueCreateInfo.pQueuePriorities = &queue_priorities[0];
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.queueFamilyIndex = g_app.graphicsQueueFamilyIndex;

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.ppEnabledExtensionNames = nullptr;
    deviceCreateInfo.pEnabledFeatures = nullptr;

    VkResult result = VK_SUCCESS;

    result = vkCreateDevice(g_app.gpu[0], &deviceCreateInfo, nullptr, &g_app.device);

    assert (result == VK_SUCCESS);

    vkGetDeviceQueue(g_app.device, g_app.graphicsQueueFamilyIndex, 0, &g_app.queue);

    return true;
}

void setImageLayout(uint16_t iBuffer, VkImage image,
                    VkImageAspectFlags aspectMask,
                    VkImageLayout old_image_layout,
                    VkImageLayout new_image_layout) 
{
    /* DEPENDS on info.cmd and info.queue initialized */

    assert(g_app.drawCmdBuffers[iBuffer] != VK_NULL_HANDLE);
    assert(g_app.queue != VK_NULL_HANDLE);

    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;
    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = 0;
    image_memory_barrier.oldLayout = old_image_layout;
    image_memory_barrier.newLayout = new_image_layout;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange.aspectMask = aspectMask;
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = 1;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;

    if (old_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) 
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
    {
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) 
    {
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (old_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (old_image_layout == VK_IMAGE_LAYOUT_PREINITIALIZED) 
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
    {
        image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) 
    {
        image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
    {
        image_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    vkCmdPipelineBarrier(g_app.drawCmdBuffers[iBuffer], src_stages, dest_stages, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);
}

bool initVKSwapchain()
{
    // Identify surface format
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_app.gpu[0], g_app.renderSurface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    surfaceFormats.resize(formatCount);

    vkGetPhysicalDeviceSurfaceFormatsKHR(g_app.gpu[0], g_app.renderSurface, &formatCount, &(surfaceFormats[0]));

    if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        surfaceFormats[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    }
    g_app.colorFormat = surfaceFormats[0].format;

    // identify surface capabilities
    VkSurfaceCapabilitiesKHR surfCapabilities;

    VkResult result = VK_SUCCESS;

    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_app.gpu[0], g_app.renderSurface, &surfCapabilities);
    assert(result == VK_SUCCESS);

    uint32_t presentModeCount;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(g_app.gpu[0], g_app.renderSurface, &presentModeCount, NULL);
    assert(result == VK_SUCCESS);

    std::vector<VkPresentModeKHR> presentModes;
    presentModes.resize(presentModeCount);
    
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(g_app.gpu[0], g_app.renderSurface, &presentModeCount, &presentModes[0]);
    assert(result == VK_SUCCESS);

    // Determine the number of VkImage's to use in the swap chain (we desire to
    // own only 1 image at a time, besides the images being displayed and
    // queued for display):
    uint32_t desiredNumberOfSwapChainImages = surfCapabilities.minImageCount + 1;
    if ((surfCapabilities.maxImageCount > 0) && (desiredNumberOfSwapChainImages > surfCapabilities.maxImageCount)) 
    {
        // Application must settle for fewer images than desired:
        desiredNumberOfSwapChainImages = surfCapabilities.maxImageCount;
    }

    // identify swap chain present mode 

    // If mailbox mode is available, use it, as is the lowest-latency non-
    // tearing mode.  If not, try IMMEDIATE which will usually be available,
    // and is fastest (though it tears).  If not, fall back to FIFO which is
    // always available.
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (size_t i = 0; i < presentModeCount; i++) 
    {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) 
        {
            swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
            (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)) 
        {
            swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }
 
    VkSwapchainCreateInfoKHR info;
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.pNext = nullptr;
    info.flags = 0;
    info.imageFormat = surfaceFormats[0].format;
    info.imageColorSpace = surfaceFormats[0].colorSpace;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = swapchainPresentMode;
    info.surface = g_app.renderSurface;
    info.minImageCount = desiredNumberOfSwapChainImages;
    info.imageExtent.width = SCREEN_WIDTH;
    info.imageExtent.height = SCREEN_HEIGHT;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.clipped = true;
    info.oldSwapchain = nullptr;

    result = vkCreateSwapchainKHR(g_app.device, &info, nullptr, &g_app.swapchain);
    assert(result == VK_SUCCESS);

    result = vkGetSwapchainImagesKHR(g_app.device, g_app.swapchain, &g_app.swapchainImageCount, nullptr);
    assert(result == VK_SUCCESS);

    g_app.swapBuffers.resize(g_app.swapchainImageCount);
    g_app.drawCmdBuffers.resize(g_app.swapchainImageCount);

    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(g_app.swapchainImageCount);
    
    result = vkGetSwapchainImagesKHR(g_app.device, g_app.swapchain, &g_app.swapchainImageCount, &swapchainImages[0]);
    assert(result == VK_SUCCESS);

    for (uint32_t i = 0; i < g_app.swapchainImageCount; i++) 
    {
        VkImageViewCreateInfo colorImageView = {};
        colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorImageView.pNext = NULL;
        colorImageView.format = g_app.colorFormat;
        colorImageView.components.r = VK_COMPONENT_SWIZZLE_R;
        colorImageView.components.g = VK_COMPONENT_SWIZZLE_G;
        colorImageView.components.b = VK_COMPONENT_SWIZZLE_B;
        colorImageView.components.a = VK_COMPONENT_SWIZZLE_A;
        colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageView.subresourceRange.baseMipLevel = 0;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.baseArrayLayer = 0;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageView.flags = 0;

        g_app.swapBuffers[i].image = swapchainImages[i];

        colorImageView.image = g_app.swapBuffers[i].image;

        result = vkCreateImageView(g_app.device, &colorImageView, NULL, &g_app.swapBuffers[i].view);
        assert(result == VK_SUCCESS);
    }

    return true;
}

bool memoryTypeFromProperties(uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) 
{
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < g_app.memoryProperties.memoryTypeCount; i++) 
    {
        if ((typeBits & 1) == 1) 
        {
            // Type is available, does it match user properties?
            if ((g_app.memoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) 
            {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

bool initVKDepthBuffer()
{
    VkImageCreateInfo image_info = {};
    const VkFormat depth_format = VK_FORMAT_D16_UNORM;
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(g_app.gpu[0], depth_format, &props);
    
    if (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) 
    {
        image_info.tiling = VK_IMAGE_TILING_LINEAR;
    } 
    else if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) 
    {
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    } 
    else 
    {
        /* Try other depth formats? */
        printf("VK_FORMAT_D16_UNORM Unsupported.\n");
        return false;
    }

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = NULL;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = depth_format;
    image_info.extent.width = SCREEN_WIDTH;
    image_info.extent.height = SCREEN_HEIGHT;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = NULL;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.flags = 0;

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.pNext = NULL;
    view_info.image = VK_NULL_HANDLE;
    view_info.format = depth_format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_R;
    view_info.components.g = VK_COMPONENT_SWIZZLE_G;
    view_info.components.b = VK_COMPONENT_SWIZZLE_B;
    view_info.components.a = VK_COMPONENT_SWIZZLE_A;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.flags = 0;

    g_app.depth.format = depth_format;

    /* Create image */
    VkResult result = vkCreateImage(g_app.device, &image_info, NULL, &g_app.depth.image);
    assert(result == VK_SUCCESS);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(g_app.device, g_app.depth.image, &memReqs);

    mem_alloc.allocationSize = memReqs.size;
    /* Use the memory properties to determine the type of memory required */
    bool pass = memoryTypeFromProperties(memReqs.memoryTypeBits, 0 /* No Requirements */, &mem_alloc.memoryTypeIndex); 
    assert(pass);

    /* Allocate memory */
    result = vkAllocateMemory(g_app.device, &mem_alloc, NULL, &g_app.depth.memory);
    assert(result == VK_SUCCESS);

    /* Bind memory */
    result = vkBindImageMemory(g_app.device, g_app.depth.image, g_app.depth.memory, 0);
    assert(result == VK_SUCCESS);

    /* Set the image layout to depth stencil optimal */
    setImageLayout(0, g_app.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    /* Create image view */
    view_info.image = g_app.depth.image;
    result = vkCreateImageView(g_app.device, &view_info, NULL, &g_app.depth.view);
    assert(result == VK_SUCCESS);

    return true;
}

bool initVKRenderPass()
{
    VkAttachmentDescription attachmentDescription[2];
    attachmentDescription[0].format = g_app.colorFormat;
    //attachmentDescription[0].flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    attachmentDescription[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescription[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attachmentDescription[1].format = g_app.depthFormat;
    //attachmentDescription[1].flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    attachmentDescription[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescription[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkAttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference;
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    VkRenderPassCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.attachmentCount = 2;
    info.pAttachments = attachmentDescription;
    info.subpassCount = 1;
    info.pSubpasses = &subpassDescription;
    info.dependencyCount = 0;
    info.pDependencies = nullptr;

    return (vkCreateRenderPass(g_app.device, &info, nullptr, &g_app.renderPass) == VK_SUCCESS);
}

bool initVKFrameBuffer()
{
    bool frameBufferCreateSuccess = true;

    VkImageView attachments[BUFFER_COUNT];

    attachments[1] = g_app.depth.view; 

    g_app.framebuffers.resize(g_app.swapchainImageCount);
    assert(g_app.framebuffers.size() > 0);

    VkFramebufferCreateInfo fb_info;
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext = nullptr;
    fb_info.renderPass = g_app.renderPass;
    fb_info.attachmentCount = BUFFER_COUNT;
    fb_info.pAttachments = &attachments[0];
    fb_info.width = SCREEN_WIDTH;
    fb_info.height = SCREEN_HEIGHT;
    fb_info.layers = 1;

    for (uint32_t i = 0; i < g_app.swapchainImageCount; i++)
    {
        attachments[0] = g_app.swapBuffers[i].view;
       
        frameBufferCreateSuccess &= (vkCreateFramebuffer(g_app.device, &fb_info, nullptr, &g_app.framebuffers[i]) == VK_SUCCESS);
    }

    return frameBufferCreateSuccess;
}

bool initVKCommandPool() 
{
    /* DEPENDS on init_swapchain_extension() */
    VkResult  result;

    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.pNext = NULL;
    cmd_pool_info.queueFamilyIndex = g_app.graphicsQueueFamilyIndex;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    result = vkCreateCommandPool(g_app.device, &cmd_pool_info, NULL, &g_app.cmdPool);

    assert(result == VK_SUCCESS);

    return (result == VK_SUCCESS);
}

bool initVKCommandBuffer() 
{
    VkResult  result;

    VkCommandBufferAllocateInfo cmd = {};
    cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext = NULL;
    cmd.commandPool = g_app.cmdPool;
    cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount = g_app.swapchainImageCount;

    result = vkAllocateCommandBuffers(g_app.device, &cmd, g_app.drawCmdBuffers.data());
    
    cmd.commandBufferCount = 1;
    result = vkAllocateCommandBuffers(g_app.device, &cmd, &g_app.postPresentCmdBuffer);

    assert(result == VK_SUCCESS);

    return (result == VK_SUCCESS);
}

bool initSemaphores() 
{
    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;    // VkStructureType          
    semaphore_create_info.pNext = nullptr;                                    // const void*
    semaphore_create_info.flags = 0;                                          // VkSemaphoreCreateFlags   flags

    if( (vkCreateSemaphore( g_app.device, &semaphore_create_info, nullptr, &g_app.ImageAvailableSemaphore ) != VK_SUCCESS) ||
        (vkCreateSemaphore( g_app.device, &semaphore_create_info, nullptr, &g_app.RenderingFinishedSemaphore ) != VK_SUCCESS) ) 
    {
        return false;
    }

    return true;
}

bool initVertexData()
{
    // vertices
    struct vertex
    {
        float veritces[3];
        float colors[3];
    };

    std::vector<vertex> triangleVertices = { 
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
        { {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
    };

    uint32_t vertexBufferSize = triangleVertices.size() * sizeof (vertex);

    // indices
    std::vector<uint32_t> triangleIndices = {0, 1, 2};

    uint32_t indexBufferSize = triangleIndices.size() * sizeof (uint32_t);

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    VkMemoryRequirements memReqs;

    void *data;

    // [TODO] : Later, Should add the staging path since that is the more optimal solution instead of the host visible solution below

    // create host visible memory to put data in
    VkBufferCreateInfo vertexBufferInfo = {};

    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = vertexBufferSize;
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    // vertex Buffer
    // copy data to buffer visible to host
    vkCreateBuffer(g_app.device, &vertexBufferInfo, nullptr, &g_app.vertices.buffer);
    vkGetBufferMemoryRequirements(g_app.device, g_app.vertices.buffer, &memReqs);
    mem_alloc.allocationSize = memReqs.size;
    memoryTypeFromProperties(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mem_alloc.memoryTypeIndex);
    vkAllocateMemory(g_app.device, &mem_alloc, nullptr, &g_app.vertices.memory);

    vkMapMemory(g_app.device, g_app.vertices.memory, 0, mem_alloc.allocationSize, 0, &data);
    memcpy(data, triangleVertices.data(), vertexBufferSize);
    vkUnmapMemory(g_app.device, g_app.vertices.memory);
    vkBindBufferMemory(g_app.device, g_app.vertices.buffer, g_app.vertices.memory, 0);

    // index Buffer 
    VkBufferCreateInfo indexBufferInfo = {};

    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.size = indexBufferSize;
    indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    // copy data to buffer visible to host
    vkCreateBuffer(g_app.device, &indexBufferInfo, nullptr, &g_app.indices.buffer);
    vkGetBufferMemoryRequirements(g_app.device, g_app.indices.buffer, &memReqs);
    mem_alloc.allocationSize = memReqs.size;
    memoryTypeFromProperties(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mem_alloc.memoryTypeIndex);
    vkAllocateMemory(g_app.device, &mem_alloc, nullptr, &g_app.indices.memory);

    vkMapMemory(g_app.device, g_app.indices.memory, 0, mem_alloc.allocationSize, 0, &data);
    memcpy(data, triangleIndices.data(), indexBufferSize);
    vkUnmapMemory(g_app.device, g_app.indices.memory);
    vkBindBufferMemory(g_app.device, g_app.indices.buffer, g_app.indices.memory, 0);

    return true;
}

bool initDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding vtxLayoutBinding = {};

    vtxLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vtxLayoutBinding.descriptorCount = 1;
    vtxLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    vtxLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descSetCreateInfo = {};

    descSetCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descSetCreateInfo.pNext = nullptr;
    descSetCreateInfo.bindingCount = 1;
    descSetCreateInfo.pBindings = &vtxLayoutBinding;

    vkCreateDescriptorSetLayout(g_app.device, &descSetCreateInfo, nullptr, &g_app.descriptorSetLayout);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};

    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &g_app.descriptorSetLayout;

    vkCreatePipelineLayout(g_app.device, &pipelineLayoutCreateInfo, nullptr, &g_app.pipelineLayout);

    return true;
}

bool initDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> descriptorTypes;

    // request one for now
    descriptorTypes.resize(1);

    descriptorTypes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorTypes[0].descriptorCount = 1;

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = nullptr;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = descriptorTypes.data();
    descriptorPoolInfo.maxSets = 1;

    vkCreateDescriptorPool(g_app.device, &descriptorPoolInfo, nullptr, &g_app.descriptorPool);

    return true;
}

bool initDescriptorSet()
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};

    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = g_app.descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &g_app.descriptorSetLayout;
    
    vkAllocateDescriptorSets(g_app.device, &descriptorSetAllocateInfo, &g_app.descriptorSet);

    VkWriteDescriptorSet writeDescriptorSet = {};

    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = g_app.descriptorSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &g_app.uniformDataVS.descriptor;
    writeDescriptorSet.dstBinding = 0;

    vkUpdateDescriptorSets(g_app.device, 1, &writeDescriptorSet, 0, nullptr);
    
    return true;
}

bool initPipelines()
{
    VkGraphicsPipelineCreateInfo gfxPipelineCreateInfo = {};
    gfxPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gfxPipelineCreateInfo.layout = g_app.pipelineLayout;
    gfxPipelineCreateInfo.renderPass = g_app.renderPass;

    // primitive topology for the pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;
    
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentState;
    blendAttachmentState.resize(1);
    blendAttachmentState[0].colorWriteMask = 0xf;
    blendAttachmentState[0].blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState.data();

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // enable dynamic states
    std::vector<VkDynamicState> dynamicStateEnables;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
    
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = dynamicStateEnables.size();

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {};

    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = depthStencilState.back;

    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.pSampleMask =  nullptr;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.resize(2);
    shaderStages[0] = loadShader("vertexShader.glsl", VK_SHADER_STAGE_VERTEX_BIT); 
    shaderStages[1] = loadShader("fragmentShader.glsl", VK_SHADER_STAGE_FRAGMENT_BIT);

    // assign states to pipeline
    gfxPipelineCreateInfo.stageCount = shaderStages.size();
    gfxPipelineCreateInfo.pStages = shaderStages.data();
    gfxPipelineCreateInfo.pVertexInputState = &g_app.vertices.inputState;
    gfxPipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    gfxPipelineCreateInfo.pRasterizationState = &rasterizationState;
    gfxPipelineCreateInfo.pColorBlendState = &colorBlendState;
    gfxPipelineCreateInfo.pMultisampleState = &multisampleState;
    gfxPipelineCreateInfo.pViewportState = &viewportState;
    gfxPipelineCreateInfo.pDepthStencilState = &depthStencilState;
    gfxPipelineCreateInfo.renderPass = g_app.renderPass;
    gfxPipelineCreateInfo.pDynamicState = &dynamicState;

    vkCreateGraphicsPipelines(g_app.device, g_app.pipelineCache, 1, &gfxPipelineCreateInfo, nullptr, &g_app.pipeline);

    return true;
} 

void initUniformBuffers()
{
    VkBufferCreateInfo buffCreateInfo = {};
    buffCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffCreateInfo.pNext = nullptr;
    buffCreateInfo.size = sizeof(g_app.uboVS);
    buffCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vkCreateBuffer(g_app.device, &buffCreateInfo, nullptr, &g_app.uniformDataVS.buffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(g_app.device, g_app.uniformDataVS.buffer, &memReqs);

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.pNext = nullptr;
    memAllocInfo.allocationSize = 0;
    memAllocInfo.memoryTypeIndex = 0;
    memAllocInfo.allocationSize = memReqs.size;
    if (uint32_t memoryTypeIndex = memoryTypeFromProperties(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memAllocInfo.memoryTypeIndex))
    {
        memAllocInfo.memoryTypeIndex = memoryTypeIndex;
    } 
    vkAllocateMemory(g_app.device, &memAllocInfo, nullptr, &g_app.uniformDataVS.memory);

    vkBindBufferMemory(g_app.device, g_app.uniformDataVS.buffer, g_app.uniformDataVS.memory, 0);

    g_app.uniformDataVS.descriptor.buffer = g_app.uniformDataVS.buffer;
    g_app.uniformDataVS.descriptor.offset = 0;
    g_app.uniformDataVS.descriptor.range = sizeof(g_app.uboVS);

    // update Unifrom Buffers
    updateUniformBuffers();    
}

void updateUniformBuffers()
{
    g_app.uboVS.projectionMatrix = glm::perspective(glm::radians(60.0f), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 256.0f);

    g_app.uboVS.viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, 10.0f));

    g_app.uboVS.modelMatrix = glm::mat4();
//    g_app.uboVS.modelMatrix = glm::r

    uint8_t *pData;

    vkMapMemory(g_app.device, g_app.uniformDataVS.memory, 0, sizeof(g_app.uboVS), 0, (void **)&pData);
    memcpy(pData, &g_app.uboVS, sizeof(g_app.uboVS));
    vkUnmapMemory(g_app.device, g_app.uniformDataVS.memory);
}

VkPipelineShaderStageCreateInfo loadShader(std::string filename, VkShaderStageFlagBits shaderStage)
{
    VkPipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = shaderStage;
    shaderStageInfo.pName = "main";
    shaderStageInfo.module = loadShaderGLSL(filename.c_str(), shaderStage);
    assert(shaderStageInfo.module != NULL); 
    g_app.shaderModules.push_back(shaderStageInfo.module);

    return shaderStageInfo;
}

VkShaderModule loadShaderGLSL(const char *filename, VkShaderStageFlagBits shaderStage)
{
    //TODO: Almost there....
    return 0;
}

bool initVulkan()
{
    return  initVKInstance()        &&
            initVKSurface()         &&
            initVKDevice()          &&
            initVKCommandPool()     &&
            initVKSwapchain()       &&
            initVKCommandBuffer()   &&            
            initVKDepthBuffer()     &&
            initVKRenderPass()      &&
            initVKFrameBuffer()     &&
            initSemaphores();
}

bool init()
{
    return initWindow() && initVulkan();
}

void destroyWindow()
{
    vkDestroySurfaceKHR(g_app.instance, g_app.renderSurface, nullptr);
    vkDestroySwapchainKHR(g_app.device, g_app.swapchain, nullptr);
}

void clearScreen()
{
    // clear back buffer
    VkClearColorValue clear_color = 
    {
        { 1.0f, 0.8f, 0.4f, 0.0f }
    };

    VkImageSubresourceRange image_subresource_range; 
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;                   
    image_subresource_range.baseMipLevel = 0;                                         
    image_subresource_range.levelCount = 1;                                           
    image_subresource_range.baseArrayLayer = 0;                                       
    image_subresource_range.layerCount = 1;                                                  
    
    for (uint32_t i = 0; i < g_app.swapchainImageCount; ++i) 
    {
        VkImageMemoryBarrier barrier_from_present_to_clear = {}; 
        barrier_from_present_to_clear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier_from_present_to_clear.pNext = nullptr;
        barrier_from_present_to_clear.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier_from_present_to_clear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier_from_present_to_clear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier_from_present_to_clear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier_from_present_to_clear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier_from_present_to_clear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier_from_present_to_clear.image = g_app.swapBuffers[i].image;
        barrier_from_present_to_clear.subresourceRange = image_subresource_range;

        VkImageMemoryBarrier barrier_from_clear_to_present = {}; 
        barrier_from_clear_to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;// VkStructureType                        
        barrier_from_clear_to_present.pNext = nullptr;                               // const void                            
        barrier_from_clear_to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;          // VkAccessFlags                          
        barrier_from_clear_to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;             // VkAccessFlags                          
        barrier_from_clear_to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;  // VkImageLayout                          
        barrier_from_clear_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;       // VkImageLayout                          
        barrier_from_clear_to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;               // uint32_t                               
        barrier_from_clear_to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;               // uint32_t                               
        barrier_from_clear_to_present.image = g_app.swapBuffers[i].image;            // VkImage                                
        barrier_from_clear_to_present.subresourceRange = image_subresource_range;                     // VkImageSubresourceRange                

        executeBeginCommandBuffer(i);
        //setImageLayout(i, g_app.swapBuffers[i].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkCmdPipelineBarrier( g_app.drawCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_present_to_clear );
        vkCmdClearColorImage( g_app.drawCmdBuffers[i], g_app.swapBuffers[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_subresource_range );
        vkCmdPipelineBarrier( g_app.drawCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_clear_to_present );

        executeEndCommandBuffer(i);
        
        executeQueueCommandBuffer(i); //[MH][TODO] : Removing this causes flickering. Why do we need this for correct image display?
    }
}

void createCommandBuffers()
{

}

void render()
{
    VkResult result = VK_SUCCESS;
    
    result = vkQueueWaitIdle(g_app.queue);
    assert (result == VK_SUCCESS);

    uint32_t image_index;
    
    result = vkAcquireNextImageKHR( g_app.device, g_app.swapchain, UINT64_MAX, g_app.ImageAvailableSemaphore, VK_NULL_HANDLE, &image_index );
    assert (result == VK_SUCCESS);

   /* // Add a post present image memory barrier
    // This will transform the frame buffer color attachment back
    // to it's initial layout after it has been presented to the
    // windowing system
    VkImageMemoryBarrier postPresentBarrier = {};
    postPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    postPresentBarrier.pNext = NULL;
    postPresentBarrier.srcAccessMask = 0;
    postPresentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    postPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    postPresentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    postPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postPresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    postPresentBarrier.image = g_app.swapBuffers[image_index].image;

    // Use dedicated command buffer from example base class for submitting the post present barrier
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    result = vkBeginCommandBuffer(g_app.postPresentCmdBuffer, &cmdBufInfo);
    assert (result == VK_SUCCESS);

    // Put post present barrier into command buffer
    vkCmdPipelineBarrier(
        g_app.postPresentCmdBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &postPresentBarrier);

    result = vkEndCommandBuffer(g_app.postPresentCmdBuffer);
    assert (result == VK_SUCCESS);    

    // Submit the image barrier to the current queue
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_app.postPresentCmdBuffer;

    result = vkQueueSubmit(g_app.queue, 1, &submitInfo, VK_NULL_HANDLE);
    assert (result == VK_SUCCESS);*/

    // Make sure that the image barrier command submitted to the queue 
    // has finished executing

    /* Queue the command buffer for execution */
    VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info[1] = {};
    submit_info[0].pNext = NULL;
    submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info[0].waitSemaphoreCount = 1;
    submit_info[0].pWaitSemaphores = &g_app.ImageAvailableSemaphore;
    submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
    submit_info[0].commandBufferCount = 1;
    submit_info[0].pCommandBuffers = &g_app.drawCmdBuffers[image_index];
    submit_info[0].signalSemaphoreCount = 1;
    submit_info[0].pSignalSemaphores = &g_app.RenderingFinishedSemaphore;

    result = vkQueueSubmit(g_app.queue, 1, submit_info, VK_NULL_HANDLE);
    assert(result == VK_SUCCESS);

    // swap buffers
    VkPresentInfoKHR present_info = {}; 
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;          
    present_info.pNext = nullptr;
    present_info.waitSemaphoreCount = 1;                                
    present_info.pWaitSemaphores = &g_app.RenderingFinishedSemaphore;
    present_info.swapchainCount = 1;                                    
    present_info.pSwapchains = &g_app.swapchain;                     
    present_info.pImageIndices = &image_index;                       
    present_info.pResults = nullptr;                                 

    result = vkQueuePresentKHR( g_app.queue, &present_info );

    assert (result == VK_SUCCESS);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        g_app.shouldExit = true;       
    }
} 

void mainloop()
{
    glfwPollEvents();
    
    render();
}

int main(int argc, char **argv)
{
    printf("Entering Vulkan Test program");
    init(); // init Vulkan subsystems
    printf("Vulkan init success!!!\n");

    clearScreen(); // record command buffer

    do 
    {
        mainloop();
    }
    while(!g_app.shouldExit);

    // Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(g_app.device);
        
    destroyWindow();

    printf("Exiting program");

    return 0;
}
