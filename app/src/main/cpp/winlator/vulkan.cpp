#include <vulkan/vulkan.h>
#include <iostream>
#include <map>
#include <vector>

#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>

static void *vulkan_handle = NULL;
static PFN_vkGetInstanceProcAddr gip = NULL;

__attribute__((constructor))
void start() {
    if (!vulkan_handle) {
        vulkan_handle = dlopen("/system/lib64/libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        gip = (PFN_vkGetInstanceProcAddr)dlsym(vulkan_handle, "vkGetInstanceProcAddr");
    }
}

VkInstance create_instance() {
    VkResult result;
    VkInstance instance;
    VkInstanceCreateInfo create_info = {};

    PFN_vkCreateInstance createInstance = (PFN_vkCreateInstance)dlsym(vulkan_handle, "vkCreateInstance");

    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.pApplicationInfo = NULL;
    create_info.enabledLayerCount = 0;
    create_info.enabledExtensionCount = 0;

    result = createInstance(&create_info, NULL, &instance);

    if (result != VK_SUCCESS)
        __android_log_print(ANDROID_LOG_DEBUG, "GPUInformation", "Failed to create instance: %d", result);

    return instance;

}

std::vector<VkPhysicalDevice> get_physical_devices(VkInstance instance) {
    VkResult result = VK_ERROR_UNKNOWN;
    std::vector<VkPhysicalDevice> physical_devices;
    uint32_t deviceCount;

    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)gip(instance, "vkEnumeratePhysicalDevices");

    enumeratePhysicalDevices(instance, &deviceCount, NULL);
    physical_devices.resize(deviceCount);

    if (deviceCount > 0)
        result = enumeratePhysicalDevices(instance, &deviceCount, physical_devices.data());

    if (result != VK_SUCCESS)
        __android_log_print(ANDROID_LOG_DEBUG, "GPUInformation", "Failed to enumerate devices: %d", result);

    return physical_devices;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_winlator_core_GPUInformation_getDriverVersion(JNIEnv *env, jclass obj) {
    VkPhysicalDeviceProperties props = {};
    char *driverVersion;
    VkInstance instance;

    instance = create_instance();
    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)gip(instance, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroyInstance = (PFN_vkDestroyInstance)gip(instance, "vkDestroyInstance");

    for (const auto &pdevice: get_physical_devices(instance)) {
        getPhysicalDeviceProperties(pdevice, &props);
        uint32_t vk_driver_major = VK_VERSION_MAJOR(props.driverVersion);
        uint32_t vk_driver_minor = VK_VERSION_MINOR(props.driverVersion);
        uint32_t vk_driver_patch = VK_VERSION_PATCH(props.driverVersion);
        asprintf(&driverVersion, "%d.%d.%d", vk_driver_major, vk_driver_minor,
                 vk_driver_patch);
    }

    destroyInstance(instance, NULL);

    return (env->NewStringUTF(driverVersion));
}