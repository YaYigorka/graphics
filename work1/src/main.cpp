#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "geometry.hpp"
#include "math.hpp"

namespace {

using namespace cg;

constexpr uint32_t WIDTH = 1000;
constexpr uint32_t HEIGHT = 700;
constexpr int MAX_FRAMES_IN_FLIGHT = 1;
constexpr float PI = 3.14159265358979323846f;

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open file: " + filename);
    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

struct QueueFamilies { uint32_t graphics = UINT32_MAX, present = UINT32_MAX; };
struct SwapSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> modes;
};

class Application {
public:
    void run() {
        initWindow();
        initVulkan();
        initImGui();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapFormat{};
    VkExtent2D swapExtent{};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapViews;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailable{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinished{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlight{};
    size_t currentFrame = 0;
    bool framebufferResized = false;
    float rotation = 0.0f;

    void check(VkResult result, const char* message) {
        if (result != VK_SUCCESS) throw std::runtime_error(message);
    }

    void initWindow() {
        if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Computer Graphics - Dodecahedron", nullptr, nullptr);
        if (!window) throw std::runtime_error("Window creation failed");
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int, int) {
            static_cast<Application*>(glfwGetWindowUserPointer(w))->framebufferResized = true;
        });
    }

    void initVulkan() {
        createInstance();
        check(glfwCreateWindowSurface(instance, window, nullptr, &surface), "Surface creation failed");
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createImageViews();
        createRenderPass();
        createDepthResources();
        createFramebuffers();
        createGraphicsPipeline();
        createCommandPool();
        createGeometryBuffers();
        createCommandBuffers();
        createSyncObjects();
    }

    void createInstance() {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Dodecahedron";
        app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app.pEngineName = "Simple Vulkan Renderer";
        app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app.apiVersion = VK_API_VERSION_1_0;

        uint32_t count = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo = &app;
        info.enabledExtensionCount = count;
        info.ppEnabledExtensionNames = extensions;
        check(vkCreateInstance(&info, nullptr, &instance), "Vulkan instance creation failed");
    }

    QueueFamilies findQueueFamilies(VkPhysicalDevice candidate) {
        QueueFamilies result;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());
        for (uint32_t i = 0; i < count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) result.graphics = i;
            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &supported);
            if (supported) result.present = i;
        }
        return result;
    }

    SwapSupport querySwapSupport(VkPhysicalDevice candidate) {
        SwapSupport support;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate, surface, &support.capabilities);
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &count, nullptr);
        support.formats.resize(count);
        if (count) vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &count, support.formats.data());
        vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &count, nullptr);
        support.modes.resize(count);
        if (count) vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &count, support.modes.data());
        return support;
    }

    bool suitable(VkPhysicalDevice candidate) {
        QueueFamilies q = findQueueFamilies(candidate);
        SwapSupport s = querySwapSupport(candidate);
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, extensions.data());
        bool hasSwapchain = false;
        for (const auto& e : extensions)
            if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) hasSwapchain = true;
        return q.graphics != UINT32_MAX && q.present != UINT32_MAX && hasSwapchain &&
               !s.formats.empty() && !s.modes.empty();
    }

    void pickPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (!count) throw std::runtime_error("No Vulkan-compatible GPU found");
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());
        for (auto candidate : devices) if (suitable(candidate)) { physicalDevice = candidate; break; }
        if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No suitable Vulkan GPU found");
    }

    void createLogicalDevice() {
        QueueFamilies q = findQueueFamilies(physicalDevice);
        std::set<uint32_t> unique = {q.graphics, q.present};
        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queues;
        for (uint32_t family : unique) {
            VkDeviceQueueCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            info.queueFamilyIndex = family;
            info.queueCount = 1;
            info.pQueuePriorities = &priority;
            queues.push_back(info);
        }
        const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkPhysicalDeviceFeatures features{};
        VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        info.queueCreateInfoCount = static_cast<uint32_t>(queues.size());
        info.pQueueCreateInfos = queues.data();
        info.pEnabledFeatures = &features;
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = extensions;
        check(vkCreateDevice(physicalDevice, &info, nullptr, &device), "Logical device creation failed");
        vkGetDeviceQueue(device, q.graphics, 0, &graphicsQueue);
        vkGetDeviceQueue(device, q.present, 0, &presentQueue);
    }

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
        for (const auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
        return formats[0];
    }

    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
        for (auto mode : modes) if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) return capabilities.currentExtent;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
        return extent;
    }

    void createSwapchain() {
        SwapSupport support = querySwapSupport(physicalDevice);
        VkSurfaceFormatKHR format = chooseSurfaceFormat(support.formats);
        VkPresentModeKHR mode = choosePresentMode(support.modes);
        swapExtent = chooseExtent(support.capabilities);
        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount && imageCount > support.capabilities.maxImageCount)
            imageCount = support.capabilities.maxImageCount;
        QueueFamilies q = findQueueFamilies(physicalDevice);
        uint32_t families[] = {q.graphics, q.present};
        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface;
        info.minImageCount = imageCount;
        info.imageFormat = format.format;
        info.imageColorSpace = format.colorSpace;
        info.imageExtent = swapExtent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (q.graphics != q.present) {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = families;
        } else info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = support.capabilities.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = mode;
        info.clipped = VK_TRUE;
        check(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "Swapchain creation failed");
        swapFormat = format.format;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapImages.data());
    }

    void createImageViews() {
        swapViews.resize(swapImages.size());
        for (size_t i = 0; i < swapImages.size(); ++i) {
            VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            info.image = swapImages[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = swapFormat;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount = 1;
            check(vkCreateImageView(device, &info, nullptr, &swapViews[i]), "Image view creation failed");
        }
    }

    void createRenderPass() {
        VkAttachmentDescription color{};
        color.format = swapFormat;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depth{};
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = dependency.srcStageMask;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        std::array<VkAttachmentDescription, 2> attachments = {color, depth};
        VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        info.attachmentCount = static_cast<uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        check(vkCreateRenderPass(device, &info, nullptr, &renderPass), "Render pass creation failed");
    }

    uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memory{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memory);
        for (uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if ((filter & (1u << i)) && (memory.memoryTypes[i].propertyFlags & properties) == properties) return i;
        throw std::runtime_error("Suitable Vulkan memory type not found");
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
                     VkImage& image, VkDeviceMemory& memory) {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.extent = {width, height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.format = format;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage = usage;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateImage(device, &info, nullptr, &image), "Image creation failed");
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device, image, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(device, &allocation, nullptr, &memory), "Image memory allocation failed");
        vkBindImageMemory(device, image, memory, 0);
    }

    void createDepthResources() {
        createImage(swapExtent.width, swapExtent.height, VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImage, depthMemory);
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image = depthImage;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = VK_FORMAT_D32_SFLOAT;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        check(vkCreateImageView(device, &info, nullptr, &depthView), "Depth image view creation failed");
    }

    void createFramebuffers() {
        framebuffers.resize(swapViews.size());
        for (size_t i = 0; i < swapViews.size(); ++i) {
            VkImageView attachments[] = {swapViews[i], depthView};
            VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            info.renderPass = renderPass;
            info.attachmentCount = 2;
            info.pAttachments = attachments;
            info.width = swapExtent.width;
            info.height = swapExtent.height;
            info.layers = 1;
            check(vkCreateFramebuffer(device, &info, nullptr, &framebuffers[i]), "Framebuffer creation failed");
        }
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule module;
        check(vkCreateShaderModule(device, &info, nullptr, &module), "Shader module creation failed");
        return module;
    }

    void createGraphicsPipeline() {
        auto vertCode = readFile(std::string(SHADER_DIR) + "/vertex.vert.spv");
        auto fragCode = readFile(std::string(SHADER_DIR) + "/fragment.frag.spv");
        VkShaderModule vert = createShaderModule(vertCode);
        VkShaderModule frag = createShaderModule(fragCode);
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";

        VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attribute{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 1;
        vertexInput.pVertexAttributeDescriptions = &attribute;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.lineWidth = 1.0f;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;
        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;
        VkPushConstantRange push{};
        push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push.offset = 0;
        push.size = sizeof(Mat4);
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &push;
        check(vkCreatePipelineLayout(device, &layout, nullptr, &pipelineLayout), "Pipeline layout creation failed");
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depth;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = pipelineLayout;
        info.renderPass = renderPass;
        info.subpass = 0;
        check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline), "Graphics pipeline creation failed");
        vkDestroyShaderModule(device, frag, nullptr);
        vkDestroyShaderModule(device, vert, nullptr);
    }

    void createCommandPool() {
        QueueFamilies q = findQueueFamilies(physicalDevice);
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.queueFamilyIndex = q.graphics;
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        check(vkCreateCommandPool(device, &info, nullptr, &commandPool), "Command pool creation failed");
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &info, nullptr, &buffer), "Buffer creation failed");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkAllocateMemory(device, &allocation, nullptr, &memory), "Buffer memory allocation failed");
        vkBindBufferMemory(device, buffer, memory, 0);
    }

    void createGeometryBuffers() {
        auto vertices = createVertices();
        auto indices = createIndices(vertices);
        if (indices.size() != 108) throw std::runtime_error("Dodecahedron geometry generation failed");
        indexCount = static_cast<uint32_t>(indices.size());
        VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
        createBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexMemory);
        void* data;
        vkMapMemory(device, vertexMemory, 0, vertexSize, 0, &data);
        std::memcpy(data, vertices.data(), static_cast<size_t>(vertexSize));
        vkUnmapMemory(device, vertexMemory);
        VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();
        createBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexMemory);
        vkMapMemory(device, indexMemory, 0, indexSize, 0, &data);
        std::memcpy(data, indices.data(), static_cast<size_t>(indexSize));
        vkUnmapMemory(device, indexMemory);
    }

    void createCommandBuffers() {
        commandBuffers.resize(framebuffers.size());
        VkCommandBufferAllocateInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        info.commandPool = commandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        check(vkAllocateCommandBuffers(device, &info, commandBuffers.data()), "Command buffer allocation failed");
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            check(vkCreateSemaphore(device, &semaphore, nullptr, &imageAvailable[i]), "Semaphore creation failed");
            check(vkCreateSemaphore(device, &semaphore, nullptr, &renderFinished[i]), "Semaphore creation failed");
            check(vkCreateFence(device, &fence, nullptr, &inFlight[i]), "Fence creation failed");
        }
    }

    void initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window, true);
        std::array<VkDescriptorPoolSize, 11> sizes{{
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
        }};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool.maxSets = 1000 * static_cast<uint32_t>(sizes.size());
        pool.poolSizeCount = static_cast<uint32_t>(sizes.size());
        pool.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device, &pool, nullptr, &imguiPool), "ImGui descriptor pool creation failed");

        QueueFamilies q = findQueueFamilies(physicalDevice);
        ImGui_ImplVulkan_InitInfo init{};
        init.Instance = instance;
        init.PhysicalDevice = physicalDevice;
        init.Device = device;
        init.QueueFamily = q.graphics;
        init.Queue = graphicsQueue;
        init.PipelineCache = VK_NULL_HANDLE;
        init.DescriptorPool = imguiPool;
        init.Subpass = 0;
        init.MinImageCount = static_cast<uint32_t>(swapImages.size());
        init.ImageCount = static_cast<uint32_t>(swapImages.size());
        init.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        if (!ImGui_ImplVulkan_Init(&init, renderPass)) throw std::runtime_error("ImGui Vulkan initialization failed");

        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandPool = commandPool;
        alloc.commandBufferCount = 1;
        VkCommandBuffer cmd;
        check(vkAllocateCommandBuffers(device, &alloc, &cmd), "Font command buffer allocation failed");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    }

    void recreateSwapchain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(device);
        for (auto framebuffer : framebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffers.clear();
        if (depthView) vkDestroyImageView(device, depthView, nullptr);
        if (depthImage) vkDestroyImage(device, depthImage, nullptr);
        if (depthMemory) vkFreeMemory(device, depthMemory, nullptr);
        for (auto view : swapViews) vkDestroyImageView(device, view, nullptr);
        swapViews.clear();
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        createSwapchain();
        createImageViews();
        createDepthResources();
        createFramebuffers();
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        createCommandBuffers();
        ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(swapImages.size()));
        framebufferResized = false;
    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlight[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
            imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) throw std::runtime_error("Failed to acquire swapchain image");

        vkResetFences(device, 1, &inFlight[currentFrame]);
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        VkCommandBuffer cmd = commandBuffers[imageIndex];
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(cmd, &begin), "Command buffer begin failed");

        VkClearValue clear[2]{};
        clear[0].color = {{0.055f, 0.065f, 0.09f, 1.0f}};
        clear[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo render{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        render.renderPass = renderPass;
        render.framebuffer = framebuffers[imageIndex];
        render.renderArea.extent = swapExtent;
        render.clearValueCount = 2;
        render.pClearValues = clear;
        vkCmdBeginRenderPass(cmd, &render, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{0.0f, 0.0f, static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, swapExtent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        Mat4 model = multiply(rotationY(rotation), rotationX(rotation * 0.55f));
        Mat4 view = lookAt({0.0f, 0.0f, 4.2f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        Mat4 proj = perspective(45.0f * PI / 180.0f,
            static_cast<float>(swapExtent.width) / static_cast<float>(swapExtent.height), 0.1f, 100.0f);
        Mat4 mvp = multiply(proj, multiply(view, model));
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &mvp);
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRenderPass(cmd);
        check(vkEndCommandBuffer(cmd), "Command buffer end failed");

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailable[currentFrame];
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished[currentFrame];
        check(vkQueueSubmit(graphicsQueue, 1, &submit, inFlight[currentFrame]), "Queue submission failed");

        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished[currentFrame];
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &imageIndex;
        result = vkQueuePresentKHR(presentQueue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
            recreateSwapchain();
        else if (result != VK_SUCCESS) throw std::runtime_error("Presentation failed");
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Dodecahedron");
            ImGui::Text("Variant 10 - regular dodecahedron");
            ImGui::Text("Vertices: 20    Faces: 12    Triangles: 36");
            ImGui::Text("Vulkan + GLFW + Dear ImGui");
            ImGui::End();
            ImGui::Render();
            rotation += 0.008f;
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) rotation -= 0.025f;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) rotation += 0.025f;
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }

    void cleanup() {
        if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (imguiPool) vkDestroyDescriptorPool(device, imguiPool, nullptr);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (imageAvailable[i]) vkDestroySemaphore(device, imageAvailable[i], nullptr);
            if (renderFinished[i]) vkDestroySemaphore(device, renderFinished[i], nullptr);
            if (inFlight[i]) vkDestroyFence(device, inFlight[i], nullptr);
        }
        if (vertexBuffer) vkDestroyBuffer(device, vertexBuffer, nullptr);
        if (vertexMemory) vkFreeMemory(device, vertexMemory, nullptr);
        if (indexBuffer) vkDestroyBuffer(device, indexBuffer, nullptr);
        if (indexMemory) vkFreeMemory(device, indexMemory, nullptr);
        if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
        for (auto framebuffer : framebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
        if (depthView) vkDestroyImageView(device, depthView, nullptr);
        if (depthImage) vkDestroyImage(device, depthImage, nullptr);
        if (depthMemory) vkFreeMemory(device, depthMemory, nullptr);
        for (auto view : swapViews) vkDestroyImageView(device, view, nullptr);
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
        if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
        if (device) vkDestroyDevice(device, nullptr);
        if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }
};

} // namespace

int main() {
    Application app;
    try {
        app.run();
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
