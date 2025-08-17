#include <vulkan/vulkan.h>
#include <iostream>
#include <map>
#include <vector>

#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../adrenotools/include/adrenotools/driver.h"

#define LOG_TAG "System.out"
#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

PFN_vkGetInstanceProcAddr gip;

const char *get_native_library_dir(JNIEnv *env, jobject context) {
    char *native_libdir;

    if (context != nullptr) {
        jclass class_ = env->FindClass("com/winlator/cmod/core/AppUtils");
        jmethodID getNativeLibraryDir = env->GetStaticMethodID(class_, "getNativeLibDir",
                                                               "(Landroid/content/Context;)Ljava/lang/String;");
        jstring nativeLibDir = static_cast<jstring>(env->CallStaticObjectMethod(class_,
                                                                                getNativeLibraryDir,
                                                                                context));
        if (nativeLibDir)
            native_libdir = (char *)env->GetStringUTFChars(nativeLibDir, nullptr);
    }
    return native_libdir;
}

const char *get_driver_path(JNIEnv *env, jobject context, const char *driver_name) {
    char *driver_path;
    char *absolute_path;

    jclass contextWrapperClass = env->FindClass("android/content/ContextWrapper");
    jmethodID  getFilesDir = env->GetMethodID(contextWrapperClass, "getFilesDir", "()Ljava/io/File;");
    jobject  filesDirObj = env->CallObjectMethod(context, getFilesDir);
    jclass fileClass = env->GetObjectClass(filesDirObj);
    jmethodID getAbsolutePath = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jstring absolutePath = static_cast<jstring>(env->CallObjectMethod(filesDirObj,
                                                                      getAbsolutePath));

    if (absolutePath) {
        absolute_path = (char *)env->GetStringUTFChars(absolutePath, nullptr);
        asprintf(&driver_path, "%s/contents/adrenotools/%s/", absolute_path, driver_name);
        env->ReleaseStringUTFChars(absolutePath, absolute_path);
    }

    return driver_path;
}

const char *get_library_name(JNIEnv *env, jobject context, const char *driver_name) {
    char *library_name;

    jclass adrenotoolsManager = env->FindClass("com/winlator/cmod/contents/AdrenotoolsManager");
    jmethodID constructor = env->GetMethodID(adrenotoolsManager, "<init>", "(Landroid/content/Context;)V");
    jobject  adrenotoolsManagerObj = env->NewObject(adrenotoolsManager, constructor, context);
    jmethodID getLibraryName = env->GetMethodID(adrenotoolsManager, "getLibraryName","(Ljava/lang/String;)Ljava/lang/String;");

    jstring driverName = env->NewStringUTF(driver_name);

    jstring libraryName = static_cast<jstring>(env->CallObjectMethod(adrenotoolsManagerObj,getLibraryName, driverName));

    if (libraryName)
        library_name = (char *)env->GetStringUTFChars(libraryName, nullptr);

    return library_name;
}

void *init_original_vulkan() {
    return dlopen("/system/lib64/libvulkan.so", RTLD_LOCAL | RTLD_NOW);
}

void *init_vulkan(JNIEnv  *env, jobject context, jstring driverName) {
        char *tmpdir;
        void *vulkan_handle;

        const char *driver_name = env->GetStringUTFChars(driverName, nullptr);

        if (!strcmp(driver_name, "System"))
            return init_original_vulkan();

        const char *driver_path = get_driver_path(env, context, driver_name);
        const char *library_name = get_library_name(env, context, driver_name);
        const char *native_library_dir = get_native_library_dir(env, context);

        if (driver_path) {
            asprintf(&tmpdir, "%s%s", driver_path, "temp");
            mkdir(tmpdir, S_IRWXU | S_IRWXG);
        }

        vulkan_handle = adrenotools_open_libvulkan(RTLD_LOCAL | RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, tmpdir, native_library_dir, driver_path, library_name, nullptr, nullptr);

        if (!vulkan_handle)
           return init_original_vulkan();

        return vulkan_handle;
}

VkResult create_instance(jstring driverName, JNIEnv *env, jobject context, VkInstance *instance) {
    VkResult result;
    VkInstanceCreateInfo create_info = {};
    void *vulkan_handle;

    if (driverName)
        vulkan_handle = init_vulkan(env, context, driverName);
    else
        vulkan_handle = init_original_vulkan();

    gip = (PFN_vkGetInstanceProcAddr)dlsym(vulkan_handle, "vkGetInstanceProcAddr");
    PFN_vkCreateInstance createInstance = (PFN_vkCreateInstance)dlsym(vulkan_handle, "vkCreateInstance");

    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.pApplicationInfo = NULL;
    create_info.enabledLayerCount = 0;
    create_info.enabledExtensionCount = 0;

    result = createInstance(&create_info, NULL, instance);

    return result;
}

