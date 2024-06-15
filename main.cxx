#include "vulkan/vulkan_core.h"
#include <iostream>
#include <assert.h>
#include <vector>

#undef UNICODE
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

// Windows
WNDCLASS wc = { };
const char *wnd_class_name = "gfxwndclass";
HWND hwnd = nullptr;
bool window_closed = false;

// Vk
VkInstance vk_instance = nullptr;
VkDevice vk_device = nullptr;
VkCommandPool vk_cmd_pool = nullptr;
VkCommandBuffer vk_cmd_buffer = nullptr;

LRESULT APIENTRY
window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_CLOSE:
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:
    std::cerr << "Window destroyed\n";
    window_closed = true;
    PostQuitMessage(0);
    return 0;
  case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
      EndPaint(hwnd, &ps);
    }
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void
make_window_class() {
  wc.lpfnWndProc = window_proc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = wnd_class_name;
  RegisterClass(&wc);
}

void
make_window() {
  hwnd = CreateWindowEx(0, wnd_class_name, "Window", WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  assert(hwnd != nullptr);
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);
}

void
update_window() {
  MSG msg;
  GetMessage(&msg, nullptr, 0, 0);
  TranslateMessage(&msg);
  DispatchMessage(&msg);
}

std::string
vk_physical_device_type_to_string(VkPhysicalDeviceType type) {
  switch (type) {
  case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    return "Other";
  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
    return "Integrated GPU";
  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
    return "Discrete GPU";
  case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
    return "Virtual GPU";
  case VK_PHYSICAL_DEVICE_TYPE_CPU:
    return "GPU";
  default:
    return "Unknown";
  }
}

bool
init_vk() {
  VkApplicationInfo app_info = {
    VK_STRUCTURE_TYPE_APPLICATION_INFO,
    nullptr,
    "GFX",
    1,
    "GFX",
    1,
    VK_API_VERSION_1_0
  };
  const char *extension_names[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
  uint32_t extension_count = 2;
  VkInstanceCreateInfo create_info = {
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    nullptr,
    0,
    &app_info,
    0, nullptr,
    extension_count, extension_names
  };
  VkResult result = vkCreateInstance(&create_info, nullptr, &vk_instance);
  if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
    std::cerr << "cannot find a compatible Vulkan ICD\n";
    return false;
  } else if (result) {
    std::cerr << "Could not initialize vulkan\n";
    return false;
  } else {
    std::cerr << "Vulkan initialized\n";
  }

  // Enumerate devices.

  uint32_t device_count = 0;
  std::vector<VkPhysicalDevice> devices;
  result = vkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr);
  assert(!result);
  devices.resize(device_count);
  result = vkEnumeratePhysicalDevices(vk_instance, &device_count, devices.data());
  assert(!result);

  std::vector<VkPhysicalDeviceProperties> device_prop_list;
  device_prop_list.resize(device_count);
  for (uint32_t i = 0; i < device_count; ++i) {
    vkGetPhysicalDeviceProperties(devices[i], &device_prop_list[i]);
  }

  // Pick in the order of: discrete, integrated, virtual.

  int chosen_device = -1;

  std::cerr << device_count << " graphics devices\n";
  for (uint32_t i = 0; i < device_count; ++i) {
    std::cerr << "Device " << i << "\n";
    VkPhysicalDeviceProperties &device_props = device_prop_list[i];
    std::cerr << "Name: " << std::string(device_props.deviceName) << "\n";
    std::cerr << "Type: " << vk_physical_device_type_to_string(device_props.deviceType) << "\n";
    std::cerr << "Vendor ID: " << device_props.vendorID << "\n";
    std::cerr << "Device ID: " << device_props.deviceID << "\n";
    std::cerr << "Driver version: " << device_props.driverVersion << "\n";
    std::cerr << "VK API version: "
              << "variant " << VK_API_VERSION_VARIANT(device_props.apiVersion) << " "
              << "ver " << VK_API_VERSION_MAJOR(device_props.apiVersion) << "."
              << VK_API_VERSION_MINOR(device_props.apiVersion) << "."
              << VK_API_VERSION_PATCH(device_props.apiVersion) << "\n";
  }

  // First look for discrete.
  for (int i = 0; i < (int)device_prop_list.size(); ++i) {
    if (device_prop_list[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      chosen_device = i;
      break;
    }
  }
  if (chosen_device == -1) {
    // Don't have a discrete, look for integrated.
    for (int i = 0; i < (int)device_prop_list.size(); ++i) {
      if (device_prop_list[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        chosen_device = i;
        break;
      }
    }
  }
  if (chosen_device == -1) {
    // Didn't get a discrete or integrated chip.  Fail.
    std::cerr << "No discrete or integrated graphics device available!\n";
    return false;
  }

  std::cerr << "Using graphics device " << chosen_device << " (" << std::string(device_prop_list[chosen_device].deviceName) << ")\n";

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(devices[chosen_device], &queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> queue_props;
  queue_props.resize(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(devices[chosen_device], &queue_family_count, queue_props.data());

  float queue_prior[1] = { 0.0f };
  VkDeviceQueueCreateInfo queue_info = { };
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.pNext = nullptr;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = queue_prior;
  for (uint32_t i = 0; i < queue_family_count; ++i) {
    if (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      // Grab the graphics queue.
      queue_info.queueFamilyIndex = i;
    }
  }

  const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
  uint32_t device_extension_count = 1;
  VkDeviceCreateInfo device_info = { };
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = nullptr;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledExtensionCount = device_extension_count;
  device_info.ppEnabledExtensionNames = device_extensions;
  device_info.enabledLayerCount = 0;
  device_info.ppEnabledLayerNames = nullptr;
  device_info.pEnabledFeatures = nullptr;

  result = vkCreateDevice(devices[chosen_device], &device_info, nullptr, &vk_device);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create vulkan device\n";
    return false;
  } else {
    std::cerr << "Created vulkan device\n";
  }

  VkCommandPoolCreateInfo pool_info = { };
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.pNext = nullptr;
  pool_info.queueFamilyIndex = queue_info.queueFamilyIndex;
  pool_info.flags = 0;

  result = vkCreateCommandPool(vk_device, &pool_info, nullptr, &vk_cmd_pool);
  if (result != VK_SUCCESS) {
    std::cerr << "Couldn't create vk command pool\n";
    return false;
  }

  VkCommandBufferAllocateInfo cmd = { };
  cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd.pNext = nullptr;
  cmd.commandPool = vk_cmd_pool;
  cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd.commandBufferCount = 1;

  result = vkAllocateCommandBuffers(vk_device, &cmd, &vk_cmd_buffer);
  if (result != VK_SUCCESS) {
    std::cerr << "Couldn't create vk command buffer\n";
  }

  VkBool32 *supports_present = (VkBool32 *)alloca(queue_family_count * sizeof(VkBool32));

  for (uint32_t i = 0; i < queue_family_count; ++i) {
    vkGetPhysicalDeviceSurfaceSupportKHR(devices[chosen_device], i,
  }


  return true;
}

void close_vk() {
  vkDestroyInstance(vk_instance, nullptr);
  vk_instance = nullptr;

  std::cerr << "Vulkan shut down\n";
}

int
main(int argc, char *argv[]) {
  make_window_class();
  make_window();

  if (!init_vk()) {
    return 1;
  }

  while (!window_closed) {
    update_window();
  }

  close_vk();

  return 0;
}
