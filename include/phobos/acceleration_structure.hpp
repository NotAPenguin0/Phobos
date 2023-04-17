#pragma once

#include <phobos/context.hpp>

#include <vector>

#if PHOBOS_ENABLE_RAY_TRACING

namespace ph {

// A single acceleration structure allocation with bound memory.
// A full acceleration structure is made out of multiple dedicated acceleration strctures.
// An example of a dedicated acceleration structure is a single BLAS entry.
struct DedicatedAccelerationStructure {
	VkAccelerationStructureKHR handle = nullptr;
	ph::RawBuffer buffer{};
};

// A built acceleration structure. 
// This is a resource that needs to be managed and freed on exit.
struct AccelerationStructure {
	// Bottom level acceleration structure consists of a number of dedicated acceleration structures, each
	// representing a mesh.
	std::vector<DedicatedAccelerationStructure> bottom_level;
	// Top level acceleration structure has a single dedicated acceleration structure, containing all instances of meshes.
	DedicatedAccelerationStructure top_level;
	// Buffer holding the instance data for the TLAS.
	ph::RawBuffer instance_buffer;
};


// Describes a mesh that needs to be added to the acceleration structure.
// Requires a buffer slice for the vertex and index data.
// Note that the acceleration structure will only consume position data, so the given vertex format
// describes the format of the position data.
// The stride is the distance between two consecutive vertices.
// This structure assumes triangles are used.
struct AccelerationStructureMesh {
	ph::BufferSlice vertices{};
	VkFormat vertex_format{};
	uint32_t num_vertices = 0;

	ph::BufferSlice indices{};
	VkIndexType index_type{};
	uint32_t num_indices = 0;

	uint32_t stride = 0;

	VkGeometryFlagsKHR flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
};

struct AccelerationStructureInstance {
	// Index of the mesh we are referring to.
	uint32_t mesh_index{};
	// 4x3 row-major transformation matrix.
	float transform[3][4]{};
	// gl_InstanceCustomIndex
	uint32_t custom_id{};
	// Index of the shader hit group for this instance
	uint32_t hit_group_index{};
	// Additional flags
	VkGeometryInstanceFlagsKHR flags{};
	// Culling mask
	uint8_t cull_mask = 0xFF;
};

class AccelerationStructureBuilder {
public:
	static AccelerationStructureBuilder create(ph::Context& ctx);

	// Add a mesh to the acceleration structure. This mesh will be added to the list of meshes in the bottom-level acceleration structure.
	// Returns the index of the mesh (used to create instances of it).
	uint32_t add_mesh(AccelerationStructureMesh const& mesh);

	void add_instance(AccelerationStructureInstance const& instance);

    // Remove all instances in the TLAS builder (but keeps the buffer around, so it can be reused).
    // Doesn't actually change the acceleration structure.
    void clear_instances();

    // Remove all meshes in the BLAS builder.
    // Doesn't actually change the underlying acceleration structure object.
    void clear_meshes();

    // Builds only the BLAS
    void build_blas_only(uint32_t thread_index = 0);

    // Builds only the TLAS using the same BLAS structures.
    void build_tlas_only(uint32_t thread_index = 0);

	// Builds the acceleration structure. If done on a different thread, you must supply the thread index to ensure
	// queue access is properly synchronized.
	AccelerationStructure build(uint32_t thread_index = 0);

    // Gets resulting AS.
    AccelerationStructure get();

    ~AccelerationStructureBuilder();

private:
	ph::Context& ctx;
	std::vector<AccelerationStructureMesh> meshes;
	std::vector<AccelerationStructureInstance> instances;
	
	// Stores the final acceleration structure while it's building
	AccelerationStructure build_result;

	// Fence that can be used throughout the building process.
	VkFence fence{};
	// Query pool containing the compacted BLAS size.
	VkQueryPool compacted_blas_size_qp{};

	explicit AccelerationStructureBuilder(ph::Context& ctx);

	// Creates a single DedicatedAccelerationStructure from a CreateInfo. This means a buffer is created and
	// bound to the acceleration structure
	DedicatedAccelerationStructure create_acceleration_structure(VkAccelerationStructureCreateInfoKHR create_info);

	// Describes the geometry of a single BLAS entry.
	struct BLASEntry {
		VkAccelerationStructureGeometryKHR geometry{};
		VkAccelerationStructureBuildRangeInfoKHR range{};
	};

	// All information needed to build a BLAS entry.
	struct BLASBuildInfo {
		VkAccelerationStructureBuildGeometryInfoKHR geometry{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		VkAccelerationStructureBuildSizesInfoKHR size_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	};

	// Creates the BLAS entry for a single mesh. Does not actually copy any data.
	BLASEntry build_blas_entry(AccelerationStructureMesh const& mesh);
	// Fill sin the VkAccelerationStructureGeometryKHR field for the BLAS build.
	void fill_geometry_info(BLASBuildInfo& info, BLASEntry const& entry);
	// Writes the size info for the BLAS to the info.size_info field. Returns the amount of scratch memory needed
	// to build this BLAS.
	VkDeviceSize calculate_size_info(BLASBuildInfo& info, BLASEntry const& entry);
	// Allocates a BLAS and binds buffer memory to it.
	DedicatedAccelerationStructure create_blas(BLASBuildInfo const& info);
	// Records the commands for building a BLAS to the command buffer.
	void record_blas_build_commands(ph::CommandBuffer& cmd_buf, uint32_t index, 
		BLASBuildInfo const& info, BLASEntry const& entry);
	// Submits BLAS build commands and waits for them. Blocks the calling thread.
	void submit_blas_build(uint32_t thread_index, ph::Queue& queue, std::vector<ph::CommandBuffer>& cmd_bufs);
	// Launches command buffers to do the BLAS building.
	void build_blas(uint32_t thread_index, VkDeviceSize scratch_size, 
		std::vector<BLASBuildInfo>& build_infos, std::vector<BLASEntry> const& entries);
	// Compacts the BLAS.
	void compact_blas(uint32_t thread_index, std::vector<BLASBuildInfo> const& infos);
	// Builds the entire bottom-level acceleration structure.
	void create_bottom_level(uint32_t thread_index);

	struct TLASInstances {
		VkAccelerationStructureGeometryKHR instances{};
		VkAccelerationStructureBuildRangeInfoKHR range{};
	};

	struct TLASBuildInfo {
		VkAccelerationStructureBuildGeometryInfoKHR geometry{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		VkAccelerationStructureBuildSizesInfoKHR size_info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	};

	VkAccelerationStructureInstanceKHR instance_info_to_tlas_instance(AccelerationStructureInstance const& instance);
	// Creates buffer holding the instance data for the TLAS
	ph::RawBuffer create_instance_buffer(uint32_t thread_index, std::vector<VkAccelerationStructureInstanceKHR> const& instance_data);
	// Calculate needed space for TLAS and scratch buffer
	VkDeviceSize calculate_size_info(TLASBuildInfo& info, TLASInstances const& instances);
	// Builds the entire top-level acceleration structure
	void create_top_level(uint32_t thread_index);
};

}

#endif