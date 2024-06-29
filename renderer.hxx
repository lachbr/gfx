#ifndef RENDERER_HXX
#define RENDERER_HXX

#ifdef _WIN32
#include "wininclude.hxx"
typedef HWND WindowHandle;
#endif

#include <vector>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#define VK_PROTOTYPES
#include "vulkan/vulkan.hpp"
#include "vma/vk_mem_alloc.h"

#include <vector>

#include "material.hxx"

struct VkBufferBase {
  // This one holds the GPU-local data of the vertex buffer.
  VkBuffer gpu_buffer = nullptr;
  VmaAllocation gpu_alloc = nullptr;
};

struct VkDeletionRequest {
  VkBuffer buffer;
  VmaAllocation alloc;
  VkFence wait_fence;
};

struct VkIndexData : public VkBufferBase, public IndexData { };

struct VkVertexBuffer : public VkBufferBase { };
struct VkVertexData : public VertexData {
  vector<VkVertexBuffer> vk_buffers;
};

class RendererVk {
public:
  VkInstance _instance;
  VkDevice _device;
  VkPhysicalDevice _active_physical_device;
  uint32_t _device_index;
  vector<VkPhysicalDevice> _physical_devices;
  vector<VkPhysicalDeviceProperties> _physical_device_properties;
  vector<VkQueueFamilyProperties> _queue_family_properties;
  int _gfx_queue_family_index;
  int _present_queue_family_index;
  int _transfer_queue_family_index;
  VkQueue _gfx_queue;
  VkQueue _present_queue;
  VkQueue _transfer_queue;

  VkCommandPool _cmd_pool;
  VkCommandPool _transfer_cmd_pool;

  VmaAllocator _alloc;

  // Graphics output objects.
  VkSurfaceKHR _surface;
  VkExtent3D _surface_extents;
  VkSwapchainKHR _swapchain;
  uint32_t _curr_swapchain_image_index;
  vector<VkImage> _swapchain_images;
  vector<VkImageView> _swapchain_image_views;
  VkImage _depth_image;
  VkImageView _depth_image_view;
  VmaAllocation _depth_image_alloc;
  VkFormat _surface_color_format;
  VkFormat _surface_depth_format;

  static constexpr uint32_t _num_frames = 2u;
  uint32_t _frame_cycle_index = 0u;
  VkSemaphore _image_acquired_semaphores[_num_frames];
  VkCommandBuffer _command_buffers[_num_frames];
  VkCommandBuffer _transfer_command_buffers[_num_frames];
  VkFence _draw_fences[_num_frames];
  VkSemaphore _draw_semaphores[_num_frames];
  VkSemaphore _transfer_semaphores[_num_frames];
  VkFence _transfer_fences[_num_frames];
  // Current frame.
  VkCommandBuffer _current_command_buffer;
  VkFence _current_draw_fence;
  VkSemaphore _current_draw_semaphore;
  VkSemaphore _current_image_acquired_semaphore;
  VkSemaphore _current_transfer_semaphore;
  VkCommandBuffer _current_transfer_command_buffer;
  VkFence _current_transfer_fence;

  std::vector<VkDeletionRequest> _deletion_queue;
  std::vector<VkFence> _created_deletion_fences;

public:
  bool initialize(WindowHandle hwnd);

  bool create_device();
  bool create_queues();
  bool create_graphics_output(WindowHandle hwnd);
  bool create_command_buffer();

  void cycle_frame();
  void update_frame_objects();

  bool init_temp();

  bool begin_frame();
  bool end_frame();

  bool begin_prepare();
  bool end_prepare();

  // TODO: Drawing to which framebuffer?
  bool begin_frame_surface();
  // Enqueue present/submit command buffer(s).
  bool end_frame_surface();

  void enqueue_buffer_deletion(VkBuffer buffer, VmaAllocation alloc);

  void process_deletions();

  bool draw(const VertexData *vdata, const IndexData *idata,
            int first_vertex = 0, int num_vertices = -1);
  bool draw_mesh(const Mesh *mesh);

  void prepare_buffer(VkBufferBase *buffer, ubyte *data, size_t size, u32 buffer_usage);
  void prepare_vertex_data(VertexData *data);
  void prepare_index_data(IndexData *data);

  IndexData *make_index_data(MaterialEnums::IndexType type, size_t initial_size = 0u);
  VertexData *make_vertex_data(const VertexFormat &format, size_t initial_size = 0u);

  VkShaderModule make_shader_module(const vector<uint8_t> &code);
};

#endif // RENDERER_HXX
