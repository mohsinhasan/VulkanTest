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

    VkPhysicalDevice physical_device[gpuCount]; // [MH][TODO]: assuming at least one GPU for now

    vkEnumeratePhysicalDevices(g_app.instance, &gpuCount, physical_device);
    g_app.gpu = physical_device[0];

    /* Call with nullptr data to get count */
    vkGetPhysicalDeviceQueueFamilyProperties(g_app.gpu, &g_app.queue_count, nullptr);
    assert(g_app.queue_count >= 1);

    g_app.queue_props.resize(g_app.queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_app.gpu, &g_app.queue_count, &(g_app.queue_props[0]));

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfo;
    queueCreateInfo.resize(g_app.queue_count);

    float queue_priorities[1] = {0.0};
    for (uint32_t i = 0; i < g_app.queue_count; i++)
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


bool initVKSwapchain()
{
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_app.gpu, g_app.renderSurface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    surfaceFormats.resize(formatCount);

    vkGetPhysicalDeviceSurfaceFormatsKHR(g_app.gpu, g_app.renderSurface, &formatCount, &(surfaceFormats[0]));

    if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        surfaceFormats[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    }

    g_app.color_format = surfaceFormats[0].format;

    g_app.swapchainImageCount = 2;// double buffering

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
    info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    info.surface = g_app.renderSurface;
    info.minImageCount = g_app.swapchainImageCount;
    info.imageExtent.width = SCREEN_WIDTH;
    info.imageExtent.height = SCREEN_HEIGHT;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;
    info.clipped = true;
    info.oldSwapchain = nullptr;

    vkCreateSwapchainKHR(g_app.device, &info, nullptr, &g_app.swapchain);

    VkImage images[2];

    return (vkGetSwapchainImagesKHR(g_app.device, g_app.swapchain, &g_app.swapchainImageCount, images) == VK_SUCCESS);
}

bool initVKSwapImages()
{
    return false;
}

bool initVKRenderPass()
{
    VkAttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference;
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription attachmentDescription[2];
    attachmentDescription[0].format = g_app.color_format;
    //attachmentDescription[0].flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    attachmentDescription[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescription[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attachmentDescription[1].format = g_app.depth_format;
    //attachmentDescription[1].flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    attachmentDescription[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescription[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


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

    VkImageView attachments[2];
    //attachments[1] = g_app.depth.view;[MH][TODO]

    VkFramebufferCreateInfo fb_info;

    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext = nullptr;
    fb_info.renderPass = g_app.renderPass;
    fb_info.attachmentCount = 2;
    fb_info.pAttachments = attachments;
    fb_info.width = SCREEN_WIDTH;
    fb_info.height = SCREEN_HEIGHT;
    fb_info.layers = 1;

    g_app.framebuffers.resize(g_app.swapchainImageCount);
    assert(g_app.framebuffers.size() > 0);

    for (uint32_t i = 0; i < g_app.swapchainImageCount; i++)
    {
        //attachments[0] = g_app.buffers[i].view; // [MH][TODO]
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
            initVKSwapImages()  &&
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
    getchar();

    return 0;
}
