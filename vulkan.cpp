#include <dlfcn.h>
#include <stdio.h>
#include <map>
#include <vector>
#include <algorithm>
#include <limits>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define VK_NO_PROTOTYPES
#ifdef USE_GLFW
	#define VK_USE_PLATFORM_XLIB_KHR
	#define GLFW_INCLUDE_VULKAN
	#define GLFW_EXPOSE_NATIVE_X11
	#include <GLFW/glfw3.h>
	#include <GLFW/glfw3native.h>
#elif defined(USE_XCB)
	#define VK_USE_PLATFORM_XCB_KHR
	#include <xcb/xcb.h>
#elif defined(USE_XLIB)
	#define VK_USE_PLATFORM_XLIB_KHR
	#include <X11/Xlib.h>
#endif

#include <vulkan/vulkan.h>
#include "VulkanFunctions.h"

void* VULKAN_LIBRARY;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    #define LoadProcAddress GetProcAddress
#elif defined(VK_USE_PLATFORM_XCB_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR)
    #define LoadProcAddress dlsym
#endif

#define VK_EXPORTED_FUNCTION( fun )                                     \
    if( !(fun = (PFN_##fun)LoadProcAddress( VULKAN_LIBRARY, #fun )) ) { \
      printf("Could not load exported function: %s!\n", #fun);          \
      return false;                                                     \
    }

//use only AFTER VK_EXPORTED_FUNCTION( vkGetInstanceProcAddr )
#define VK_LOAD_INSTANCE_FUNCTION( source, fun ) \
	if (!(fun = (PFN_##fun)vkGetInstanceProcAddr( source, #fun ))) {  \
		printf("Could not load function: %s!\n", #fun);               \
		throw VulkanException("Loading func error");		          \
	}

//use only AFTER VK_LOAD_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
#define VK_LOAD_DEVICE_FUNCTION( source, fun ) \
	if (!(fun = (PFN_##fun)vkGetDeviceProcAddr( source, #fun ))) {	 \
		printf("Could not load function: %s!\n", #fun);              \
		throw VulkanException("Loading func error");			     \
	}

#define VK_DESTROY_INSTANCE_FUNCTION ( source, obj, fun ) \
    if ( obj != VK_NULL_HANDLE) {                         \
        VK_LOAD_INSTANCE_FUNCTION(instance , fun);        \
        fun(source, obj, nullptr);                        \
    }

#define VK_DESTROY_DEVICE_FUNCTION ( source, obj, fun ) \
    if ( obj != VK_NULL_HANDLE) {                       \
        VK_LOAD_DEVICE_FUNCTION(instance , fun);        \
        fun(source, obj, nullptr);                      \
    }

#ifdef USE_GLFW
	GLFWwindow* window;
	const int WIDTH = 800;
	const int HEIGHT = 600;
#elif defined(USE_XCB)
	xcb_connection_t *c;
	xcb_screen_t *screen;
	xcb_drawable_t window;
	xcb_gcontext_t foreground;
	xcb_gcontext_t background;
	xcb_generic_event_t *e;
	uint32_t mask = 0;
	uint32_t values[2];
#elif defined(USE_XLIB)
	Display *display;
	Window window;
	XEvent e;
	int s;
#endif

void create_window()
{
#ifdef USE_GLFW
	glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
#elif defined(USE_XCB)
	c = xcb_connect (NULL, NULL);
    screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;
    window = screen->root;
    window = xcb_generate_id(c);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = screen->white_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;
    xcb_create_window (c,                              /* connection    */
                    //  XCB_COPY_FROM_PARENT,          /* depth         */
                    //  window,                        /* window Id     */
                    //  screen->root,                  /* parent window */
                    //  0, 0,                          /* x, y          */
                    //  150, 150,                      /* width, height */
                    //  10,                            /* border_width  */
                    //  XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class         */
                    //  screen->root_visual,           /* visual        */
                    //  mask, values);                 /* masks         */
    xcb_map_window(c, window);
    xcb_flush(c);
#elif defined(USE_XLIB)
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    s = DefaultScreen(display);
    window = XCreateSimpleWindow(display, RootWindow(display, s), 10, 10, 100, 100, 1,
                           BlackPixel(display, s), WhitePixel(display, s));
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
#endif
}

static
std::map<char, std::string> vk_return_codes =
{
    {0, "VK_SUCCESS"},
    {1, "VK_NOT_READY"},
    {2, "VK_TIMEOUT"},
    {3, "VK_EVENT_SET"},
    {4, "VK_EVENT_RESET"},
    {5, "VK_INCOMPLETE"},
    {-1, "VK_ERROR_OUT_OF_HOST_MEMORY"},
    {-2, "VK_ERROR_OUT_OF_DEVICE_MEMORY"},
    {-3, "VK_ERROR_INITIALIZATION_FAILED"},
    {-4, "VK_ERROR_DEVICE_LOST"},
    {-5, "VK_ERROR_MEMORY_MAP_FAILED"},
    {-6, "VK_ERROR_LAYER_NOT_PRESENT"},
    {-7, "VK_ERROR_EXTENSION_NOT_PRESENT"},
    {-8, "VK_ERROR_FEATURE_NOT_PRESENT"},
    {-9, "VK_ERROR_INCOMPATIBLE_DRIVER"},
    {-10, "VK_ERROR_TOO_MANY_OBJECTS"},
    {-11, "VK_ERROR_FORMAT_NOT_SUPPORTED"},
    {-12, "VK_ERROR_FRAGMENTED_POOL"}
};

class VulkanException : std::runtime_error
{
public:
	template <typename T>
	explicit VulkanException(const T arg) : std::runtime_error(arg)
	{}
};

void vkCheckResult(const char code)
{
	if (code) {
		const char* const code_str = vk_return_codes[code].c_str();
		printf("Return code: %d - %s\n", code, code_str);
		throw(VulkanException(code_str));
	}
}

void window_main_loop(VkDevice& logical_device, VkSwapchainKHR& swapChain,
    VkSemaphore& imageAvailableSemaphore,
    VkSemaphore& renderFinishedSemaphore,
    std::vector<VkCommandBuffer>& commandBuffers,
    VkQueue& graphicsQueue,
    VkQueue& presentQueue)
{
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkAcquireNextImageKHR)
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkQueueSubmit)
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkQueuePresentKHR)
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkDeviceWaitIdle)
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkQueueWaitIdle)
#ifdef USE_GLFW
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        vkQueueWaitIdle(presentQueue);
        uint32_t imageIndex;
        vkAcquireNextImageKHR(logical_device, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        vkCheckResult(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional
        vkCheckResult(vkQueuePresentKHR(presentQueue, &presentInfo));
    }
    vkDeviceWaitIdle(logical_device);
	/*GLFW Window main termination*/
    glfwDestroyWindow(window);
    glfwTerminate();
#elif USE_XCB
	while ((e = xcb_wait_for_event (c))) {
		switch (e->response_type & ~0x80) {
			case XCB_EXPOSE:
				xcb_flush (c);
				break;
			case XCB_KEY_PRESS:
				return;
		}
    free (e);
  }
#elif defined(USE_XLIB)
   while (1) {
      XNextEvent(d, &e);
      if (e.type == KeyPress)
         break;
   }
   XCloseDisplay(d);
#endif
}

VkShaderModule create_vertex_module(PFN_vkCreateShaderModule vkCreateShaderModule, VkDevice& logical_device, const std::vector<char>& shader)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shader.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shader.data());
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    VkShaderModule shaderModule;
    vkCheckResult(vkCreateShaderModule(logical_device, &createInfo, nullptr, &shaderModule));
    return shaderModule;
}

std::vector<char> load_shader(const std::string& filename)
{
    FILE* shader_file = fopen(filename.c_str(), "rb");
    if (shader_file) {
        struct stat info;
        stat(filename.c_str(), &info);
        std::vector<char> shader_data(info.st_size);
        fread(shader_data.data(), info.st_size, 1, shader_file);
        fclose(shader_file);
        printf("\tShader (%s) has been loaded (data-size: %d)\n", filename.c_str(), info.st_size);
        return shader_data;
    } else {
        printf("\tCouldn't open shader file: %s\n", filename.c_str());
        throw std::runtime_error("Couldn't open file");
    }
}

void available_layers_and_extensions()
{
    printf("---Checking Vulkan-driver Layers and Extensions\n");
	VK_LOAD_INSTANCE_FUNCTION(nullptr , vkEnumerateInstanceLayerProperties)
	VK_LOAD_INSTANCE_FUNCTION(nullptr , vkEnumerateInstanceExtensionProperties)

    uint32_t instanceLayerCount;
    vkCheckResult(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
    VkLayerProperties* layerProperty = new VkLayerProperties[instanceLayerCount];
    vkCheckResult(vkEnumerateInstanceLayerProperties(&instanceLayerCount, layerProperty));
    printf("Number of instance layers: %d\n", instanceLayerCount);
    for (uint32_t i = 0; i < instanceLayerCount; ++i) {
		printf("\tDetected layer: %s\n", layerProperty[i].layerName);
        uint32_t layersExtensionCount;
        vkCheckResult(vkEnumerateInstanceExtensionProperties(layerProperty[i].layerName, &layersExtensionCount, nullptr));
        VkExtensionProperties *layerExtensions = new VkExtensionProperties[layersExtensionCount];
        vkCheckResult(vkEnumerateInstanceExtensionProperties(layerProperty[i].layerName,
                &layersExtensionCount, layerExtensions));
        for (uint32_t j = 0; j < layersExtensionCount; ++j) {
            printf("\t\tExtensions: %s\n", layerExtensions[j].extensionName);
        }
    }

    uint32_t instanceExtensionsCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionsCount, nullptr);
	printf("Number of instance extensions: %d\n", instanceExtensionsCount);
    VkExtensionProperties *instanceExtensions = new VkExtensionProperties[instanceExtensionsCount];
    vkCheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionsCount, instanceExtensions));
    for (uint32_t i = 0; i < instanceExtensionsCount; ++i) {
        printf("\tDetected instance extension: %s\n", instanceExtensions[i].extensionName);
    }
    printf("\n");
}

VkInstance init_vulkan_instance(const std::vector<const char *> enabledLayerNames)
{
    printf("---Creating vulkan instance\n");
    VkInstance instance;
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.pNext = nullptr;
    instanceInfo.flags = 0;
//#ifdef USE_GLFW
/*Bug or featrue in glfw: glfwGetRequiredInstanceExtensions doesn't work correctly*/
    // unsigned int glfwExtensionCount = 0;
    // const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    // for (size_t i = 0; i < glfwExtensionCount; ++i) {
    //     printf("\tExtensions neccessary for glfw: %s\n", glfwExtensions[i]);
    // }
    // instanceInfo.enabledExtensionCount = glfwExtensionCount;
    // instanceInfo.ppEnabledExtensionNames = glfwExtensions;
#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
    instanceInfo.enabledExtensionCount = 2;
    const char * const enabledExtensionNames[] =  {
        VK_KHR_SURFACE_EXTENSION_NAME,
	#if defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME
	#elif defined(VK_USE_PLATFORM_XCB_KHR)
		VK_KHR_XCB_SURFACE_EXTENSION_NAME
	#endif
	};
	instanceInfo.ppEnabledExtensionNames = enabledExtensionNames;
#endif

    instanceInfo.enabledLayerCount = enabledLayerNames.size();
    instanceInfo.ppEnabledLayerNames = enabledLayerNames.data();

	VK_LOAD_INSTANCE_FUNCTION(nullptr , vkCreateInstance)
    vkCheckResult(vkCreateInstance(&instanceInfo, nullptr, &instance));
    printf("\n");

    return instance;
}

VkPhysicalDevice find_phisical_device(VkInstance& instance)
{
    printf("---Checking phisical devices properties\n");
	VK_LOAD_INSTANCE_FUNCTION(instance , vkEnumeratePhysicalDevices)
	VK_LOAD_INSTANCE_FUNCTION(instance , vkGetPhysicalDeviceProperties)
	VK_LOAD_INSTANCE_FUNCTION(instance , vkGetPhysicalDeviceMemoryProperties)
	VK_LOAD_INSTANCE_FUNCTION(instance , vkGetPhysicalDeviceFeatures)
	VK_LOAD_INSTANCE_FUNCTION(instance , vkGetPhysicalDeviceQueueFamilyProperties)
	VK_LOAD_INSTANCE_FUNCTION(instance , vkEnumerateDeviceExtensionProperties)
    VkPhysicalDevice gpu; 				   // Physical device
    uint32_t gpuCount; 					   // Pysical device count
    std::vector<VkPhysicalDevice> gpuList; // List of physical devices
    // Get number of GPU count
    vkCheckResult(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
    if (gpuCount == 0) {
        printf("\tSorry, could not detect any GPU on you system\n");
        throw VulkanException("No GPU detected");
    }
    printf("\tNum of GPU units: %d\n", gpuCount);
    gpuList.resize(gpuCount);
    // Get GPU information
    vkCheckResult(vkEnumeratePhysicalDevices(instance, &gpuCount, gpuList.data()));
    unsigned int rate = 0;
    uint32_t count = 0, selected = 0;

    for (auto &gpuDevice : gpuList) {
        unsigned int d_rate = 0;
		VkPhysicalDeviceProperties pProperties;
		VkPhysicalDeviceMemoryProperties memoryProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceMemoryProperties(gpuDevice, &memoryProperties);
		vkGetPhysicalDeviceProperties(gpuDevice, &pProperties);
        vkGetPhysicalDeviceFeatures(gpuDevice, &deviceFeatures);
        printf("\tPhisical GPU name: %s\n", pProperties.deviceName);
        if (pProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            d_rate += 1000;
        }
        d_rate += pProperties.limits.maxImageDimension2D;
        if (!deviceFeatures.geometryShader) {
            d_rate = 0;
        }

		if (d_rate > rate) {
            rate = d_rate;
            selected = count;
        }
        ++count;
	}
    printf("\tSelected device index: %d\n", selected);
    return gpuList[selected];
}

const std::vector<uint32_t> find_queue_families(VkInstance& instance, VkPhysicalDevice& gpuDevice, VkSurfaceKHR& surface)
{
    VK_LOAD_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceSurfaceSupportKHR)
    uint32_t queueFamilyCount = 0;
    std::vector<VkQueueFamilyProperties> queueFamilies;
    vkGetPhysicalDeviceQueueFamilyProperties(gpuDevice, &queueFamilyCount, nullptr);
    queueFamilies.resize(queueFamilyCount);
    std::vector<uint32_t> graphicalQueueFamilies;
    std::vector<uint32_t> supportPresentationQueueFamilies;
    vkGetPhysicalDeviceQueueFamilyProperties(gpuDevice, &queueFamilyCount, queueFamilies.data());
    uint32_t count = 0;
    printf("\tDevice have %d queue families\n", queueFamilyCount);
    for (const auto queueFamily : queueFamilies) {
        VkBool32 presentSupport = false;
        vkCheckResult(vkGetPhysicalDeviceSurfaceSupportKHR(gpuDevice, count, surface, &presentSupport));
        if (presentSupport) {
            printf("\tFound queue family with presentation support with index: %d\n", count);
            supportPresentationQueueFamilies.push_back(count);
        }
        if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphicalQueueFamilies.push_back(count);
            printf("\tFound graphical queue family with index: %d\n", count);
        }
        ++count;
    }

    if (!supportPresentationQueueFamilies.size()) {
        printf("\tCouldn't find QueueFamily with presentation support");
        throw VulkanException("No presentation family\n");
    }

    if (!graphicalQueueFamilies.size()) {
        printf("\tCouldn't find graphical QueueFamily");
        throw VulkanException("No graphical family\n");
    }

    for (const auto& prQF : supportPresentationQueueFamilies) {
        if (std::find(graphicalQueueFamilies.begin(), graphicalQueueFamilies.end(), prQF) != graphicalQueueFamilies.end()) {
            return std::vector<uint32_t>{prQF};
        }
    }
    return std::vector<uint32_t>{graphicalQueueFamilies[0], supportPresentationQueueFamilies[0]};
}

VkDevice create_logical_device(VkInstance& instance, 
    VkPhysicalDevice& gpuDevice, 
    const std::vector<uint32_t>& neccessary_queues,
    const std::vector<const char*> enabled_layers)
{
	printf("---Creating logical device\n");

	VK_LOAD_INSTANCE_FUNCTION(instance, vkCreateDevice)
	VK_LOAD_INSTANCE_FUNCTION(instance, vkGetDeviceProcAddr)

    // Available extensions and layers names
	uint32_t deviceExtensionCount = 0;
	std::vector<VkExtensionProperties> deviceExtensionProps;
	std::vector<const char *> deviceExtensionNames;
    const char* const* _ppExtensionNames = nullptr;
	vkEnumerateDeviceExtensionProperties(gpuDevice, nullptr, &deviceExtensionCount, nullptr);
    deviceExtensionProps.resize(deviceExtensionCount);
    vkEnumerateDeviceExtensionProperties(gpuDevice, nullptr, &deviceExtensionCount, deviceExtensionProps.data());
    for (uint32_t i = 0; i < deviceExtensionCount; ++i) {
        deviceExtensionNames.push_back(deviceExtensionProps[i].extensionName);
        printf("\tDetected device extension:%s\n",deviceExtensionProps[i].extensionName);
    }

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> VkDeviceQueueCreateInfos;
    for (const auto& queue : neccessary_queues) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.flags = 0;
        queueCreateInfo.queueFamilyIndex = queue;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        VkDeviceQueueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = nullptr;
    deviceInfo.flags = 0;
    deviceInfo.enabledLayerCount = enabled_layers.size();
    deviceInfo.ppEnabledLayerNames = enabled_layers.data();
    deviceInfo.enabledExtensionCount = deviceExtensionCount;
    deviceInfo.ppEnabledExtensionNames = deviceExtensionNames.data();
    deviceInfo.queueCreateInfoCount = VkDeviceQueueCreateInfos.size();
    deviceInfo.pQueueCreateInfos = VkDeviceQueueCreateInfos.data();

    VkDevice logical_device;
    vkCheckResult(vkCreateDevice(gpuDevice, &deviceInfo, nullptr, &logical_device));
    return logical_device;
}

VkSurfaceKHR create_swapchain_surface(VkInstance& instance)
{
	printf("---Creating window surface\n");
#ifdef VK_USE_PLATFORM_XLIB_KHR
    VkXlibSurfaceCreateInfoKHR
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	VkXcbSurfaceCreateInfoKHR
#endif 
    surfaceCreateInfo = {};

#ifdef USE_GLFW
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = glfwGetX11Window(window);
    surfaceCreateInfo.dpy = glfwGetX11Display();
#elif defined(USE_XCB)
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = window;
    surfaceCreateInfo.connection = c;
#elif defined(USE_XLIB)
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.window = window;
    surfaceCreateInfo.dpy = display;
#endif
    surfaceCreateInfo.pNext = NULL;
    surfaceCreateInfo.flags = 0;

    VkSurfaceKHR surface;
#if defined(USE_XLIB) || defined(USE_GLFW)
	VK_LOAD_INSTANCE_FUNCTION(instance , vkCreateXlibSurfaceKHR)
	vkCheckResult(vkCreateXlibSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface));
#elif defined(USE_XCB)
	VK_LOAD_INSTANCE_FUNCTION(instance , vkCreateXcbSurfaceKHR)
	vkCheckResult(vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface));