VkResult get_physical_devices(VkInstance instance, std::vector<VkPhysicalDevice> &physical_devices) {
    VkResult result = VK_ERROR_UNKNOWN;
    uint32_t deviceCount;

    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)gip(instance, "vkEnumeratePhysicalDevices");
    if (!enumeratePhysicalDevices)
        return VK_ERROR_INITIALIZATION_FAILED;

    enumeratePhysicalDevices(instance, &deviceCount, NULL);
    physical_devices.resize(deviceCount);

    if (deviceCount > 0)
        result = enumeratePhysicalDevices(instance, &deviceCount, physical_devices.data());

    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_winlator_cmod_core_GPUInformation_getVersion(JNIEnv *env, jclass obj, jstring driverName, jobject context) {
    VkPhysicalDeviceProperties props = {};
    std::vector<VkPhysicalDevice> pdevices;
    char *driverVersion;
    VkInstance instance;

    if  (create_instance(driverName, env, context, &instance) != VK_SUCCESS) {
        printf("Failed to create instance");
        return env->NewStringUTF("Unknown");
    }

    if (get_physical_devices(instance, pdevices) != VK_SUCCESS) {
        printf("Failed to query physical devices");
        return env->NewStringUTF("Unknown");
    }

    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)gip(instance, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroyInstance = (PFN_vkDestroyInstance)gip(instance, "vkDestroyInstance");

    if (!getPhysicalDeviceProperties || !destroyInstance) {
        printf("Failed to get function pointers");
        return env->NewStringUTF("Unknown");
    }

    for (const auto &pdevice: pdevices) {
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

extern "C" JNIEXPORT jstring JNICALL
Java_com_winlator_cmod_core_GPUInformation_getVulkanVersion(JNIEnv *env, jclass obj, jstring driverName, jobject context) {
    VkPhysicalDeviceProperties props = {};
    std::vector<VkPhysicalDevice> pdevices;
    char *vulkanVersion;
    VkInstance instance;

    if  (create_instance(driverName, env, context, &instance) != VK_SUCCESS) {
        printf("Failed to create instance");
        return env->NewStringUTF("Unknown");
    }

    if (get_physical_devices(instance, pdevices) != VK_SUCCESS) {
        printf("Failed to query physical devices");
        return env->NewStringUTF("Unknown");
    }

    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)gip(instance, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroyInstance = (PFN_vkDestroyInstance)gip(instance, "vkDestroyInstance");

    if (!getPhysicalDeviceProperties || !destroyInstance) {
        printf("Failed to get function pointers");
        return env->NewStringUTF("Unknown");
    }

    for (const auto &pdevice: pdevices) {
        getPhysicalDeviceProperties(pdevice, &props);
        uint32_t vk_driver_major = VK_VERSION_MAJOR(props.apiVersion);
        uint32_t vk_driver_minor = VK_VERSION_MINOR(props.apiVersion);
        uint32_t vk_driver_patch = VK_VERSION_PATCH(props.apiVersion);
        asprintf(&vulkanVersion, "%d.%d.%d", vk_driver_major, vk_driver_minor,
                 vk_driver_patch);
    }

    destroyInstance(instance, NULL);

    return (env->NewStringUTF(vulkanVersion));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_winlator_cmod_core_GPUInformation_getRenderer(JNIEnv *env, jclass obj, jstring driverName, jobject context) {
    VkPhysicalDeviceProperties props = {};
    std::vector<VkPhysicalDevice> pdevices;
    char *renderer;
    VkInstance instance;

    if  (create_instance(driverName, env, context, &instance) != VK_SUCCESS) {
        printf("Failed to create instance");
        return env->NewStringUTF("Unknown");
    }

    if (get_physical_devices(instance, pdevices) != VK_SUCCESS) {
        printf("Failed to query physical devices");
        return env->NewStringUTF("Unknown");
    }

    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)gip(instance, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroyInstance = (PFN_vkDestroyInstance)gip(instance, "vkDestroyInstance");

    if (!getPhysicalDeviceProperties || !destroyInstance) {
        printf("Failed to get function pointers");
        return env->NewStringUTF("Unknown");
    }

    for (const auto &pdevice: pdevices) {
        getPhysicalDeviceProperties(pdevice, &props);
        asprintf(&renderer, "%s", props.deviceName);
    }

    destroyInstance(instance, NULL);

    return (env->NewStringUTF(renderer));
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_winlator_cmod_core_GPUInformation_enumerateExtensions(JNIEnv *env, jclass obj, jstring driverName, jobject context) {
    jobjectArray extensions;
    VkInstance instance;
    std::vector<VkPhysicalDevice> pdevices;
    uint32_t extensionCount;
    std::vector<VkExtensionProperties> extensionProperties;

    if  (create_instance(driverName, env, context, &instance) != VK_SUCCESS) {
        printf("Failed to create instance");
        return env->NewObjectArray(0, env->FindClass("java/lang/String"), env->NewStringUTF(""));
    }

    if (get_physical_devices(instance, pdevices) != VK_SUCCESS) {
        printf("Failed to query physical devices");
        return env->NewObjectArray(0, env->FindClass("java/lang/String"), env->NewStringUTF(""));
    }

    PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)gip(instance, "vkEnumerateDeviceExtensionProperties");
    PFN_vkDestroyInstance destroyInstance = (PFN_vkDestroyInstance)gip(instance, "vkDestroyInstance");

    if (!enumerateDeviceExtensionProperties || !destroyInstance) {
        printf("Failed to get function pointers");
        return env->NewObjectArray(0, env->FindClass("java/lang/String"), env->NewStringUTF(""));
    }


    for (const auto &pdevice : pdevices) {
        enumerateDeviceExtensionProperties(pdevice, NULL, &extensionCount, NULL);
        extensionProperties.resize(extensionCount);
        enumerateDeviceExtensionProperties(pdevice, NULL, &extensionCount, extensionProperties.data());
        extensions = (jobjectArray)env->NewObjectArray(extensionCount, env->FindClass("java/lang/String"), env->NewStringUTF(""));
        int index = 0;
        for (const auto &extensionProperty : extensionProperties) {
            env->SetObjectArrayElement(extensions, index, env->NewStringUTF(extensionProperty.extensionName));
            index++;
        }
    }

    destroyInstance(instance, NULL);

    return extensions;
}