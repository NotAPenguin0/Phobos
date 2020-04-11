#include <phobos/renderer/renderer.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <phobos/renderer/meta.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/present/present_manager.hpp>

#include <stl/enumerate.hpp>

// for std::byte
#include <cstddef>

namespace ph {

Renderer::Renderer(VulkanContext& context) : ctx(context) {
    ctx.event_dispatcher.add_listener(this);
} 

void Renderer::render_frame(FrameInfo& info) {
    CommandBuffer cmd_buffer { info.command_buffer };
    cmd_buffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    update_camera_data(info, info.render_graph);
    update_lights(info, info.render_graph);

    for (auto& pass : info.render_graph->passes) {
        cmd_buffer.begin_renderpass(pass);
        pass.callback(cmd_buffer);
        cmd_buffer.end_renderpass();
    }

    cmd_buffer.end();
}

vk::DescriptorSet Renderer::get_descriptor(FrameInfo& frame, RenderPass& pass, DescriptorSetBinding set_binding, void* pNext) {
    STL_ASSERT(pass.active, "get_descriptor called on inactive renderpass");
    auto const& set_layout_info = ctx.pipelines.get_named_pipeline(pass.active_pipeline.name)->layout.set_layout;
    return get_descriptor(frame, set_layout_info, stl::move(set_binding), pNext);
}


vk::DescriptorSet Renderer::get_descriptor(FrameInfo& frame, DescriptorSetLayoutCreateInfo const& set_layout_info, 
    DescriptorSetBinding set_binding, void* pNext) {
        
    set_binding.set_layout = set_layout_info;
    auto set_opt = frame.descriptor_cache.get(set_binding);
    if (!set_opt) {
        // Create descriptor set layout and issue writes
        vk::DescriptorSetAllocateInfo alloc_info;
        // Get set layout. We can assume that it has already been created. If not, something went very wrong
        vk::DescriptorSetLayout* set_layout = ctx.set_layout_cache.get(set_binding.set_layout);
        STL_ASSERT(set_layout, "Descriptor requested without creating desctiptor set layout first. This can happen if there is"
            " no active pipeline bound, or get_descriptor was called outside a valid ph::RenderPass callback.");
        alloc_info.pSetLayouts = set_layout;
        // add default descriptor pool if no custom one was specified
        if (!set_binding.pool) { set_binding.pool = frame.descriptor_pool; };
        alloc_info.descriptorPool = set_binding.pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pNext = pNext;

        // allocateDescriptorSets returns an array of descriptor sets, but we only allocate one, so we need to index it with 0
        vk::DescriptorSet set = ctx.device.allocateDescriptorSets(alloc_info)[0];

        // Now we have the set we need to write the requested data to it
        // TODO: Look into vkUpdateDescriptorSetsWithTemplate?
        stl::vector<vk::WriteDescriptorSet> writes;
        struct DescriptorWriteInfo {
            stl::vector<vk::DescriptorBufferInfo> buffer_infos;
            stl::vector<vk::DescriptorImageInfo> image_infos;
        };
        stl::vector<DescriptorWriteInfo> write_infos;
        writes.reserve(set_binding.bindings.size());
        for (auto const& binding : set_binding.bindings) {
            DescriptorWriteInfo write_info;
            vk::WriteDescriptorSet write;
            write.dstSet = set;
            write.dstBinding = binding.binding;
            write.descriptorType = binding.type;
            write.descriptorCount = binding.descriptors.size();
            switch(binding.type) {
                case vk::DescriptorType::eCombinedImageSampler:
                case vk::DescriptorType::eSampledImage: {
                    write_info.image_infos.reserve(binding.descriptors.size());
                    for (auto const& descriptor : binding.descriptors) {
                        vk::DescriptorImageInfo img;
                        img.imageLayout = descriptor.image.layout;
                        img.imageView = descriptor.image.view.view;
                        img.sampler = descriptor.image.sampler;
                        write_info.image_infos.push_back(img);
                        // Push dummy buffer info to make sure indices match
                        write_info.buffer_infos.emplace_back();
                    }
                } break;
                default: {
                    vk::DescriptorBufferInfo buf;
                    auto& info = binding.descriptors.front().buffer;
                    buf.buffer = info.buffer;
                    buf.offset = info.offset;
                    buf.range = info.range;

                    write_info.buffer_infos.push_back(buf);
                    // Push dummy image info to make sure indices match
                    write_info.image_infos.emplace_back();
                } break;
            }
            write_infos.push_back(write_info);
            writes.push_back(stl::move(write));
        }

        for (size_t i = 0; i < write_infos.size(); ++i) {
            switch (writes[i].descriptorType) {
            case vk::DescriptorType::eSampledImage:
            case vk::DescriptorType::eCombinedImageSampler: {
                writes[i].pImageInfo = write_infos[i].image_infos.data();
            } break;
            default: {
                writes[i].pBufferInfo = write_infos[i].buffer_infos.data();
            } break;
            }
        }

        ctx.device.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);

        // Insert into cache and return the set to the user
        vk::DescriptorSet backup = set;
        frame.descriptor_cache.insert(set_binding, stl::move(backup));
        return set;
    } else {
        return *set_opt;
    }
}