#endif
    return surface;
}

VkExtent2D getSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    VkExtent2D swapChainExtent;
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        swapChainExtent = capabilities.currentExtent; 
        printf("\tCurrent extent: width - %d; height - %d\n", capabilities.currentExtent.width, capabilities.currentExtent.height);
    } else {
        int width, height;
#ifdef USE_GLFW
        glfwGetWindowSize(window, &width, &height);
#endif

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
        swapChainExtent = {actualExtent.width, actualExtent.height};
    }
    return swapChainExtent;
}

uint32_t getImageCount(const VkSurfaceCapabilitiesKHR& capabilities)
{
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    printf("\tImage count of swapchain: %d\n", imageCount);
}

struct VkSwapchain
{
    VkSwapchainKHR swapchain;
    uint32_t imageCount;
    VkExtent2D extent;
    VkSurfaceFormatKHR format;
};

VkSwapchain create_swapchain(VkInstance& instance, 
VkPhysicalDevice& gpuDevice,
VkDevice& logical_device, 
VkSurfaceKHR& surface)
{
    printf("---Creating swapchain\n");
    VK_LOAD_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceSurfaceFormatsKHR)
    VK_LOAD_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceSurfacePresentModesKHR)
    VK_LOAD_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkCreateSwapchainKHR)

    uint32_t formatCount = 0;	
	vkGetPhysicalDeviceSurfaceFormatsKHR(gpuDevice, surface, &formatCount, nullptr);	
	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    if (formatCount) printf("\tAvailable surface formats:%d\n", formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpuDevice, surface, &formatCount, surfaceFormats.data());
    VkSurfaceFormatKHR surfaceFormat;
    for (const auto& format : surfaceFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            printf("\t\tFound exactly good format\n");
            surfaceFormat = format;
            break;
        }
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpuDevice, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> surfaceModes(modeCount);
    if (modeCount) printf("\tAvailable surface modes:%d\n", modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpuDevice, surface, &modeCount, surfaceModes.data());
    VkPresentModeKHR presentMode;
    for (const auto& mode : surfaceModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            printf("\t\tFound exactly good mode\n");
            presentMode = mode;
        }
    }

    VkSurfaceCapabilitiesKHR capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpuDevice, surface, &capabilities);
    auto swapChainExtent = getSwapchainExtent(capabilities);
    auto imageCount = getImageCount(capabilities);

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0; // Optional
    createInfo.pQueueFamilyIndices = nullptr; // Optional
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    VkSwapchainKHR swapChain;
    vkCheckResult(vkCreateSwapchainKHR(logical_device, &createInfo, nullptr, &swapChain));

    VkSwapchain swapchain;
    swapchain.swapchain = swapChain;
    swapchain.imageCount = imageCount;
    swapchain.extent = swapChainExtent;
    swapchain.format = surfaceFormat;
    return swapchain;
}

