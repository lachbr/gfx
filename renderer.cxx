#include <iostream>

#include "renderer.hxx"
#include "vulkan/vulkan_core.h"

#include "linmath.hxx"

#include <algorithm>
#include <fstream>

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

vector<uint8_t>
read_binary_file(const std::string &filename) {
  std::ifstream stream(filename, std::ios::ate | std::ios::binary);
  if (!stream.good()) {
    return vector<uint8_t>();
  }

  size_t num_bytes = stream.tellg();
  vector<uint8_t> out;
  out.resize(num_bytes);

  stream.seekg(0, std::ios::beg);
  stream.read((char *)out.data(), num_bytes);

  return out;
}

VkShaderModule RendererVk::
make_shader_module(const vector<uint8_t> &code) {
  VkShaderModuleCreateInfo shm_create = { };
  shm_create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shm_create.pNext = nullptr;
  shm_create.flags = 0;
  shm_create.codeSize = code.size();
  shm_create.pCode = (uint32_t *)code.data();
  VkShaderModule mod = nullptr;
  VkResult result = vkCreateShaderModule(_device, &shm_create, nullptr, &mod);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create shader module: " << result << "\n";
    return nullptr;
  }
  return mod;
}

bool
vk_error_check(VkResult ret, const std::string &context) {
  if (ret != VK_SUCCESS) {
    std::cerr << "Vk error for " << context << ", code " << ret << "\n";
    return false;
  }
  return true;
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

VkIndexType get_vk_index_type(MaterialEnums::IndexType type) {
  switch (type) {
  case MaterialEnums::IT_uint32:
    return VK_INDEX_TYPE_UINT32;
  case MaterialEnums::IT_uint16:
    return VK_INDEX_TYPE_UINT16;
  case MaterialEnums::IT_uint8:
    return VK_INDEX_TYPE_UINT8_KHR;
  }
}

// Initialize vulkan.
bool RendererVk::
initialize(WindowHandle hwnd) {
  VkApplicationInfo app_info = {
    VK_STRUCTURE_TYPE_APPLICATION_INFO,
    nullptr,
    "GFX",
    1,
    "GFX",
    1,
    VK_API_VERSION_1_3
  };
#ifdef _WIN32
#define PLAT_SURFACE_EXT_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#else
#pragma error("don't know vk surf extension name for this platform")
#endif
  const char *extension_names[] = { VK_KHR_SURFACE_EXTENSION_NAME, PLAT_SURFACE_EXT_NAME, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };
  uint32_t extension_count = 3;
  VkInstanceCreateInfo create_info = {
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    nullptr,
    0,
    &app_info,
    0, nullptr,
    extension_count, extension_names
  };
  VkResult result = vkCreateInstance(&create_info, nullptr, &_instance);
  if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
    std::cerr << "cannot find a compatible Vulkan ICD\n";
    return false;
  } else if (result) {
    std::cerr << "Could not initialize vulkan\n";
    return false;
  } else {
    std::cerr << "Vulkan initialized\n";
  }

  if (!create_device()) {
    return false;
  }

#ifdef _WIN32
  VkWin32SurfaceCreateInfoKHR surf_info = { };
  surf_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  surf_info.pNext = nullptr;
  surf_info.flags = 0;
  surf_info.hwnd = hwnd;
  surf_info.hinstance = GetModuleHandle(nullptr);
  result = vkCreateWin32SurfaceKHR(_instance, &surf_info, nullptr, &_surface);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create win32 vulkan surface\n";
    return false;
  }

  std::cerr << "Win32 surface created\n";
#endif

  if (!create_queues()) {
    return false;
  }

  // Initialize the allocator.
  VmaAllocatorCreateInfo alloc_info = { };
  alloc_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
  alloc_info.vulkanApiVersion = VK_API_VERSION_1_3;
  alloc_info.physicalDevice = _active_physical_device;
  alloc_info.device = _device;
  alloc_info.instance = _instance;
  alloc_info.pVulkanFunctions = nullptr;
  result = vmaCreateAllocator(&alloc_info, &_alloc);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to initialize vma allocator\n";
    return false;
  }

  if (!create_graphics_output(hwnd)) {
    return false;
  }

  if (!create_command_buffer()) {
    return false;
  }

  if (!init_temp()) {
    return false;
  }

  _frame_cycle_index = 0;
  update_frame_objects();

  return true;
}

struct CamParams {
  Matrix4x4 model_mat;
  Matrix4x4 view_mat;
  Matrix4x4 proj_mat;
};
CamParams cam_params;

VkBuffer vk_cam_params_buf = nullptr;
VmaAllocation vk_cam_params_alloc = nullptr;
VkDescriptorSetLayout vk_desc_set_layout = nullptr;
VkPipelineLayout vk_pipeline_layout = nullptr;
VkPipeline vk_pipeline = nullptr;
VkDescriptorPool vk_desc_pool = nullptr;
VkDescriptorSet vk_desc_set = nullptr;
VkDescriptorBufferInfo cam_params_desc_buf_info;

VkShaderModule vk_vtx_module = nullptr;
VkShaderModule vk_frag_module = nullptr;