void Renderer::execute_draw_commands(FrameInfo& frame, CommandBuffer& cmd_buffer) {
    ph::RenderPass* pass_ptr = cmd_buffer.get_active_renderpass();
    STL_ASSERT(pass_ptr, "execute_draw_commands called outside of an active renderpass");
    ph::RenderPass& pass = *pass_ptr;
    Pipeline pipeline = get_pipeline("generic", pass);
    cmd_buffer.bind_pipeline(pipeline);

    // Allocate fixed descriptor set and bind it
    vk::DescriptorSet fixed_set = get_fixed_descriptor_set(frame, frame.render_graph);
    update_model_matrices(frame, frame.render_graph, fixed_set);
    cmd_buffer.bind_descriptor_set(0, fixed_set);

    vk::Viewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = pass.target.get_width();
    viewport.height = pass.target.get_height();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd_buffer.set_viewport(viewport);

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = vk::Extent2D{ pass.target.get_width(), pass.target.get_height() };
    cmd_buffer.set_scissor(scissor);

    for (auto[idx, draw] : stl::enumerate(pass.draw_commands.begin(), pass.draw_commands.end())) {
        Mesh* mesh = draw.mesh;
        // Bind draw data
        cmd_buffer.bind_vertex_buffer(0, mesh->get_vertices());
        cmd_buffer.bind_index_buffer(mesh->get_indices());

        // update push constant ranges
        stl::uint32_t const transform_index = idx + pass.transforms_offset;
        cmd_buffer.push_constants(vk::ShaderStageFlagBits::eVertex, sizeof(uint32_t), sizeof(uint32_t), &transform_index);
        cmd_buffer.push_constants(vk::ShaderStageFlagBits::eFragment, 0, sizeof(uint32_t), &draw.material_index);
        // Execute drawcall
        cmd_buffer.draw_indexed(mesh->get_index_count(), 1, 0, 0, 0);
        frame.draw_calls++;
    }
}

Pipeline Renderer::get_pipeline(std::string_view name, RenderPass& pass) {
    ph::PipelineCreateInfo const* pci = ctx.pipelines.get_named_pipeline(std::string(name));
    STL_ASSERT(pci, "Pipeline not found");
    STL_ASSERT(pass.active, "Cannot get pipeline handle without an active renderpass");
    Pipeline pipeline = create_or_get_pipeline(&ctx, &pass, *pci);
    pipeline.name = name;
    return pipeline;
}

void Renderer::destroy() {
    ctx.event_dispatcher.remove_listener(this);
}

