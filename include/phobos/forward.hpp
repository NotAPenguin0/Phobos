#ifndef PHOBOS_FORWARD_HPP_
#define PHOBOS_FORWARD_HPP_

// This file contains forward declarations for all types used in Phobos

struct Mimas_Window;

namespace ph {

struct DeviceRequirements;

struct QueueFamilyIndices;
struct SurfaceDetails;
struct PhysicalDeviceDetails;
struct PhysicalDeviceRequirements;

struct SwapchainDetails;

struct Pipeline;
class PipelineLayouts;
class PipelineManager;

struct WindowContext;

struct VulkanContext;
struct PerThreadContext;
struct AppSettings;

class Mesh;
class Texture;

struct Material;
struct PointLight;

class RenderGraph;
struct FrameInfo;
class Renderer;
class PresentManager;

struct RawBuffer;
struct ImageView;

} // namespace ph


#endif