#if PHOBOS_ENABLE_RAY_TRACING

#include <phobos/acceleration_structure.hpp>

#include <algorithm>

namespace ph {

#define PH_RTX_CALL(func, ...) ctx.rtx_fun._##func(__VA_ARGS__)

AccelerationStructureBuilder AccelerationStructureBuilder::create(ph::Context& ctx) {
	return AccelerationStructureBuilder{ ctx };
}

AccelerationStructureBuilder::AccelerationStructureBuilder(ph::Context& ctx) : ctx(ctx) {

}

uint32_t AccelerationStructureBuilder::add_mesh(AccelerationStructureMesh const& mesh) {
	meshes.push_back(mesh);
	return meshes.size() - 1;
}

void AccelerationStructureBuilder::add_instance(AccelerationStructureInstance const& instance) {
	instances.push_back(instance);
}

void AccelerationStructureBuilder::clear_instances() {
    instances.clear();
}

void AccelerationStructureBuilder::clear_meshes() {
    meshes.clear();
}

void AccelerationStructureBuilder::build_blas_only(uint32_t thread_index) {
    fence = ctx.create_fence();

    // Delete old BLAS. TODO: Check if this doesn't cause free-during-use errors.
    for (auto& blas : build_result.bottom_level) {
        PH_RTX_CALL(vkDestroyAccelerationStructureKHR, ctx.device(), blas.handle, nullptr);
        ctx.destroy_buffer(blas.buffer);
    }
    build_result.bottom_level.clear();

    create_bottom_level(thread_index);
    ctx.destroy_fence(fence);
}

void AccelerationStructureBuilder::build_tlas_only(uint32_t thread_index) {
    fence = ctx.create_fence();

    // Delete old TLAS. // TODO: Make sure this doesn't cause free-during-use.

    PH_RTX_CALL(vkDestroyAccelerationStructureKHR, ctx.device(), build_result.top_level.handle, nullptr);
    ctx.destroy_buffer(build_result.top_level.buffer);
    // We don't destroy the instance buffer because we might be able to re-use it.

    create_top_level(thread_index);
    ctx.destroy_fence(fence);
}

AccelerationStructure AccelerationStructureBuilder::build(uint32_t thread_index) {
	// Initialize reusable objects.
	fence = ctx.create_fence();
	// Create bottom and top level acceleration structures.
	create_bottom_level(thread_index);
	create_top_level(thread_index);
	// Cleanup.
	ctx.destroy_fence(fence);
	// Return acceleration structure to user.
	return build_result;
}

AccelerationStructure AccelerationStructureBuilder::get() {
    return build_result;
}

AccelerationStructureBuilder::~AccelerationStructureBuilder() {

}

DedicatedAccelerationStructure AccelerationStructureBuilder::create_acceleration_structure(VkAccelerationStructureCreateInfoKHR create_info) {
	DedicatedAccelerationStructure result{};

	result.buffer = ctx.create_buffer(ph::BufferType::AccelerationStructure, create_info.size);
	create_info.buffer = result.buffer.handle;

	PH_RTX_CALL(vkCreateAccelerationStructureKHR, ctx.device(), &create_info, nullptr, &result.handle);
	return result;
}

AccelerationStructureBuilder::BLASEntry AccelerationStructureBuilder::build_blas_entry(AccelerationStructureMesh const& mesh) {
	BLASEntry entry{};
	entry.geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	entry.geometry.pNext = nullptr;
	entry.geometry.flags = mesh.flags;
	entry.geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	entry.geometry.geometry.triangles = VkAccelerationStructureGeometryTrianglesDataKHR{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
		.pNext = nullptr,
		.vertexFormat = mesh.vertex_format,
		.vertexData = ctx.get_device_address(mesh.vertices),
		.vertexStride = mesh.stride,
		.maxVertex = mesh.num_vertices,
		.indexType = mesh.index_type,
		.indexData = ctx.get_device_address(mesh.indices),
		// By leaving transform data for each vertex empty we specify an identity transform
		.transformData = {}
	};
	entry.range = VkAccelerationStructureBuildRangeInfoKHR{
		// Every 3 indices make up a triangle, so there are num_indices / 3 triangles
		.primitiveCount = mesh.num_indices / 3,
		.primitiveOffset = 0,
		.firstVertex = 0,
		.transformOffset = 0
	};
	return entry;
}

void AccelerationStructureBuilder::fill_geometry_info(BLASBuildInfo& info, BLASEntry const& entry) {
	info.geometry = VkAccelerationStructureBuildGeometryInfoKHR{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.pNext = nullptr,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
				| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.srcAccelerationStructure = nullptr,
		.dstAccelerationStructure = nullptr, // Filled later
		.geometryCount = 1,
		.pGeometries = &entry.geometry
	};
}

VkDeviceSize AccelerationStructureBuilder::calculate_size_info(BLASBuildInfo& info, BLASEntry const& entry) {
	// Get acceleration structure size. Note that this is uncompacted, we'll compact this afterwards.
	uint32_t primitive_count = entry.range.primitiveCount;
	PH_RTX_CALL(vkGetAccelerationStructureBuildSizesKHR, ctx.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&info.geometry, &primitive_count, &info.size_info);
	
	return info.size_info.buildScratchSize;
}

DedicatedAccelerationStructure AccelerationStructureBuilder::create_blas(BLASBuildInfo const& info) {
	VkAccelerationStructureCreateInfoKHR asci{};
	asci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	asci.size = info.size_info.accelerationStructureSize;
	asci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	
	return create_acceleration_structure(asci);
}

void AccelerationStructureBuilder::record_blas_build_commands(ph::CommandBuffer& cmd_buf, uint32_t index,
	BLASBuildInfo const& info, BLASEntry const& entry) {

	cmd_buf.build_acceleration_structure(info.geometry, &entry.range);

	// Insert a barrier to make sure build is complete before starting the next one
	VkMemoryBarrier barrier{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
	};
	cmd_buf.barrier(ph::PipelineStage::AccelerationStructureBuild, ph::PipelineStage::AccelerationStructureBuild, barrier);
	// Query the compacted size
	cmd_buf.reset_query_pool(compacted_blas_size_qp, index, 1);
	cmd_buf.write_acceleration_structure_properties(info.geometry.dstAccelerationStructure,
		VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, compacted_blas_size_qp, index);
	cmd_buf.end();
}

void AccelerationStructureBuilder::submit_blas_build(uint32_t thread_index, ph::Queue& queue, std::vector<ph::CommandBuffer>& cmd_bufs) {
	// VkSubmitInfo requires a pointer to a list of command buffers.
	std::vector<VkCommandBuffer> cmd_buf_handles{ cmd_bufs.size() };
	for (uint32_t i = 0; i < cmd_bufs.size(); ++i) { cmd_buf_handles[i] = cmd_bufs[i].handle(); }
	VkSubmitInfo submit{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.pWaitDstStageMask = nullptr,
		.commandBufferCount = (uint32_t)(cmd_bufs.size()),
		.pCommandBuffers = cmd_buf_handles.data(),
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr
	};
	queue.submit(submit, fence);
	// Wait for the fence
	ctx.wait_for_fence(fence);
	ctx.reset_fence(fence);

	for (ph::CommandBuffer& cmd_buf : cmd_bufs) {
		queue.free_single_time(cmd_buf, thread_index);
	}
	cmd_bufs.clear();
}

void AccelerationStructureBuilder::build_blas(uint32_t thread_index, VkDeviceSize scratch_size, 
	std::vector<BLASBuildInfo>& build_infos, std::vector<BLASEntry> const& entries) {

	uint32_t const num_blas = build_infos.size();

	// First we create a scratch buffer.
	ph::RawBuffer scratch_buffer = ctx.create_buffer(BufferType::AccelerationStructureScratch, scratch_size);
	VkDeviceAddress scratch_address = ctx.get_device_address(scratch_buffer);

	compacted_blas_size_qp = ctx.create_query_pool(VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, num_blas);

	// Build acceleration structures
	std::vector<ph::CommandBuffer> cmd_bufs;
	ph::Queue& queue = *ctx.get_queue(ph::QueueType::Compute);
	for (uint32_t i = 0; i < num_blas; ++i) {
		// Create command buffer and store it.
		cmd_bufs.push_back(queue.begin_single_time(thread_index));
		ph::CommandBuffer& cmd_buf = cmd_bufs.back();

		BLASBuildInfo& build = build_infos[i];
		build.geometry.scratchData.deviceAddress = scratch_address;
		record_blas_build_commands(cmd_buf, i, build, entries[i]);
	}

	submit_blas_build(thread_index, queue, cmd_bufs);

	// Free resources
	ctx.destroy_buffer(scratch_buffer);
}

void AccelerationStructureBuilder::compact_blas(uint32_t thread_index, std::vector<BLASBuildInfo> const& infos) {
	uint32_t num_blas = infos.size();
	// Read back the query results.
	std::vector<uint32_t> compacted_sizes;
	compacted_sizes.resize(num_blas);
	vkGetQueryPoolResults(ctx.device(), compacted_blas_size_qp,
		0, num_blas, num_blas * sizeof(uint32_t),
		compacted_sizes.data(), sizeof(uint32_t), VK_QUERY_RESULT_WAIT_BIT);
	// For logging purposes
	uint32_t total_orig_size = 0;
	uint32_t total_compact_size = 0;

	// Old acceleration structures to destroy when done
	std::vector<DedicatedAccelerationStructure> old_as{ num_blas };

	ph::Queue& queue = *ctx.get_queue(ph::QueueType::Compute);
	// We can do compaction with a single command buffer;
	ph::CommandBuffer cmd_buf = queue.begin_single_time(thread_index);
	for (uint32_t i = 0; i < num_blas; ++i) {
		total_orig_size += infos[i].size_info.accelerationStructureSize;
		total_compact_size += compacted_sizes[i];

		ctx.logger()->write_fmt(ph::LogSeverity::Info, "RT BLAS: Reducing size from {} to {} bytes.",
			infos[i].size_info.accelerationStructureSize, compacted_sizes[i]);

		// Create new compacted acceleration structure
		VkAccelerationStructureCreateInfoKHR asci{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
			.pNext = nullptr,
			.size = compacted_sizes[i],
			.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		};
		DedicatedAccelerationStructure compacted = create_acceleration_structure(asci);

		cmd_buf.compact_acceleration_structure(build_result.bottom_level[i].handle, compacted.handle);
		// Save old acceleration structure to destroy later
		old_as[i] = build_result.bottom_level[i];
		// Set final acceleration structure to compacted AS.
		build_result.bottom_level[i] = compacted;
	}
	// Submit compacting commands
	queue.end_single_time(cmd_buf, fence);
	ctx.wait_for_fence(fence);
	ctx.reset_fence(fence);

	ctx.logger()->write_fmt(ph::LogSeverity::Info, "RT BLAS: Total size reduced from {} to {} bytes",
		total_orig_size, total_compact_size);

	for (auto& as : old_as) {
		PH_RTX_CALL(vkDestroyAccelerationStructureKHR, ctx.device(), as.handle, nullptr);
		ctx.destroy_buffer(as.buffer);
	}
	ctx.destroy_query_pool(compacted_blas_size_qp);
}

void AccelerationStructureBuilder::create_bottom_level(uint32_t thread_index) {
	std::vector<BLASEntry> entries{};
	uint32_t num_blas = meshes.size();
	entries.reserve(num_blas);
	for (AccelerationStructureMesh const& mesh : meshes) {
		entries.push_back(build_blas_entry(mesh));
	}

	// We now need to pack this BLASEntry information into a structure ready for Vulkan to build.
	// We need to create one of these for every BLASEntry
	std::vector<BLASBuildInfo> build_infos{ num_blas };
	for (size_t i = 0; i < num_blas; ++i) {
		fill_geometry_info(build_infos[i], entries[i]);
	}

	// We've filled in the geometry information for each BLAS build entry, now we create the
	// acceleration structure handles and buffers. We'll also figure out the amount of scratch memory
	// we need.
	VkDeviceSize max_scratch_needed = 0;
	build_result.bottom_level.resize(num_blas);
	for (size_t i = 0; i < num_blas; ++i) {
		BLASBuildInfo& build_info = build_infos[i];
		BLASEntry const& entry = entries[i];
		// Calculate size info and create BLAS.
		max_scratch_needed = std::max(max_scratch_needed, calculate_size_info(build_info, entry));
		DedicatedAccelerationStructure blas = create_blas(build_info);
		// Now we know the dstAccelerationStructure field for VkAccelerationStructureBuildGeometryInfoKHR.
		build_infos[i].geometry.dstAccelerationStructure = blas.handle;
		// Save created BLAS.
		build_result.bottom_level[i] = blas;
	}

	// We created the acceleration structures, now we'll build them by launching a command buffer for every
	// BLAS we create.
	build_blas(thread_index, max_scratch_needed, build_infos, entries);
	// Compact the final BLAS
	compact_blas(thread_index, build_infos);
}

VkAccelerationStructureInstanceKHR AccelerationStructureBuilder::instance_info_to_tlas_instance(AccelerationStructureInstance const& instance) {
	VkAccelerationStructureInstanceKHR info{};
	
	// Get device address of BLAS
	VkAccelerationStructureDeviceAddressInfoKHR blas_address_info{};
	blas_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	blas_address_info.accelerationStructure = build_result.bottom_level[instance.mesh_index].handle;
	VkDeviceAddress blas_address = 
		PH_RTX_CALL(vkGetAccelerationStructureDeviceAddressKHR, ctx.device(), &blas_address_info);

	info.accelerationStructureReference = blas_address;
	info.instanceCustomIndex = instance.custom_id;
	info.instanceShaderBindingTableRecordOffset = instance.hit_group_index;
	info.mask = instance.cull_mask;
	info.flags = instance.flags;
	std::memcpy(&info.transform, &instance.transform, sizeof(instance.transform));

	return info;
}

ph::RawBuffer AccelerationStructureBuilder::create_instance_buffer(uint32_t thread_index, 
	std::vector<VkAccelerationStructureInstanceKHR> const& instance_data) {
	uint32_t const num_instances = instance_data.size();
	VkDeviceSize const size = sizeof(VkAccelerationStructureInstanceKHR) * num_instances;

	ph::RawBuffer instance_buffer;
    // Only recreate if it's too small
    if (size > build_result.instance_buffer.size) {
        ctx.destroy_buffer(build_result.instance_buffer);
        instance_buffer = ctx.create_buffer(ph::BufferType::AccelerationStructureInstance, size);
    } else {
        instance_buffer = build_result.instance_buffer;
    }
	ph::RawBuffer scratch = ctx.create_buffer(ph::BufferType::TransferBuffer, size);

	std::byte* memory = ctx.map_memory(scratch);
	std::memcpy(memory, instance_data.data(), size);

	ph::Queue& queue = *ctx.get_queue(ph::QueueType::Transfer);
	ph::CommandBuffer cmd = queue.begin_single_time(thread_index);

	cmd.copy_buffer(scratch, instance_buffer.slice(0, size));

	queue.end_single_time(cmd, fence);
	ctx.wait_for_fence(fence);
	ctx.reset_fence(fence);

	ctx.destroy_buffer(scratch);
	queue.free_single_time(cmd, thread_index);

	return instance_buffer;
}

VkDeviceSize AccelerationStructureBuilder::calculate_size_info(TLASBuildInfo& info, TLASInstances const& instances) {
	uint32_t num_instances = this->instances.size();
	PH_RTX_CALL(vkGetAccelerationStructureBuildSizesKHR, 
		ctx.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info.geometry, &num_instances, &info.size_info);
	return info.size_info.buildScratchSize;
}


void AccelerationStructureBuilder::create_top_level(uint32_t thread_index) {
	std::vector<VkAccelerationStructureInstanceKHR> instance_data;
	instance_data.reserve(instances.size());
	for (auto const& instance : instances) {
		instance_data.push_back(instance_info_to_tlas_instance(instance));
	}

	build_result.instance_buffer = create_instance_buffer(thread_index, instance_data);

	TLASInstances tlas_instances{};
	{
		// This structure will point the TLAS to the instance buffer
		VkAccelerationStructureGeometryInstancesDataKHR instances_vk{};
		instances_vk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		instances_vk.arrayOfPointers = false;
		instances_vk.data.deviceAddress = ctx.get_device_address(build_result.instance_buffer);

		VkAccelerationStructureGeometryKHR geometry_data{};
		tlas_instances.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		tlas_instances.instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		tlas_instances.instances.geometry.instances = instances_vk;
		// The amount of primitives is the amount of instances. We can leave all other values at zero.
		tlas_instances.range.primitiveCount = instances.size();
	}

	// Start filling build info
	TLASBuildInfo build_info{};
	build_info.geometry.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	build_info.geometry.srcAccelerationStructure = nullptr;
	build_info.geometry.dstAccelerationStructure = nullptr; // We fill this field later
	build_info.geometry.geometryCount = 1;
	build_info.geometry.pGeometries = &tlas_instances.instances;
	build_info.geometry.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

	// Figure out needed scratch space and TLAS space.
	VkDeviceSize scratch_space = calculate_size_info(build_info, tlas_instances);

	// Create TLAS and bind to memory
	VkAccelerationStructureCreateInfoKHR asci{};
	asci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	asci.size = build_info.size_info.accelerationStructureSize;
	asci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	build_result.top_level = create_acceleration_structure(asci);

	// Allocate the scratch buffer for building the TLAS
	ph::RawBuffer scratch = ctx.create_buffer(ph::BufferType::AccelerationStructureScratch, scratch_space);
	VkDeviceAddress scratch_address = ctx.get_device_address(scratch);

	ph::Queue& queue = *ctx.get_queue(ph::QueueType::Compute);
	ph::CommandBuffer cmd = queue.begin_single_time(thread_index);

	build_info.geometry.dstAccelerationStructure = build_result.top_level.handle;
	build_info.geometry.scratchData.deviceAddress = scratch_address;

	cmd.build_acceleration_structure(build_info.geometry, &tlas_instances.range);

	queue.end_single_time(cmd, fence);
	ctx.wait_for_fence(fence);
	ctx.reset_fence(fence);
	ctx.destroy_buffer(scratch);
	queue.free_single_time(cmd, thread_index);
}

}

#undef PH_RTX_CALL

#endif