std::vector<VkImageView> create_image_views(VkDevice& logical_device, VkSwapchain& swapChain)
{
    printf("---Create image views\n");
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkGetSwapchainImagesKHR)
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkCreateImageView)

    uint32_t imageCount;
    std::vector<VkImage> swapChainImages;
    vkGetSwapchainImagesKHR(logical_device, swapChain.swapchain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(logical_device, swapChain.swapchain, &imageCount, swapChainImages.data());
    
    std::vector<VkImageView> swapChainImageViews(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChain.format.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        vkCheckResult(vkCreateImageView(logical_device, &createInfo, nullptr, &swapChainImageViews[i]));
    }

    return swapChainImageViews;
}

VkRenderPass create_render_pass(VkDevice& logical_device, VkSwapchain swapchain)
{
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkCreateRenderPass)

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPass renderPass;
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchain.format.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    vkCheckResult(vkCreateRenderPass(logical_device, &renderPassInfo, nullptr, &renderPass));
    return renderPass;
}

VkPipelineLayout create_pipeline_layout(VkDevice& logical_device)
{
    printf("---Creating pipeline layout\n");
    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = 0; // Optional
    VK_LOAD_DEVICE_FUNCTION(logical_device, vkCreatePipelineLayout)
    vkCheckResult(vkCreatePipelineLayout(logical_device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
    return pipelineLayout;
}

VkPipeline create_pipeline(VkDevice& logical_device, 
VkPipelineShaderStageCreateInfo shaderStages[], 
VkExtent2D& swapchainExtent,
VkRenderPass& renderPass,
VkPipelineLayout& pipelineLayout
)
{
    printf("---Creating pipeline\n");
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapchainExtent.width;
    viewport.height = (float) swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    VK_LOAD_DEVICE_FUNCTION(logical_device, vkCreateGraphicsPipelines)
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optional
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional
    VkPipeline graphicsPipeline;
    vkCheckResult(vkCreateGraphicsPipelines(logical_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));
    printf("---Pipeline created\n");
    return graphicsPipeline;
}

int main()
{
	printf("\t\t######START######\n");
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VULKAN_LIBRARY = LoadLibrary( "vulkan-1.dll" );
#elif defined(VK_USE_PLATFORM_XCB_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR)
	VULKAN_LIBRARY = dlopen( "libvulkan.so", RTLD_NOW );
#endif
    if( VULKAN_LIBRARY == nullptr ) {
        printf("Couldn't load Vulkan Library\n");
        return 0;
    }

    create_window();
    VK_EXPORTED_FUNCTION( vkGetInstanceProcAddr )
    available_layers_and_extensions();
#define ENABLED_DEBUG
    #ifdef ENABLED_DEBUG
        const std::vector<const char*> enabledLayerNames =  {
            "VK_LAYER_LUNARG_standard_validation",
            "VK_LAYER_LUNARG_object_tracker" };
    #else
        const std::vector<const char*> enabledLayerNames = {};
    #endif

    auto instance = init_vulkan_instance(enabledLayerNames);
    auto gpu = find_phisical_device(instance);
    auto swapchain_surface = create_swapchain_surface(instance);
    auto queueFamilies = find_queue_families(instance, gpu, swapchain_surface);
    auto device = create_logical_device(instance, gpu, queueFamilies, enabledLayerNames);
    auto swapchain = create_swapchain(instance, gpu, device, swapchain_surface);
    auto graphicsQueueFamilyIndex = queueFamilies[0];

    VK_LOAD_INSTANCE_FUNCTION(instance, vkGetDeviceQueue)
    VkQueue graphicsQueue;
    VkQueue presentQueue; 
    if (queueFamilies.size() > 1) {
        vkGetDeviceQueue(device, queueFamilies[0], 0, &graphicsQueue);
        vkGetDeviceQueue(device, queueFamilies[1], 0, &presentQueue);
    } else {
        vkGetDeviceQueue(device, queueFamilies[0], 0, &graphicsQueue);
        vkGetDeviceQueue(device, queueFamilies[0], 0, &presentQueue);
    }
   
    printf("---Loading shaders\n");
    auto vertexShader = load_shader("vert.spv");
    auto fragmentShader = load_shader("frag.spv");
    VK_LOAD_DEVICE_FUNCTION(device , vkCreateShaderModule)
    VkShaderModule vertModule = create_vertex_module(vkCreateShaderModule, device, vertexShader);
    VkShaderModule fragModule = create_vertex_module(vkCreateShaderModule, device, fragmentShader);

    printf("---Creating shader stage\n");
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo = nullptr;
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr;
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto renderPass = create_render_pass(device, swapchain);
    auto pipelineLayout = create_pipeline_layout(device);
    auto graphicalPipeline = create_pipeline(device, shaderStages, swapchain.extent, renderPass, pipelineLayout);
    auto swapChainImageViews = create_image_views(device, swapchain);
    printf("---Creating framebuffer\n");
    VK_LOAD_DEVICE_FUNCTION(device, vkCreateFramebuffer)
    std::vector<VkFramebuffer> swapChainFramebuffers(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain.extent.width;
        framebufferInfo.height = swapchain.extent.height;
        framebufferInfo.layers = 1;

        vkCheckResult(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]));
    }
    printf("---Creating command pool\n");
    VK_LOAD_DEVICE_FUNCTION(device, vkCreateCommandPool)
    VK_LOAD_DEVICE_FUNCTION(device, vkAllocateCommandBuffers)
    VK_LOAD_DEVICE_FUNCTION(device, vkBeginCommandBuffer)
    VkCommandPool commandPool;
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    poolInfo.flags = 0; // Optional
    vkCheckResult(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));

    std::vector<VkCommandBuffer> commandBuffers(swapChainFramebuffers.size());
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
    vkCheckResult(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()));

    VK_LOAD_DEVICE_FUNCTION(device, vkCmdBeginRenderPass)
    VK_LOAD_DEVICE_FUNCTION(device, vkCmdBindPipeline)
    VK_LOAD_DEVICE_FUNCTION(device, vkCmdDraw)
    VK_LOAD_DEVICE_FUNCTION(device, vkCmdEndRenderPass)
    VK_LOAD_DEVICE_FUNCTION(device, vkEndCommandBuffer)
    for (size_t i = 0; i < commandBuffers.size(); ++i) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        beginInfo.pInheritanceInfo = nullptr; // Optional

        vkCheckResult(vkBeginCommandBuffer(commandBuffers[i], &beginInfo));
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[i];
        renderPassInfo.renderArea.offset =  {0, 0};
        renderPassInfo.renderArea.extent = swapchain.extent;
        VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicalPipeline);
        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffers[i]);
        vkCheckResult(vkEndCommandBuffer(commandBuffers[i]));
    }

    VK_LOAD_DEVICE_FUNCTION(device, vkCreateSemaphore)
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    vkCheckResult(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore));
    vkCheckResult(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore));

    printf("---Starting main window-loop\n");
    window_main_loop(device, swapchain.swapchain,
    imageAvailableSemaphore, renderFinishedSemaphore,
    commandBuffers, graphicsQueue, presentQueue);
	printf("---Unloading vulkan application\n");

    VK_LOAD_DEVICE_FUNCTION(device, vkDestroySemaphore)
    vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

    VK_LOAD_DEVICE_FUNCTION(device, vkDestroyFramebuffer)
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
    }

	VK_LOAD_DEVICE_FUNCTION(device, vkDestroyCommandPool)
    vkDestroyCommandPool(device, commandPool, nullptr);
	VK_LOAD_DEVICE_FUNCTION(device, vkDestroyPipeline)
    vkDestroyPipeline(device, graphicalPipeline, nullptr);
    VK_LOAD_DEVICE_FUNCTION(device, vkDestroyPipelineLayout)
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    VK_LOAD_DEVICE_FUNCTION(device, vkDestroyRenderPass)
    vkDestroyRenderPass(device, renderPass, nullptr);
    VK_LOAD_DEVICE_FUNCTION(device, vkDestroyShaderModule)
    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, vertModule, nullptr);

    VK_LOAD_DEVICE_FUNCTION(device, vkDestroyImageView)
    for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
        if ( swapChainImageViews[i] != VK_NULL_HANDLE )
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }

    if ( swapchain.swapchain != VK_NULL_HANDLE) {
        VK_LOAD_INSTANCE_FUNCTION(instance , vkDestroySwapchainKHR);
        vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
    }

    if( device != VK_NULL_HANDLE ) {
	  VK_LOAD_DEVICE_FUNCTION(device , vkDeviceWaitIdle)
	  VK_LOAD_DEVICE_FUNCTION(device , vkDestroyDevice)
      vkDeviceWaitIdle( device );
      vkDestroyDevice( device, nullptr );
    }

    if ( swapchain_surface != VK_NULL_HANDLE ) {
        VK_LOAD_INSTANCE_FUNCTION(instance , vkDestroySurfaceKHR);
        vkDestroySurfaceKHR(instance, swapchain_surface, nullptr);
    }

    if( instance != VK_NULL_HANDLE ) {
	  VK_LOAD_INSTANCE_FUNCTION(instance , vkDestroyInstance);
      vkDestroyInstance( instance, nullptr );
    }

	if( VULKAN_LIBRARY ) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
      FreeLibrary( VULKAN_LIBRARY );
#elif defined(VK_USE_PLATFORM_XCB_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR)
      dlclose( VULKAN_LIBRARY );
#endif
    }
	printf("\t\t######FINISH######\n");
    return 0;
}