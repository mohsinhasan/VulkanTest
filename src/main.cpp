/*
    A simple program to test out Vulkan
*/

#include "main.h"

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <limits.h>
#include <memory>

VulkanApp g_app;

///
void executeBeginCommandBuffer() 
{
    /* DEPENDS on init_command_buffer() */
    VkResult res;
 
    VkCommandBufferBeginInfo cmd_buf_info = {};
    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_info.pNext = NULL;
    cmd_buf_info.flags = 0;
    cmd_buf_info.pInheritanceInfo = NULL;

    res = vkBeginCommandBuffer(g_app.cmd, &cmd_buf_info);

    assert(res == VK_SUCCESS);
}   

void executeEndCommandBuffer() 
{
    VkResult res;

    res = vkEndCommandBuffer(g_app.cmd);

    assert(res == VK_SUCCESS);
}

void executeQueueCommandBuffer() 
{
    VkResult res;

    /* Queue the command buffer for execution */
    const VkCommandBuffer cmd_bufs[] = {g_app.cmd};
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
    submit_info[0].pCommandBuffers = cmd_bufs;
    submit_info[0].signalSemaphoreCount = 0;
    submit_info[0].pSignalSemaphores = NULL;

    res = vkQueueSubmit(g_app.queue, 1, submit_info, drawFence);
    assert(res == VK_SUCCESS);

    do 
    {
        res = vkWaitForFences(g_app.device, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
    } while (res == VK_TIMEOUT);

    assert(res == VK_SUCCESS);

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

    float queue_priorities[1] = {0.0};

    VkDeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = nullptr;
    queueCreateInfo.flags = 0;
    queueCreateInfo.pQueuePriorities = queue_priorities;
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

    return (vkCreateDevice(g_app.gpu[0], &deviceCreateInfo, nullptr, &g_app.device) == VK_SUCCESS);
}

void setImageLayout(VkImage image,
                    VkImageAspectFlags aspectMask,
                    VkImageLayout old_image_layout,
                    VkImageLayout new_image_layout) 
{
    /* DEPENDS on info.cmd and info.queue initialized */

    assert(g_app.cmd != VK_NULL_HANDLE);
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

    vkCmdPipelineBarrier(g_app.cmd, src_stages, dest_stages, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);
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

    VkResult res = VK_SUCCESS;

    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_app.gpu[0], g_app.renderSurface, &surfCapabilities);
    assert(res == VK_SUCCESS);

    uint32_t presentModeCount;
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(g_app.gpu[0], g_app.renderSurface, &presentModeCount, NULL);
    assert(res == VK_SUCCESS);

    std::vector<VkPresentModeKHR> presentModes;
    presentModes.resize(presentModeCount);
    
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(g_app.gpu[0], g_app.renderSurface, &presentModeCount, &presentModes[0]);
    assert(res == VK_SUCCESS);

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

    res = vkCreateSwapchainKHR(g_app.device, &info, nullptr, &g_app.swapchain);
    assert(res == VK_SUCCESS);

    res = vkGetSwapchainImagesKHR(g_app.device, g_app.swapchain, &g_app.swapchainImageCount, nullptr);
    assert(res == VK_SUCCESS);

    g_app.swapBuffers.resize(g_app.swapchainImageCount);

    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(g_app.swapchainImageCount);
    
    res = vkGetSwapchainImagesKHR(g_app.device, g_app.swapchain, &g_app.swapchainImageCount, &swapchainImages[0]);
    assert(res == VK_SUCCESS);

    executeBeginCommandBuffer();
    vkGetDeviceQueue(g_app.device, g_app.graphicsQueueFamilyIndex, 0, &g_app.queue);
    
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

        setImageLayout(g_app.swapBuffers[i].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        colorImageView.image = g_app.swapBuffers[i].image;

        res = vkCreateImageView(g_app.device, &colorImageView, NULL, &g_app.swapBuffers[i].view);
        assert(res == VK_SUCCESS);
    }

    executeEndCommandBuffer();
    executeQueueCommandBuffer();

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
    VkResult res = vkCreateImage(g_app.device, &image_info, NULL, &g_app.depth.image);
    assert(res == VK_SUCCESS);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(g_app.device, g_app.depth.image, &memReqs);

    mem_alloc.allocationSize = memReqs.size;
    /* Use the memory properties to determine the type of memory required */
    bool pass = memoryTypeFromProperties(memReqs.memoryTypeBits, 0 /* No Requirements */, &mem_alloc.memoryTypeIndex); 
    assert(pass);

    /* Allocate memory */
    res = vkAllocateMemory(g_app.device, &mem_alloc, NULL, &g_app.depth.mem);
    assert(res == VK_SUCCESS);

    /* Bind memory */
    res = vkBindImageMemory(g_app.device, g_app.depth.image, g_app.depth.mem, 0);
    assert(res == VK_SUCCESS);

    /* Set the image layout to depth stencil optimal */
    setImageLayout(g_app.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    /* Create image view */
    view_info.image = g_app.depth.image;
    res = vkCreateImageView(g_app.device, &view_info, NULL, &g_app.depth.view);
    assert(res == VK_SUCCESS);

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
    VkResult  res;

    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.pNext = NULL;
    cmd_pool_info.queueFamilyIndex = g_app.graphicsQueueFamilyIndex;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    res = vkCreateCommandPool(g_app.device, &cmd_pool_info, NULL, &g_app.cmdPool);

    assert(res == VK_SUCCESS);

    return (res == VK_SUCCESS);
}

bool initVKCommandBuffer() 
{
    /* DEPENDS on init_swapchain_extension() and init_command_pool() */
    VkResult  res;

    VkCommandBufferAllocateInfo cmd = {};
    cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext = NULL;
    cmd.commandPool = g_app.cmdPool;
    cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount = 1;

    res = vkAllocateCommandBuffers(g_app.device, &cmd, &g_app.cmd);

    assert(res == VK_SUCCESS);

    return (res == VK_SUCCESS);
}

bool initSemaphores() 
{
    VkSemaphoreCreateInfo semaphore_create_info = 
    {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,      // VkStructureType          sType
      nullptr,                                      // const void*              pNext
      0                                             // VkSemaphoreCreateFlags   flags
    };

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
            initVKCommandBuffer() &&
            initVKSwapchain()   &&
            initVKDepthBuffer() &&
            initVKRenderPass()  &&
            initVKFrameBuffer() &&
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


void clear()
{
    // clear back buffer
    VkClearColorValue clear_color = 
    {
      { 1.0f, 0.8f, 0.4f, 0.0f }
    };

    VkImageSubresourceRange image_subresource_range = 
    {
      VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags                     aspectMask
      0,                                            // uint32_t                               baseMipLevel
      1,                                            // uint32_t                               levelCount
      0,                                            // uint32_t                               baseArrayLayer
      1                                             // uint32_t                               layerCount
    };
    
    for (uint32_t i = 0; i < g_app.swapchainImageCount; ++i) 
    {
        VkImageMemoryBarrier barrier_from_present_to_clear = 
        {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType                        sType
            nullptr,                                    // const void                            *pNext
            VK_ACCESS_MEMORY_READ_BIT,                  // VkAccessFlags                          srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,               // VkAccessFlags                          dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout                          oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,       // VkImageLayout                          newLayout
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                               srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                               dstQueueFamilyIndex
            g_app.swapBuffers[i].image,                 // VkImage                                image
            image_subresource_range                     // VkImageSubresourceRange                subresourceRange
        };

        VkImageMemoryBarrier barrier_from_clear_to_present = 
        {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType                        sType
            nullptr,                                    // const void                            *pNext
            VK_ACCESS_TRANSFER_WRITE_BIT,               // VkAccessFlags                          srcAccessMask
            VK_ACCESS_MEMORY_READ_BIT,                  // VkAccessFlags                          dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,       // VkImageLayout                          oldLayout
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,            // VkImageLayout                          newLayout
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                               srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                               dstQueueFamilyIndex
            g_app.swapBuffers[i].image,                 // VkImage                                image
            image_subresource_range                     // VkImageSubresourceRange                subresourceRange
        };

        executeBeginCommandBuffer();
        vkCmdPipelineBarrier( g_app.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_present_to_clear );
        vkCmdClearColorImage( g_app.cmd, g_app.swapBuffers[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_subresource_range );
        vkCmdPipelineBarrier( g_app.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_from_clear_to_present );
        executeEndCommandBuffer();
        executeQueueCommandBuffer(); //[MH][TODO] : Removing this causes flickering. Why do we need this for correct image display?
    }
}

void render()
{
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR( g_app.device, g_app.swapchain, UINT64_MAX, g_app.ImageAvailableSemaphore, VK_NULL_HANDLE, &image_index );
    assert (result == VK_SUCCESS);

    VkResult res;

    /* Queue the command buffer for execution */
    const VkCommandBuffer cmd_bufs[] = {g_app.cmd};

    VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info[1] = {};
    submit_info[0].pNext = NULL;
    submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info[0].waitSemaphoreCount = 1;
    submit_info[0].pWaitSemaphores = &g_app.ImageAvailableSemaphore;
    submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
    submit_info[0].commandBufferCount = 1;
    submit_info[0].pCommandBuffers = cmd_bufs;
    submit_info[0].signalSemaphoreCount = 1;
    submit_info[0].pSignalSemaphores = &g_app.RenderingFinishedSemaphore;

    res = vkQueueSubmit(g_app.queue, 1, submit_info, VK_NULL_HANDLE);
    assert(res == VK_SUCCESS);

    // swap buffers
    VkPresentInfoKHR present_info = 
    {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,           // VkStructureType              sType
        nullptr,                                      // const void                  *pNext
        1,                                            // uint32_t                     waitSemaphoreCount
        &g_app.RenderingFinishedSemaphore,           // const VkSemaphore           *pWaitSemaphores
        1,                                            // uint32_t                     swapchainCount
        &g_app.swapchain,                            // const VkSwapchainKHR        *pSwapchains
        &image_index,                                 // const uint32_t              *pImageIndices
        nullptr                                       // VkResult                    *pResults
    };

    result = vkQueuePresentKHR( g_app.queue, &present_info );

    assert (result == VK_SUCCESS);
}
 
int mainloop()
{
    render();

    return 1;
}
 
int main(int argc, char **argv)
{
    printf("Entering Vulkan Test program");

    if (init())
    {
        printf("Vulkan init success!!!\n");

        clear(); // record command buffer

        while(mainloop())
        {
        };
        
        destroyWindow();
    }

    printf("Exiting program");
    getchar();

    return 0;
}