// this should not be in the renderer
void Renderer::on_event(DynamicGpuBufferResizeEvent const& e) {
    vk::DescriptorBufferInfo buffer;
    buffer.buffer = e.buffer_handle;
    buffer.offset = 0;
    buffer.range = e.new_size;
    vk::WriteDescriptorSet write;
    write.pBufferInfo = &buffer;
    // TODO: This is wrong
    write.dstBinding = 1;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eStorageBuffer;
    write.dstSet = e.descriptor_set;
    write.dstArrayElement = 0;
    ctx.device.updateDescriptorSets(write, nullptr);
}

void Renderer::update_camera_data(FrameInfo& info, RenderGraph const* graph) {
    glm::mat4 pv = graph->projection * graph->view;
    // Note that map_memory is a cheap operation here since vp_ubo.type is MappedUniformBuffer. In this case, map_memory will
    // immediately return the already stored pointer
    std::byte* data_ptr = map_memory(ctx, info.vp_ubo);
    std::memcpy(data_ptr, &pv[0][0], 16 * sizeof(float));  
    std::memcpy(data_ptr + 16 * sizeof(float), &graph->camera_pos.x, sizeof(glm::vec3));
}

void Renderer::update_model_matrices(FrameInfo& info, RenderGraph const* graph, vk::DescriptorSet descriptor_set) {
    for (auto const& pass : graph->passes) {
        // Don't write when there are no transforms to avoid out of bounds indexing
        if (!pass.transforms.empty()) {
            constexpr stl::size_t sz = sizeof(pass.transforms[0]);
            info.transform_ssbo.write_data(descriptor_set, &pass.transforms[0], 
                pass.transforms.size() * sz, pass.transforms_offset * sz);
        }
    }
}

void Renderer::update_lights(FrameInfo& info, RenderGraph const* graph) {
    if (graph->point_lights.empty()) return;
    // Update point lights

    // Note that map_memory is a cheap operation here since lights.type is MappedUniformBuffer. In this case, map_memory will
    // immediately return the already stored pointer. 
    std::byte* data_ptr = map_memory(ctx, info.lights);
    std::memcpy(data_ptr, &graph->point_lights[0].position.x, sizeof(PointLight) * graph->point_lights.size());
    // Update point light count. For this, we first need to calculate the offset into the UBO
    size_t const counts_offset = meta::max_lights_per_type * sizeof(PointLight);
    uint32_t const point_light_count = graph->point_lights.size();
    // Write data
    std::memcpy(data_ptr + counts_offset, &point_light_count, sizeof(uint32_t));
}

vk::DescriptorSet Renderer::get_fixed_descriptor_set(FrameInfo& frame, RenderGraph const* graph) {
    PipelineCreateInfo const* pci = ctx.pipelines.get_named_pipeline("generic");
    STL_ASSERT(pci, "Generic pipeline not created");
    ShaderInfo const& shader_info = pci->shader_info;

    DescriptorSetBinding bindings;
    bindings.add(make_descriptor(shader_info["camera"], frame.vp_ubo));
    bindings.add(make_descriptor(shader_info["transforms"], frame.transform_ssbo.buffer_handle(), frame.transform_ssbo.size()));
    stl::vector<ImageView> texture_views;
    texture_views.reserve(graph->materials.size());
    for (auto const& mat : graph->materials) { texture_views.push_back(mat.texture->view_handle()); }
    bindings.add(make_descriptor(shader_info["textures"], texture_views, frame.default_sampler));
    bindings.add(make_descriptor(shader_info["lights"], frame.lights));
    
    // We need variable count to use descriptor indexing
    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
    uint32_t counts[1] { meta::max_unbounded_array_size };
    variable_count_info.descriptorSetCount = 1;
    variable_count_info.pDescriptorCounts = counts;

    // use default descriptor pool and internal version of get_descriptor to specify a pipeline manually
    return get_descriptor(frame, pci->layout.set_layout, stl::move(bindings), &variable_count_info);
}

} // namespace ph