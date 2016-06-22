/*
    A simple program to test out Vulkan
*/

#include "main.h"

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <limits.h>
#include <memory>


///
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
///

VulkanApp g_app;

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
    result = vkAllocateMemory(g_app.device, &mem_alloc, NULL, &g_app.depth.mem);
    assert(result == VK_SUCCESS);

    /* Bind memory */
    result = vkBindImageMemory(g_app.device, g_app.depth.image, g_app.depth.mem, 0);
    assert(result == VK_SUCCESS);

    /* Set the image layout to depth stencil optimal */
    //setImageLayout(0, g_app.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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

bool initVulkan()
{
    return  initVKInstance()    &&
            initVKSurface()     &&
            initVKDevice()      &&
            initVKCommandPool()   &&
            initVKSwapchain()   &&
            initVKCommandBuffer() &&            
            //initVKDepthBuffer() &&
            //initVKRenderPass()  &&
            //initVKFrameBuffer() &&
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
        
        //executeQueueCommandBuffer(i); //[MH][TODO] : Removing this causes flickering. Why do we need this for correct image display?
    }
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
        
    destroyWindow();

    printf("Exiting program");

    return 0;
}
