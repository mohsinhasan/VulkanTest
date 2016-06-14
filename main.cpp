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
    uint32_t gpuCount = 1;
    //[TODO] : Fix this based on the demo code. This is incomplete
    //also add cmd
    VkPhysicalDevice physical_device[gpuCount]; // [MH][TODO]: assuming at least one GPU for now

    vkEnumeratePhysicalDevices(g_app.instance, &gpuCount, physical_device);
    g_app.gpu = physical_device[0];

    /* Call with nullptr data to get count */
    vkGetPhysicalDeviceQueueFamilyProperties(g_app.gpu, &g_app.queueCount, nullptr);
    assert(g_app.queueCount >= 1);

    g_app.queueProperties.resize(g_app.queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(g_app.gpu, &g_app.queueCount, &(g_app.queueProperties[0]));

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfo;
    queueCreateInfo.resize(g_app.queueCount);

    float queue_priorities[1] = {0.0};
    for (uint32_t i = 0; i < g_app.queueCount; i++)
    {
        queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo[i].pNext = nullptr;
        queueCreateInfo[i].flags = 0;
        queueCreateInfo[i].pQueuePriorities = queue_priorities;
        queueCreateInfo[i].queueCount = 1;
        queueCreateInfo[i].queueFamilyIndex = i;
    }

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfo.size();
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo[0];
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.ppEnabledExtensionNames = nullptr;
    deviceCreateInfo.pEnabledFeatures = nullptr;

    return (vkCreateDevice(g_app.gpu, &deviceCreateInfo, nullptr, &g_app.device) == VK_SUCCESS);
}

void set_image_layout(VkImage image,
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

    if (old_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        image_memory_barrier.srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (old_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (old_image_layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
        image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        image_memory_barrier.srcAccessMask =
            VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        image_memory_barrier.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        image_memory_barrier.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    vkCmdPipelineBarrier(g_app.cmd, src_stages, dest_stages, 0, 0, NULL, 0, NULL,
                         1, &image_memory_barrier);
}

bool initVKSwapchain()
{
    // Identify surface format
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_app.gpu, g_app.renderSurface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    surfaceFormats.resize(formatCount);

    vkGetPhysicalDeviceSurfaceFormatsKHR(g_app.gpu, g_app.renderSurface, &formatCount, &(surfaceFormats[0]));

    if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        surfaceFormats[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    }
    g_app.colorFormat = surfaceFormats[0].format;

    // identify surface capabilities
    VkSurfaceCapabilitiesKHR surfCapabilities;

    VkResult res = VK_SUCCESS;

    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_app.gpu, g_app.renderSurface, &surfCapabilities);
    assert(res == VK_SUCCESS);

    uint32_t presentModeCount;
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(g_app.gpu, g_app.renderSurface, &presentModeCount, NULL);
    assert(res == VK_SUCCESS);

    std::vector<VkPresentModeKHR> presentModes;
    presentModes.resize(presentModeCount);
    
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(g_app.gpu, g_app.renderSurface, &presentModeCount, &presentModes[0]);
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

    //[MH][TODO] : Do I need to init cmd buffers here?
    
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

        set_image_layout(g_app.swapBuffers[i].image, VK_IMAGE_ASPECT_COLOR_BIT,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        colorImageView.image = g_app.swapBuffers[i].image;

        res = vkCreateImageView(g_app.device, &colorImageView, NULL,
                                &g_app.swapBuffers[i].view);
        assert(res == VK_SUCCESS);
    }
    
    return true;
}

bool initVKDepthBuffer()
{
    VkImageCreateInfo image_info = {};
    const VkFormat depth_format = VK_FORMAT_D16_UNORM;
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(g_app.gpu, depth_format, &props);
    
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

    VkMemoryRequirements mem_reqs;

    g_app.depth.format = depth_format;

    /* Create image */
    VkResult res = vkCreateImage(g_app.device, &image_info, NULL, &g_app.depth.image);
    assert(res == VK_SUCCESS);

    vkGetImageMemoryRequirements(g_app.device, g_app.depth.image, &mem_reqs);

    mem_alloc.allocationSize = mem_reqs.size;
    /* Use the memory properties to determine the type of memory required */
    bool pass = memory_type_from_properties(mem_reqs.memoryTypeBits, 0 /* No Requirements */, &mem_alloc.memoryTypeIndex);
    assert(pass);

    /* Allocate memory */
    res = vkAllocateMemory(g_app.device, &mem_alloc, NULL, &g_app.depth.mem);
    assert(res == VK_SUCCESS);

    /* Bind memory */
    res = vkBindImageMemory(g_app.device, g_app.depth.image, g_app.depth.mem, 0);
    assert(res == VK_SUCCESS);

    /* Set the image layout to depth stencil optimal */
    set_image_layout(g_app.depth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
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
       
        frameBufferCreateSuccess &= (vkCreateFramebuffer(g_app.device, &fb_info, nullptr, g_app.framebuffers[i]) == VK_SUCCESS);
    }

    return frameBufferCreateSuccess;
}

bool initVulkan()
{
    return  initVKInstance()    &&
            initVKSurface()     &&
            initVKDevice()      &&
            initVKSwapchain()   &&
            initVKDepthBuffer()   &&
            initVKRenderPass()  &&
            initVKFrameBuffer();
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

int mainloop()
{
    return 1;
}

int main(int argc, char **argv)
{
    int i = 0; 
    printf("Entering program");

    i = 1;

    i = 2;

    if (i > 0 && init())
    {
        printf("VK Surface creation success!!!\n");
        /*
        while(mainloop())
        {

        };
        */

        destroyWindow();
    }

    printf("Exiting program");
    //getchar();

    return 0;
}