bool RendererVk::
init_temp() {
  VkResult result;

  Vector3 model_hpr(45, 0, 45);

  cam_params.model_mat = Matrix4x4::from_components(1.0f, 0.0f, model_hpr, 0.0f);
  cam_params.view_mat = Matrix4x4::identity();
  cam_params.view_mat.set_cell(3, 2, 0.0f);
  cam_params.view_mat.set_cell(3, 1, -100.0f);
  cam_params.view_mat.invert();
  cam_params.proj_mat = Matrix4x4::make_perspective_projection(0.942478f, (float)_surface_extents.width / (float)_surface_extents.height, 1.0f, 500.0f);

  VkBufferCreateInfo ub_info = { };
  ub_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  ub_info.pNext = nullptr;
  ub_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  ub_info.size = sizeof(CamParams);
  ub_info.queueFamilyIndexCount = 0;
  ub_info.pQueueFamilyIndices = nullptr;
  ub_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ub_info.flags = 0;
  VmaAllocationCreateInfo ub_alloc_info = { };
  ub_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
  result = vmaCreateBuffer(_alloc, &ub_info, &ub_alloc_info, &vk_cam_params_buf, &vk_cam_params_alloc, nullptr);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create uniform buffer\n";
    return false;
  }
  cam_params_desc_buf_info.buffer = vk_cam_params_buf;
  cam_params_desc_buf_info.offset = 0;
  cam_params_desc_buf_info.range = sizeof(CamParams);
  unsigned char *data = nullptr;
  result = vmaMapMemory(_alloc, vk_cam_params_alloc, (void **)&data);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to map ub memory\n";
    return false;
  }
  memcpy(data, (const void *)&cam_params, sizeof(CamParams));
  vmaUnmapMemory(_alloc, vk_cam_params_alloc);

  std::cerr << "Uniform buffer set up\n";

  std::cerr << "View mat:\n" << cam_params.view_mat << "\n";
  std::cerr << "Proj mat:\n" << cam_params.proj_mat << "\n";

  VkDescriptorSetLayoutBinding layout_binding = { };
  layout_binding.binding = 0;
  layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layout_binding.descriptorCount = 1;
  layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  layout_binding.pImmutableSamplers = nullptr;
  VkDescriptorSetLayoutCreateInfo desc_info = { };
  desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  desc_info.pNext = nullptr;
  desc_info.bindingCount = 1;
  desc_info.pBindings = &layout_binding;
  result = vkCreateDescriptorSetLayout(_device, &desc_info, nullptr, &vk_desc_set_layout);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create descriptor set layout\n";
    return false;
  }

  VkPipelineLayoutCreateInfo pipeline_layout_create_info = { };
  pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create_info.pNext = nullptr;
  VkPushConstantRange pcr = {};
  pcr.size = sizeof(Matrix4x4);
  pcr.offset = 0;
  pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pipeline_layout_create_info.pushConstantRangeCount = 1;
  pipeline_layout_create_info.pPushConstantRanges = &pcr;
  pipeline_layout_create_info.setLayoutCount = 1;
  pipeline_layout_create_info.pSetLayouts = &vk_desc_set_layout;
  result = vkCreatePipelineLayout(_device, &pipeline_layout_create_info, nullptr, &vk_pipeline_layout);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create pipeline layout\n";
  }

  VkDescriptorPoolSize type_count[1];
  type_count[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  type_count[0].descriptorCount = 1;

  VkDescriptorPoolCreateInfo descriptor_pool = { };
  descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool.pNext = nullptr;
  descriptor_pool.maxSets = 1;
  descriptor_pool.poolSizeCount = 1;
  descriptor_pool.pPoolSizes = type_count;
  result = vkCreateDescriptorPool(_device, &descriptor_pool, nullptr, &vk_desc_pool);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create descriptor pool\n";
    return false;
  }

  VkDescriptorSetAllocateInfo desc_alloc_info[1];
  desc_alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  desc_alloc_info[0].pNext = nullptr;
  desc_alloc_info[0].descriptorPool = vk_desc_pool;
  desc_alloc_info[0].descriptorSetCount = 1;
  desc_alloc_info[0].pSetLayouts = &vk_desc_set_layout;
  result = vkAllocateDescriptorSets(_device, desc_alloc_info, &vk_desc_set);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to alloc descriptor set\n";
    return false;
  }

  VkWriteDescriptorSet dswrite = { };
  dswrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  dswrite.pNext = nullptr;
  dswrite.dstSet = vk_desc_set;
  dswrite.descriptorCount = 1;
  dswrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  dswrite.pBufferInfo = &cam_params_desc_buf_info;
  dswrite.dstArrayElement = 0;
  dswrite.dstBinding = 0;
  vkUpdateDescriptorSets(_device, 1, &dswrite, 0, nullptr);

  vk_vtx_module = make_shader_module(read_binary_file("shaders\\simple.vert.spirv"));
  vk_frag_module = make_shader_module(read_binary_file("shaders\\simple.frag.spirv"));

  std::cerr << "Loaded vertex and fragment shaders\n";

  VkVertexInputBindingDescription vi_binding = { };
  vi_binding.binding = 0;
  vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  vi_binding.stride = sizeof(float) * 8;
  VkVertexInputAttributeDescription vi_attribs[3];
  vi_attribs[0].binding = 0;
  vi_attribs[0].location = 0;
  vi_attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  vi_attribs[0].offset = 0;
  vi_attribs[1].binding = 0;
  vi_attribs[1].location = 2;
  vi_attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  vi_attribs[1].offset = 20;
  vi_attribs[2].binding = 0;
  vi_attribs[2].location = 1;
  vi_attribs[2].format = VK_FORMAT_R32G32_SFLOAT;
  vi_attribs[2].offset = 12;


  // PIPELINE
  VkDynamicState dynamic_states[2];
  VkPipelineDynamicStateCreateInfo dynamic = { };
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.pDynamicStates = dynamic_states;
  dynamic.dynamicStateCount = 0;
  VkPipelineVertexInputStateCreateInfo vi = { };
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vi.flags = 0;
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &vi_binding;
  vi.vertexAttributeDescriptionCount = 3;
  vi.pVertexAttributeDescriptions = vi_attribs;
  VkPipelineInputAssemblyStateCreateInfo ia = { };
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.pNext = nullptr;
  ia.flags = 0;
  ia.primitiveRestartEnable = VK_FALSE;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineRasterizationStateCreateInfo rs = { };
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.pNext = nullptr;
  rs.flags = 0;
  rs.cullMode = VK_CULL_MODE_BACK_BIT;
  rs.depthBiasClamp = 0.0f;
  rs.depthBiasEnable = VK_FALSE;
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.lineWidth = 1.0f;
  VkPipelineColorBlendStateCreateInfo cb = { };
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.pNext = nullptr;
  cb.flags = 0;
  VkPipelineColorBlendAttachmentState att_state[1];
  att_state[0].colorWriteMask = 0xf;
  att_state[0].blendEnable = VK_FALSE;
  att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
  att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
  att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  cb.attachmentCount = 1;
  cb.pAttachments = att_state;
  cb.logicOpEnable = VK_FALSE;
  cb.logicOp = VK_LOGIC_OP_NO_OP;
  cb.blendConstants[0] = 1.0f;
  cb.blendConstants[1] = 1.0f;
  cb.blendConstants[2] = 1.0f;
  cb.blendConstants[3] = 1.0f;
  VkPipelineViewportStateCreateInfo vp = { };
  vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp.pNext = nullptr;
  vp.flags = 0;
  vp.viewportCount = 1;
  dynamic_states[dynamic.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
  vp.scissorCount = 1;
  dynamic_states[dynamic.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
  vp.pScissors = nullptr;
  vp.pViewports = nullptr;
  VkPipelineDepthStencilStateCreateInfo ds = { };
  ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  ds.pNext = nullptr;
  ds.flags = 0;
  ds.depthTestEnable = VK_TRUE;
  ds.depthWriteEnable = VK_TRUE;
  ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds.depthBoundsTestEnable = VK_FALSE;
  ds.minDepthBounds = 0.0f;
  ds.maxDepthBounds = 0.0f;
  ds.stencilTestEnable = VK_FALSE;
  VkPipelineMultisampleStateCreateInfo ms = { };
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.pNext = nullptr;
  ms.flags = 0;
  ms.pSampleMask = nullptr;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  ms.sampleShadingEnable = VK_FALSE;
  ms.alphaToCoverageEnable = VK_FALSE;
  ms.alphaToOneEnable = VK_FALSE;
  ms.minSampleShading = 0.0f;
  VkPipelineShaderStageCreateInfo shader_stages[2];
  shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[0].pNext = nullptr;
  shader_stages[0].pSpecializationInfo = nullptr;
  shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stages[0].pName = "main";
  shader_stages[0].module = vk_vtx_module;
  shader_stages[0].flags = 0;
  shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[1].pNext = nullptr;
  shader_stages[1].pSpecializationInfo = nullptr;
  shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_stages[1].pName = "main";
  shader_stages[1].module = vk_frag_module;
  shader_stages[1].flags = 0;
  VkPipelineRenderingCreateInfo r_info = { };
  r_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  r_info.pNext = nullptr;
  r_info.colorAttachmentCount = 1;
  r_info.pColorAttachmentFormats = &_surface_color_format;
  r_info.depthAttachmentFormat = _surface_depth_format;
  r_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
  r_info.viewMask = 0;
  VkGraphicsPipelineCreateInfo pipeline = { };
  pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline.pNext = &r_info;
  pipeline.layout = vk_pipeline_layout;
  pipeline.basePipelineHandle = VK_NULL_HANDLE;
  pipeline.basePipelineIndex = 0;
  pipeline.flags = 0;
  pipeline.pVertexInputState = &vi;
  pipeline.pInputAssemblyState = &ia;
  pipeline.pRasterizationState = &rs;
  pipeline.pColorBlendState = &cb;
  pipeline.pTessellationState = nullptr;
  pipeline.pMultisampleState = &ms;
  pipeline.pDynamicState = &dynamic;
  pipeline.pViewportState = &vp;
  pipeline.pDepthStencilState = &ds;
  pipeline.pStages = shader_stages;
  pipeline.stageCount = 2;
  pipeline.renderPass = nullptr;
  pipeline.subpass = 0;
  result = vkCreateGraphicsPipelines(_device, nullptr, 1, &pipeline, nullptr, &vk_pipeline);
  if (!vk_error_check(result, "create pipeline")) {
    return false;
  }
  std::cerr << "Pipeline created!\n";

  return true;
}

bool RendererVk::
create_command_buffer() {
  VkCommandPoolCreateInfo pool_info = { };
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.pNext = nullptr;
  pool_info.queueFamilyIndex = _gfx_queue_family_index;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VkResult result;

  result = vkCreateCommandPool(_device, &pool_info, nullptr, &_cmd_pool);
  if (result != VK_SUCCESS) {
    std::cerr << "Couldn't create vk command pool\n";
    return false;
  }

  VkCommandBufferAllocateInfo cmd = { };
  cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd.pNext = nullptr;
  cmd.commandPool = _cmd_pool;
  cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd.commandBufferCount = _num_frames;

  result = vkAllocateCommandBuffers(_device, &cmd, _command_buffers);
  if (result != VK_SUCCESS) {
    std::cerr << "Couldn't create vk command buffers\n";
    return false;
  }

  pool_info.queueFamilyIndex = _transfer_queue_family_index;
  result = vkCreateCommandPool(_device, &pool_info, nullptr, &_transfer_cmd_pool);
  if (!vk_error_check(result, "create transfer cmd pool")) {
    return false;
  }
  cmd.commandPool = _transfer_cmd_pool;
  result = vkAllocateCommandBuffers(_device, &cmd, _transfer_command_buffers);
  if (!vk_error_check(result, "create transfer cmd buffers")) {
    return false;
  }

  // Create synrhonization primitives.

  VkSemaphoreCreateInfo sema_info = { };
  sema_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sema_info.pNext = nullptr;
  sema_info.flags = 0u;
  VkFenceCreateInfo fence_info = { };
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.pNext = nullptr;
  // Start it signaled because we wait on it in begin_frame.
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (int i = 0; i < _num_frames; ++i) {
    // This one is signaled when the swapchain image is available.
    result = vkCreateSemaphore(_device, &sema_info, nullptr, &_image_acquired_semaphores[i]);
    if (!vk_error_check(result, "Create img acquired semaphore")) {
      return false;
    }
    // This one is signaled when the command buffer finishes executing and present
    // can happen.
    result = vkCreateSemaphore(_device, &sema_info, nullptr, &_draw_semaphores[i]);
    if (!vk_error_check(result, "create draw complete semaphore")) {
      return false;
    }
    // This one is also signaled when the command buffer finishes executing, but
    // is signaled from the GPU to the CPU.  This lets us know when the command
    // buffer is ready to be re-used.
    result = vkCreateFence(_device, &fence_info, nullptr, &_draw_fences[i]);
    if (!vk_error_check(result, "create draw complete fence")) {
      return false;
    }

    // This one gets signaled on GPU when the transfer command buffer is completed.
    result = vkCreateSemaphore(_device, &sema_info, nullptr, &_transfer_semaphores[i]);
    if (!vk_error_check(result, "transfer semaphore create")) {
      return false;
    }

    // This one gets signaled by GPU to CPU when the transfer command buffer is completed.
    result = vkCreateFence(_device, &fence_info, nullptr, &_transfer_fences[i]);
    if (!vk_error_check(result, "transfer fence create")) {
      return false;
    }
  }

  return true;
}

bool RendererVk::
create_device() {
  // Enumerate devices.

  VkResult result;

  uint32_t device_count = 0;
  result = vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
  assert(!result);
  _physical_devices.resize(device_count);
  result = vkEnumeratePhysicalDevices(_instance, &device_count, _physical_devices.data());
  assert(!result);
  _physical_device_properties.resize(device_count);
  for (uint32_t i = 0; i < device_count; ++i) {
    vkGetPhysicalDeviceProperties(_physical_devices[i], &_physical_device_properties[i]);
  }

  //for (uint32_t i = 0; i <

  // Pick in the order of: discrete, integrated, virtual.

  std::cerr << device_count << " graphics devices\n";
  for (uint32_t i = 0; i < device_count; ++i) {
    std::cerr << "Device " << i << "\n";
    VkPhysicalDeviceProperties &device_props = _physical_device_properties[i];
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

  _device_index = UINT32_MAX;

  // First look for discrete.
  for (int i = 0; i < (int)_physical_device_properties.size(); ++i) {
    if (_physical_device_properties[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      _device_index = i;
      break;
    }
  }
  if (_device_index == UINT32_MAX) {
    // Don't have a discrete, look for integrated.
    for (int i = 0; i < (int)_physical_device_properties.size(); ++i) {
      if (_physical_device_properties[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        _device_index = i;
        break;
      }
    }
  }
  if (_device_index == UINT32_MAX) {
    // Didn't get a discrete or integrated chip.  Fail.
    std::cerr << "No discrete or integrated graphics device available!\n";
    return false;
  }

  _active_physical_device = _physical_devices[_device_index];

  std::cerr << "Using graphics device " << _device_index << " (" << std::string(_physical_device_properties[_device_index].deviceName) << ")\n";

  return true;
}

bool RendererVk::
create_queues() {
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(_active_physical_device, &queue_family_count, nullptr);
  _queue_family_properties.resize(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(_active_physical_device, &queue_family_count, _queue_family_properties.data());

  VkResult result;

  VkBool32 *supports_present = (VkBool32 *)alloca(queue_family_count * sizeof(VkBool32));
  // Find present queue.
  for (uint32_t i = 0; i < queue_family_count; ++i) {
    vkGetPhysicalDeviceSurfaceSupportKHR(_active_physical_device, i, _surface, &supports_present[i]);
  }
  for (size_t i = 0; i < queue_family_count; ++i) {
    if (_queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      if (_gfx_queue_family_index == -1) {
        _gfx_queue_family_index = (int)i;
      }
      if (supports_present[i] == VK_TRUE) {
        _gfx_queue_family_index = (int)i;
        _present_queue_family_index = (int)i;
        break;
      }
    }
  }

  if (_present_queue_family_index == -1) {
    // Present queue wasn't in gfx queue family.  Find another
    // queue for present.
    for (size_t i = 0; i < queue_family_count; ++i) {
      if (supports_present[i] == VK_TRUE) {
        _present_queue_family_index = (int)i;
        break;
      }
    }
  }
  if (_present_queue_family_index == -1 || _gfx_queue_family_index == -1) {
    std::cerr << "Couldn't find a present and/or graphics queue family!\n";
    return false;
  }

  // Find a transfer queue.
  _transfer_queue_family_index = -1;
  for (size_t i = 0; i < queue_family_count; ++i) {
    if (_queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
      _transfer_queue_family_index = (int)i;
      break;
    }
  }
  if (_transfer_queue_family_index == -1) {
    std::cerr << "Couldn't find a transfer queue family!\n";
    return false;
  }

  std::cerr << "Chose graphics queue family " << _gfx_queue_family_index << "\n";
  std::cerr << "Chose present queue family " << _present_queue_family_index << "\n";
  std::cerr << "Chose transfer queue family " << _transfer_queue_family_index << "\n";

  bool pres_separate_from_gfx = (_gfx_queue_family_index != _present_queue_family_index);

  int queue_count = 1;
  float queue_prior[1] = { 0.0f };
  VkDeviceQueueCreateInfo queue_infos[3];
  queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[0].pNext = nullptr;
  queue_infos[0].queueCount = 1;
  queue_infos[0].pQueuePriorities = queue_prior;
  queue_infos[0].queueFamilyIndex = _gfx_queue_family_index;
  queue_infos[0].flags = 0;
  if (pres_separate_from_gfx) {
    ++queue_count;
    queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[1].pNext = nullptr;
    queue_infos[1].queueCount = 1;
    queue_infos[1].pQueuePriorities = queue_prior;
    queue_infos[1].queueFamilyIndex = _present_queue_family_index;
    queue_infos[1].flags = 0;
  }
  if (_transfer_queue_family_index != _gfx_queue_family_index) {
    ++queue_count;
    queue_infos[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[2].pNext = nullptr;
    queue_infos[2].queueCount = 1;
    queue_infos[2].pQueuePriorities = queue_prior;
    queue_infos[2].queueFamilyIndex = _transfer_queue_family_index;
    queue_infos[2].flags = 0;
  }

  const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
  uint32_t device_extension_count = 2;
  VkDeviceCreateInfo device_info = { };
  VkPhysicalDeviceDynamicRenderingFeatures dynamic_features = { };
  dynamic_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
  dynamic_features.pNext = nullptr;
  dynamic_features.dynamicRendering = VK_TRUE;
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = &dynamic_features;
  device_info.queueCreateInfoCount = queue_count;
  device_info.pQueueCreateInfos = queue_infos;
  device_info.enabledExtensionCount = device_extension_count;
  device_info.ppEnabledExtensionNames = device_extensions;
  device_info.enabledLayerCount = 0;
  device_info.ppEnabledLayerNames = nullptr;
  device_info.pEnabledFeatures = nullptr;

  result = vkCreateDevice(_active_physical_device, &device_info, nullptr, &_device);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create vulkan device\n";
    return false;
  } else {
    std::cerr << "Created vulkan device\n";
  }

  vkGetDeviceQueue(_device, _gfx_queue_family_index, 0u, &_gfx_queue);
  vkGetDeviceQueue(_device, _present_queue_family_index, 0u, &_present_queue);
  vkGetDeviceQueue(_device, _transfer_queue_family_index, 0u, &_transfer_queue);

  return true;
}

// Given a GraphicsDisplaySurface with an already assigned WindowHandle,
// constructs all the necessary Vulkan objects to get the display
// surface up and running for rendering into to be displayed on
// the window.
bool RendererVk::
create_graphics_output(WindowHandle hwnd) {

  VkResult result;

  // Query the available surface color formats.
  uint32_t format_count = 0;
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(_active_physical_device, _surface, &format_count, nullptr);
  assert(!result);
  vector<VkSurfaceFormatKHR> surf_formats;
  surf_formats.resize(format_count);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(_active_physical_device, _surface, &format_count, surf_formats.data());
  assert(!result);

  std::cerr << "Got surface formats\n";

  // We can use any of these surface formats.
  // TODO(brian): make desired surface format configurable?
  // For example, if we want alpha in the surface?
  VkFormat potential_surf_formats[] = {
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_R8G8B8_SRGB,
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_B8G8R8_SRGB,
  };

  VkFormat surf_format = VK_FORMAT_UNDEFINED;
  if (format_count == 1u && surf_formats[0].format == VK_FORMAT_UNDEFINED) {
    // Don't know, so use one of our preferred formats.
    surf_format = potential_surf_formats[0];
  } else {
    assert(format_count >= 1u);
    // Match on one of the supported formats.
    for (size_t i = 0; i < surf_formats.size(); ++i) {
      for (size_t j = 0; j < ARRAYSIZE(potential_surf_formats); ++j) {
        if (surf_formats[i].format == potential_surf_formats[j]) {
          surf_format = surf_formats[i].format;
        }
      }
    }
    surf_format = surf_formats[0].format;
  }

  _surface_color_format = surf_format;

  VkSurfaceCapabilitiesKHR surf_caps;
  result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_active_physical_device, _surface, &surf_caps);
  assert(!result);

  std::cerr << "Got surface caps\n";

  uint32_t present_mode_count = 0;
  result = vkGetPhysicalDeviceSurfacePresentModesKHR(_active_physical_device, _surface, &present_mode_count, nullptr);
  assert(!result);
  vector<VkPresentModeKHR> present_modes;
  present_modes.resize(present_mode_count);
  result = vkGetPhysicalDeviceSurfacePresentModesKHR(_active_physical_device, _surface, &present_mode_count, present_modes.data());
  assert(!result);

  std::cerr << "Got present modes\n";

  VkExtent2D swapchain_extent;
  // currentExtent looks to be the current window size.
  if (surf_caps.currentExtent.width == 0xFFFFFFFF) {
    // Surface size is undefined? Pick arbitrary for now.
    swapchain_extent.width = std::min(800u, surf_caps.minImageExtent.width);
    swapchain_extent.height = std::min(600u, surf_caps.minImageExtent.height);
  } else {
    swapchain_extent = surf_caps.currentExtent;
  }

  _surface_extents.width = swapchain_extent.width;
  _surface_extents.height = swapchain_extent.height;

  // V-Sync where if frame rate drops below v-sync, it simply stops v-syncing.
  VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

  // Don't do more than double-buffering.
  uint32_t num_swapchain_images = std::clamp(2u, surf_caps.minImageCount, surf_caps.maxImageCount);

  VkSurfaceTransformFlagBitsKHR pre_transform;
  if (surf_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
    pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  } else {
    pre_transform = surf_caps.currentTransform;
  }

  VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  VkCompositeAlphaFlagBitsKHR composite_alpha_flags[4] = {
    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
    VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
  };
  for (uint32_t i = 0; i < ARRAYSIZE(composite_alpha_flags); ++i) {
    if (surf_caps.supportedCompositeAlpha & composite_alpha_flags[i]) {
      composite_alpha = composite_alpha_flags[i];
      break;
    }
  }

  VkSwapchainCreateInfoKHR swapchain_ci = { };
  swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_ci.pNext = nullptr;
  swapchain_ci.surface = _surface;
  swapchain_ci.minImageCount = num_swapchain_images;
  swapchain_ci.imageFormat = surf_format;
  swapchain_ci.imageExtent.width = swapchain_extent.width;
  swapchain_ci.imageExtent.height = swapchain_extent.height;
  swapchain_ci.preTransform = pre_transform;
  swapchain_ci.compositeAlpha = composite_alpha;
  swapchain_ci.imageArrayLayers = 1;
  swapchain_ci.presentMode = swapchain_present_mode;
  swapchain_ci.oldSwapchain = VK_NULL_HANDLE;
  swapchain_ci.clipped = true;
  swapchain_ci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchain_ci.queueFamilyIndexCount = 0;
  swapchain_ci.pQueueFamilyIndices = nullptr;
  uint32_t queue_family_indices[] = { (uint32_t)_gfx_queue_family_index, (uint32_t)_present_queue_family_index };
  if (_gfx_queue_family_index != _present_queue_family_index) {
    // Graphics and present queues are from different families, so we need to share
    // the swapchain images between them.
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_ci.queueFamilyIndexCount = 2;
    swapchain_ci.pQueueFamilyIndices = queue_family_indices;
  }

  result = vkCreateSwapchainKHR(_device, &swapchain_ci, nullptr, &_swapchain);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create vulkan swapchain\n";
    return false;
  }

  std::cerr << "Created swapchain\n";

  // Create swapchain images.
  uint32_t swapchain_image_count = 0;
  result = vkGetSwapchainImagesKHR(_device, _swapchain, &swapchain_image_count, nullptr);
  assert(!result);
  _swapchain_images.resize(swapchain_image_count);
  result = vkGetSwapchainImagesKHR(_device, _swapchain, &swapchain_image_count, _swapchain_images.data());
  assert(!result);

  _swapchain_image_views.resize(swapchain_image_count);

  // Make image views.
  for (uint32_t i = 0; i < swapchain_image_count; ++i) {
    VkImageViewCreateInfo color_image_view = { };
    color_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    color_image_view.pNext = nullptr;
    color_image_view.flags = 0;
    color_image_view.image = _swapchain_images[i];
    color_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    color_image_view.format = surf_format;
    color_image_view.components.r = VK_COMPONENT_SWIZZLE_R;
    color_image_view.components.g = VK_COMPONENT_SWIZZLE_G;
    color_image_view.components.b = VK_COMPONENT_SWIZZLE_B;
    color_image_view.components.a = VK_COMPONENT_SWIZZLE_A;
    color_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    color_image_view.subresourceRange.baseMipLevel = 0;
    color_image_view.subresourceRange.levelCount = 1;
    color_image_view.subresourceRange.baseArrayLayer = 0;
    color_image_view.subresourceRange.layerCount = 1;

    result = vkCreateImageView(_device, &color_image_view, nullptr, &_swapchain_image_views[i]);
    if (result != VK_SUCCESS) {
      std::cerr << "Failed to create image view for swapchain image " << i << "\n";
      return false;
    }
  }

  // Depth buffer.
  VkImageCreateInfo d_image_info = { };
  VkFormat depth_format = VK_FORMAT_D16_UNORM;
  _surface_depth_format = depth_format;
  VkFormatProperties fmt_props;
  vkGetPhysicalDeviceFormatProperties(_active_physical_device, depth_format, &fmt_props);
  if (fmt_props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    d_image_info.tiling = VK_IMAGE_TILING_LINEAR;
  } else if (fmt_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    d_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  } else {
    std::cerr << "Unsupported depth format D16 unorm\n";
    return false;
  }
  d_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  d_image_info.pNext = nullptr;
  d_image_info.imageType = VK_IMAGE_TYPE_2D;
  d_image_info.format = depth_format;
  d_image_info.extent.width = swapchain_extent.width;
  d_image_info.extent.height = swapchain_extent.height;
  d_image_info.extent.depth = 1;
  d_image_info.mipLevels = 1;
  d_image_info.arrayLayers = 1;
  d_image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  d_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  d_image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  d_image_info.queueFamilyIndexCount = 0;
  d_image_info.pQueueFamilyIndices = nullptr;
  d_image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  d_image_info.flags = 0;

  VkImageViewCreateInfo d_image_view = { };
  d_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  d_image_view.pNext = nullptr;
  d_image_view.format = depth_format;
  d_image_view.components.r = VK_COMPONENT_SWIZZLE_R;
  d_image_view.components.g = VK_COMPONENT_SWIZZLE_G;
  d_image_view.components.b = VK_COMPONENT_SWIZZLE_B;
  d_image_view.components.a = VK_COMPONENT_SWIZZLE_A;
  d_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  d_image_view.subresourceRange.baseMipLevel = 0;
  d_image_view.subresourceRange.levelCount = 1;
  d_image_view.subresourceRange.baseArrayLayer = 0;
  d_image_view.subresourceRange.layerCount = 1;
  d_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  d_image_view.flags = 0;
  VmaAllocationCreateInfo d_alloc_info = { };
  d_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  result = vmaCreateImage(_alloc, &d_image_info, &d_alloc_info, &_depth_image, &_depth_image_alloc, nullptr);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create depth buffer image!\n";
    return false;
  }
  d_image_view.image = _depth_image;

  // Create depth buffer image view.
  result = vkCreateImageView(_device, &d_image_view, nullptr, &_depth_image_view);
  if (result != VK_SUCCESS) {
    std::cerr << "Failed to create depth buffer image view!\n";
    return false;
  }


  std::cerr << "Make framebuffer\n";

  return true;
}

// Marks the beginning of the pre-rendering transfer phase.
// Allows any CPU to GPU transfers to be queued and submitted before
// rendering work begins.
bool RendererVk::
begin_prepare() {
  VkResult result;

  process_deletions();

  // Wait for the transfer command buffer to be ready.
  result = vkWaitForFences(_device, 1, &_current_transfer_fence, VK_TRUE, UINT64_MAX);
  if (!vk_error_check(result, "wait for transfer cmd buf")) {
    return false;
  }
  result = vkResetFences(_device, 1, &_current_transfer_fence);
  if (!vk_error_check(result, "reset transfer fence")) {
    return false;
  }
  result = vkResetCommandBuffer(_current_transfer_command_buffer, 0);
  if (!vk_error_check(result, "reset transfer cmd buf")) {
    return false;
  }

  // Begin the transfer command buffer.
  VkCommandBufferBeginInfo begin_info = { };
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext = nullptr;
  begin_info.pInheritanceInfo = nullptr;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  result = vkBeginCommandBuffer(_current_transfer_command_buffer, &begin_info);
  if (!vk_error_check(result, "begin transfer cmd buf")) {
    return false;
  }

  return true;
}

bool RendererVk::
end_prepare() {
  VkResult result;

  result = vkEndCommandBuffer(_current_transfer_command_buffer);
  if (!vk_error_check(result, "end transfer cmd buf")) {
    return false;
  }

  // Now submit the transfer(s).
  VkSubmitInfo submit_info = { };
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = nullptr;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &_current_transfer_command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &_current_transfer_semaphore;
  submit_info.waitSemaphoreCount = 0;
  submit_info.pWaitSemaphores = nullptr;
  submit_info.pWaitDstStageMask = nullptr;
  result = vkQueueSubmit(_transfer_queue, 1, &submit_info, _current_transfer_fence);
  if (!vk_error_check(result, "submit transfer cmd buf")) {
    return false;
  }

  return true;
}

// Enters the command buffer into the recording state.
// Waits on the command buffer to become available before doing so.
bool RendererVk::
begin_frame() {
  VkResult result;

  // Wait for the current command buffer to become ready.
  result = vkWaitForFences(_device, 1, &_current_draw_fence, VK_TRUE, UINT64_MAX);
  if (!vk_error_check(result, "wait for cmd buf")) {
    return false;
  }
  result = vkResetFences(_device, 1, &_current_draw_fence);
  if (!vk_error_check(result, "reset cmd buf draw fence")) {
    return false;
  }

  // Okay, the GPU is no longer using this command buffer.  We're ready!
  result = vkResetCommandBuffer(_current_command_buffer, 0);
  if (!vk_error_check(result, "reset cmd buf")) {
    return false;
  }

  VkCommandBufferBeginInfo begin_info = { };
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext = nullptr;
  begin_info.pInheritanceInfo = nullptr;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  result = vkBeginCommandBuffer(_current_command_buffer, &begin_info);
  if (!vk_error_check(result, "begin cmd buf")) {
    return false;
  }

  return true;
}

// Begins drawing to the surface/graphics output.
bool RendererVk::
begin_frame_surface() {
  VkResult result;

  // Get a swapchain image.
  result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _current_image_acquired_semaphore, VK_NULL_HANDLE, &_curr_swapchain_image_index);
  if (!vk_error_check(result, "acquire swapchain image")) {
    return false;
  }

  // Transition the swapchain color image.
  VkImageMemoryBarrier render_barrier = { };
  render_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  render_barrier.pNext = nullptr;
  render_barrier.image = _swapchain_images[_curr_swapchain_image_index];
  render_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  render_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  render_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  render_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  render_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  render_barrier.subresourceRange.baseMipLevel = 0;
  render_barrier.subresourceRange.levelCount = 1;
  render_barrier.subresourceRange.baseArrayLayer = 0;
  render_barrier.subresourceRange.layerCount = 1;
  render_barrier.srcAccessMask = 0;
  render_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  vkCmdPipelineBarrier(_current_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &render_barrier);

  // Bind our framebuffer attachments, clear information, load/store ops, etc.
  VkRenderingAttachmentInfo color_attach = { };
  color_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attach.pNext = nullptr;
  color_attach.imageView = _swapchain_image_views[_curr_swapchain_image_index];
  color_attach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attach.resolveMode = VK_RESOLVE_MODE_NONE;
  color_attach.resolveImageView = nullptr;
  color_attach.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attach.clearValue.color.float32[0] = 0.3f;
  color_attach.clearValue.color.float32[1] = 0.3f;
  color_attach.clearValue.color.float32[2] = 0.3f;
  color_attach.clearValue.color.float32[3] = 1.0f;
  VkRenderingAttachmentInfo depth_attach = { };
  depth_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attach.pNext = nullptr;
  depth_attach.imageView = _depth_image_view;
  depth_attach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depth_attach.resolveMode = VK_RESOLVE_MODE_NONE;
  depth_attach.resolveImageView = nullptr;
  depth_attach.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attach.clearValue.depthStencil.depth = 1.0f;
  depth_attach.clearValue.depthStencil.stencil = 0u;
  VkRenderingInfo render_info = { };
  render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  render_info.pNext = nullptr;
  render_info.layerCount = 1;
  render_info.colorAttachmentCount = 1;
  render_info.pColorAttachments = &color_attach;
  render_info.pDepthAttachment = &depth_attach;
  render_info.pStencilAttachment = nullptr;
  render_info.flags = 0;//VK_RENDERING_CONTENTS_INLINE_BIT_EXT;
  render_info.viewMask = 0;
  render_info.renderArea.offset.x = 0;
  render_info.renderArea.offset.y = 0;
  render_info.renderArea.extent.width = _surface_extents.width;
  render_info.renderArea.extent.height = _surface_extents.height;
  vkCmdBeginRendering(_current_command_buffer, &render_info);

  // Supply viewport for rendering into.
  VkViewport viewport = { };
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = _surface_extents.width;
  viewport.height = _surface_extents.height;
  vkCmdSetViewport(_current_command_buffer, 0, 1, &viewport);

  // Scissor region.
  VkRect2D scissor = { };
  scissor.extent.width = _surface_extents.width;
  scissor.extent.height = _surface_extents.height;
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  vkCmdSetScissor(_current_command_buffer, 0, 1, &scissor);

  return true;
}

bool RendererVk::
end_frame_surface() {
  // End rendering.
  vkCmdEndRendering(_current_command_buffer);

  // Transition to present format.
  VkImageMemoryBarrier render_barrier = { };
  render_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  render_barrier.pNext = nullptr;
  render_barrier.image = _swapchain_images[_curr_swapchain_image_index];
  render_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  render_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  render_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  render_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  render_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  render_barrier.subresourceRange.baseMipLevel = 0;
  render_barrier.subresourceRange.levelCount = 1;
  render_barrier.subresourceRange.baseArrayLayer = 0;
  render_barrier.subresourceRange.layerCount = 1;
  render_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  render_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  vkCmdPipelineBarrier(_current_command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &render_barrier);

  return true;
}

// Enqueues a buffer for deletion.  The current frame cycle index is noted
// along with the request.  The buffer won't actually be deleted until the
// the command buffer associated with the frame cycle index is no longer being
// processed by the GPU.
void RendererVk::enqueue_buffer_deletion(VkBuffer buffer, VmaAllocation alloc) {
  VkDeletionRequest req;
  req.buffer = buffer;
  req.alloc = alloc;
  req.wait_fence = nullptr;
  std::cout << "Enqueing buf " << buffer << " to delete\n";
  _deletion_queue.push_back(std::move(req));
}


void RendererVk::process_deletions() {
  if (_deletion_queue.empty()) {
    return;
  }

  for (int i = (int)_deletion_queue.size() - 1; i >= 0; --i) {
    if (_deletion_queue[i].wait_fence == nullptr) {
      VkFenceCreateInfo fence_info = {};
      fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence_info.pNext = nullptr;
      fence_info.flags = 0;
      VkResult result = vkCreateFence(_device, &fence_info, nullptr,
                                      &_deletion_queue[i].wait_fence);
      assert(result == VK_SUCCESS);
      _created_deletion_fences.push_back(_deletion_queue[i].wait_fence);
      std::cout << "Created wait fence for " << _deletion_queue[i].buffer << " to be deleted\n";

    } else if (vkGetFenceStatus(_device, _deletion_queue[i].wait_fence) ==
               VK_SUCCESS) {
      // The fence was signaled, meaning that the device is finished with the
      // buffer.  We can go ahead and delete it now.
      std::cout << "Deletion wait fence for " << _deletion_queue[i].buffer << " is signaled, deleting now\n";
      vmaDestroyBuffer(_alloc, _deletion_queue[i].buffer,
                       _deletion_queue[i].alloc);
      vkDestroyFence(_device, _deletion_queue[i].wait_fence, nullptr);
      _deletion_queue.erase(_deletion_queue.begin() + i);
    }
  }
}

// Submits the command buffer and queues the present operation.
// If necessary in the future, we could split the present out into a
// separate method that presents all windows (if we have multiple windows).
bool RendererVk::
end_frame() {
  VkResult result;

  result = vkEndCommandBuffer(_current_command_buffer);
  if (!vk_error_check(result, "end cmd buf")) {
    return false;
  }

  // Now submit.
  VkCommandBuffer bufs[] = { _current_command_buffer };
  // Wait on the swapchain image *and* for all transfers to complete.
  VkPipelineStageFlags pipe_flags[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };
  VkSemaphore wait_semas[2] = { _current_image_acquired_semaphore, _current_transfer_semaphore };
  VkSubmitInfo submit_info = { };
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = nullptr;
  // Wait, on the gpu, for the current swapchain image to become available,
  // before writing to the color attachment (which would be the swapchain image).
  submit_info.waitSemaphoreCount = 2;
  submit_info.pWaitSemaphores = wait_semas;
  submit_info.pWaitDstStageMask = pipe_flags;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = bufs;
  // Signal this semaphore on the GPU when the command buffer finishes.
  // The present operation will wait on this semaphore.
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &_current_draw_semaphore;
  // _current_draw_fence will be signaled to the CPU when the command buffer
  // is finished on the GPU side and can be re-used.
  result = vkQueueSubmit(_gfx_queue, 1, &submit_info, _current_draw_fence);
  if (!vk_error_check(result, "submit cmd buf")) {
    return false;
  }

  if (!_created_deletion_fences.empty()) {
    // Tack on fence signals to know when resources queued for deletion
    // are done being used.
    VkSubmitInfo fence_submit = {};
    fence_submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    fence_submit.pNext = nullptr;
    fence_submit.commandBufferCount = 0;
    fence_submit.pCommandBuffers = nullptr;
    fence_submit.signalSemaphoreCount = 0;
    fence_submit.pSignalSemaphores = nullptr;
    fence_submit.waitSemaphoreCount = 0;
    fence_submit.pWaitSemaphores = nullptr;
    fence_submit.pWaitDstStageMask = nullptr;
    for (const VkFence &fence : _created_deletion_fences) {
      result = vkQueueSubmit(_gfx_queue, 1, &fence_submit, fence);
      if (!vk_error_check(result, "submit deletion fence")) {
        return false;
      }
    }
    _created_deletion_fences.clear();
  }

  // Now, present!
  VkPresentInfoKHR present_info = { };
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = nullptr;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &_swapchain;
  present_info.pImageIndices = &_curr_swapchain_image_index;
  // Wait, on the GPU, for the semaphore signaled by the GPU
  // when the command buffer finishes executing.  We can present
  // at that point.
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &_current_draw_semaphore;
  present_info.pResults = nullptr;
  result = vkQueuePresentKHR(_present_queue, &present_info);
  if (!vk_error_check(result, "queue present")) {
    return false;
  }

  // Cycle to the next frame to record another frame while the
  // GPU is processing the one we just submitted.
  cycle_frame();

  return true;
}

bool RendererVk::draw(const VertexData *vdata, const IndexData *idata,
                      int first_vertex, int num_vertices) {
  const VkVertexData *vk_vdata = (const VkVertexData *)vdata;
  const VkIndexData *vk_idata = (const VkIndexData *)idata;

  vkCmdBindPipeline(_current_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);
  vkCmdBindDescriptorSets(_current_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout,
                          0, 1, &vk_desc_set, 0, nullptr);

  bool indexed = vk_idata != nullptr;

  if (indexed) {
    vkCmdBindIndexBuffer(_current_command_buffer, vk_idata->gpu_buffer, 0, get_vk_index_type(vk_idata->type));
  }

  if (num_vertices <= 0) {
    if (indexed) {
      assert(first_vertex >= 0 && first_vertex < vk_idata->get_num_indices());
      num_vertices = vk_idata->get_num_indices() - first_vertex;
    } else {
      assert(first_vertex >= 0 && first_vertex < vk_vdata->get_num_vertices());
      num_vertices = vk_vdata->get_num_vertices() - first_vertex;
    }
  }

  size_t vbuf_count = vk_vdata->vk_buffers.size();
  VkBuffer *vkbufs = (VkBuffer *)alloca(sizeof(VkBuffer) * vbuf_count);
  for (int i = 0; i < vbuf_count; ++i) {
    vkbufs[i] = vk_vdata->vk_buffers[i].gpu_buffer;
  }
  VkDeviceSize *offsets = (VkDeviceSize *)alloca(sizeof(VkDeviceSize) * vbuf_count);
  for (int i = 0; i < vbuf_count; ++i) {
    offsets[i] = 0;
  }
  vkCmdBindVertexBuffers(_current_command_buffer, 0, vbuf_count, vkbufs, offsets);

  if (indexed) {
    vkCmdDrawIndexed(_current_command_buffer, num_vertices, 1, first_vertex, 0, 0);
  } else {
    vkCmdDraw(_current_command_buffer, num_vertices, 1, first_vertex, 0);
  }

  return true;
}

bool RendererVk::draw_mesh(const Mesh *mesh) {
  // TODO: set primitive topology, needs pipeline switch.
  return draw(mesh->vertex_data, mesh->index_data, mesh->first_vertex,
              mesh->num_vertices);
}

// Cycles the command buffer in use by the CPU for recording commands.
void RendererVk::
cycle_frame() {
  _frame_cycle_index++;
  _frame_cycle_index %= _num_frames;
  update_frame_objects();
}

void RendererVk::
update_frame_objects() {
  _current_command_buffer = _command_buffers[_frame_cycle_index];
  _current_draw_fence = _draw_fences[_frame_cycle_index];
  _current_draw_semaphore = _draw_semaphores[_frame_cycle_index];
  _current_image_acquired_semaphore = _image_acquired_semaphores[_frame_cycle_index];
  _current_transfer_semaphore = _transfer_semaphores[_frame_cycle_index];
  _current_transfer_command_buffer = _transfer_command_buffers[_frame_cycle_index];
  _current_transfer_fence = _transfer_fences[_frame_cycle_index];
}

// Initializes a VkBuffer an enqueues a transfer into device-local memory using
// the provided client-side data buffer.  Ideal for a static vertex/index buffer.
void RendererVk::prepare_buffer(VkBufferBase *buffer, ubyte *data,
                                size_t size, u32 buffer_usage) {
  if (buffer->gpu_buffer != nullptr) {
    return;
  }

  VkResult result;

  VkBufferCreateInfo staging_info = { };
  staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_info.pNext = nullptr;
  staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging_info.flags = 0;
  staging_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  staging_info.size = size;
  staging_info.queueFamilyIndexCount = 0;
  staging_info.pQueueFamilyIndices = nullptr;
  VmaAllocationCreateInfo staging_alloc_info = { };
  staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  VkBuffer staging_buffer = nullptr;
  VmaAllocation staging_alloc = nullptr;
  result = vmaCreateBuffer(_alloc, &staging_info, &staging_alloc_info, &staging_buffer, &staging_alloc, nullptr);

  VkBufferCreateInfo create_info = { };
  create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  create_info.pNext = nullptr;
  create_info.usage = buffer_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  create_info.size = size;
  create_info.queueFamilyIndexCount = 0;
  create_info.pQueueFamilyIndices = nullptr;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.flags = 0;
  VmaAllocationCreateInfo vbuf_alloc_info = { };
  vbuf_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  result = vmaCreateBuffer(_alloc, &create_info, &vbuf_alloc_info, &buffer->gpu_buffer, &buffer->gpu_alloc, nullptr);
  if (!vk_error_check(result, "create buffer")) {
    return;
  }

  // Copy data into staging buffer.
  unsigned char *vbuf_ptr = nullptr;
  result = vmaMapMemory(_alloc, staging_alloc, (void **)&vbuf_ptr);
  if (!vk_error_check(result, "map buffer")) {
    return;
  }

  memcpy(vbuf_ptr, data, size);
  vmaUnmapMemory(_alloc, staging_alloc);

  // Now queue the data transfer to GPU-local.
  VkBufferCopy region = { };
  region.srcOffset = 0;
  region.dstOffset = 0;
  region.size = size;
  vkCmdCopyBuffer(_current_transfer_command_buffer, staging_buffer,
                  buffer->gpu_buffer, 1, &region);

  enqueue_buffer_deletion(staging_buffer, staging_alloc);
}

void RendererVk::
prepare_vertex_data(VertexData *data) {
  VkVertexData *vkdata = (VkVertexData *)data;
  vkdata->vk_buffers.resize(data->array_buffers.size());
  for (size_t i = 0; i < data->array_buffers.size(); ++i) {
    prepare_buffer(&vkdata->vk_buffers[i], data->array_buffers[i].data(), data->array_buffers[i].size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  }
}

void RendererVk::prepare_index_data(IndexData *data) {
  VkIndexData *vkdata = (VkIndexData *)data;
  prepare_buffer(vkdata, data->buffer.data(), data->buffer.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

// Acquires an index buffer resource from the renderer.
// The user is responsible for releasing the resource back to the renderer.
IndexData *RendererVk::
make_index_data(MaterialEnums::IndexType type, size_t initial_size) {
  VkIndexData *data = new VkIndexData;
  data->type = type;
  data->gpu_buffer = nullptr;
  data->gpu_alloc = nullptr;
  if (initial_size > 0u) {
    data->buffer.resize(initial_size);
  }
  return data;
}

// Acquires a vertex data resource from the renderer.
// The user is responsible for releasing the resource.
VertexData *RendererVk::
make_vertex_data(const VertexFormat &format, size_t initial_size) {
  VkVertexData *data = new VkVertexData;
  data->format = format;
  data->vk_buffers.resize(format.arrays.size());
  memset(data->vk_buffers.data(), 0, sizeof(VkVertexBuffer) * data->vk_buffers.size());
  data->array_buffers.resize(data->vk_buffers.size());
  if (initial_size >= 0u) {
    for (int i = 0; i < data->array_buffers.size(); ++i) {
      data->array_buffers[i].resize(initial_size);
    }
  }
  return data;
}
