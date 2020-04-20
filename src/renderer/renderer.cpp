#include <phobos/renderer/renderer.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <phobos/renderer/meta.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/present/present_manager.hpp>

#include <stl/enumerate.hpp>

// for std::byte
#include <cstddef>
#include <numeric>

namespace ph {

static Texture create_single_color_texture(VulkanContext& ctx, uint8_t r, uint8_t g, uint8_t b, vk::Format fmt) {
    Texture::CreateInfo info;
    info.channels = 4;
    info.ctx = &ctx;
    const uint8_t data[] = { r, g, b, 255 };
    info.data = data;
    info.width = 1;
    info.height = 1;
    info.format = fmt;
    return Texture(info);
}

static Mesh create_skybox_mesh(VulkanContext& ctx) {
    static constexpr float vertices[] = {
        -1, -1, -1,   1, 1, -1, 
        1, -1, -1,    1, 1, -1, 
        -1, -1, -1,   -1, 1, -1, 
        -1, -1, 1,    1, -1, 1, 
        1, 1, 1,      1, 1, 1, 
        -1, 1, 1,      -1, -1, 1, 
        -1, 1, -1,    -1, -1, -1, 
        -1, 1, 1,     -1, -1, -1, 
        -1, -1, 1,   -1, 1, 1, 
        1, 1, 1,    1, -1, -1, 
        1, 1, -1,     1, -1, -1, 
        1, 1, 1,      1, -1, 1, 
        -1, -1, -1,  1, -1, -1, 
        1, -1, 1,    1, -1, 1, 
        -1, -1, 1,  -1, -1, -1, 
        -1, 1, -1,     1, 1, 1,
        1, 1, -1,     1, 1, 1, 
        -1, 1, -1,     -1, 1, 1,
    };
    uint32_t indices[36];
    std::iota(indices, indices + 36, 0);
    ph::Mesh::CreateInfo cube_info;
    cube_info.ctx = &ctx;
    cube_info.vertices = vertices;
    cube_info.vertex_count = 36;
    cube_info.vertex_size = 3;
    cube_info.indices = indices;
    cube_info.index_count = 36;
    return ph::Mesh(cube_info);
}

Renderer::Renderer(VulkanContext& context) : ctx(context) {
    default_color = create_single_color_texture(ctx, 255, 0, 255, vk::Format::eR8G8B8A8Srgb);
    default_specular = create_single_color_texture(ctx, 255, 255, 255, vk::Format::eR8G8B8A8Srgb);
    default_normal = create_single_color_texture(ctx, 0, 255, 0, vk::Format::eR8G8B8A8Unorm);

    skybox = create_skybox_mesh(ctx);
} 

void Renderer::render_frame(FrameInfo& info) {
    // Reset buffers struct
    per_frame_buffers = {};

    CommandBuffer cmd_buffer { &info, info.command_buffer };
    cmd_buffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    update_camera_data(cmd_buffer, info.render_graph);
    update_lights(cmd_buffer, info.render_graph);

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
            if (binding.descriptors.empty()) continue;
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

    vk::Viewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = pass.target.get_width();
    viewport.height = pass.target.get_height();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd_buffer.set_viewport(viewport);

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{ 0, 0 };
    scissor.extent = vk::Extent2D{ pass.target.get_width(), pass.target.get_height() };
    cmd_buffer.set_scissor(scissor);

    if (pass.skybox) {
        update_skybox_ubo(cmd_buffer, frame.render_graph);

        Pipeline pipeline = get_pipeline("builtin_skybox", pass);
        PipelineCreateInfo const* pci = ctx.pipelines.get_named_pipeline("builtin_skybox");
        STL_ASSERT(pci, "skybox pipeline not created");
        ShaderInfo const& shader_info = pci->shader_info;

        cmd_buffer.bind_pipeline(pipeline);
        DescriptorSetBinding bindings;
        bindings.add(make_descriptor(shader_info["transform"], per_frame_buffers.skybox_data));
        // TODO: Custom sampler?
        bindings.add(make_descriptor(shader_info["skybox"], pass.skybox->view_handle(), frame.default_sampler));
        vk::DescriptorSet set = get_descriptor(frame, pass, bindings);

        cmd_buffer.bind_descriptor_set(0, set);
        cmd_buffer.bind_vertex_buffer(0, skybox.get_vertices());
        cmd_buffer.bind_index_buffer(skybox.get_indices());
        cmd_buffer.draw_indexed(skybox.get_index_count(), 1, 0, 0, 0);
        frame.draw_calls++;
    }

    Pipeline pipeline = get_pipeline("generic", pass);
    cmd_buffer.bind_pipeline(pipeline);

    update_model_matrices(cmd_buffer, frame.render_graph);

    // Allocate fixed descriptor set and bind it
    vk::DescriptorSet fixed_set = get_fixed_descriptor_set(frame, frame.render_graph);
    cmd_buffer.bind_descriptor_set(0, fixed_set);

    for (auto[idx, draw] : stl::enumerate(pass.draw_commands.begin(), pass.draw_commands.end())) {
        Mesh* mesh = draw.mesh;
        // Bind draw data
        cmd_buffer.bind_vertex_buffer(0, mesh->get_vertices());
        cmd_buffer.bind_index_buffer(mesh->get_indices());

        // update push constant ranges
        stl::uint32_t const transform_index = idx + pass.transforms_offset;
        cmd_buffer.push_constants(vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint32_t), &transform_index);
        // First texture is diffuse, second is specular. See also get_fixed_descriptor_set() where we fill the textures array
        stl::uint32_t const texture_indices[] = { 3 * draw.material_index, 3 * draw.material_index + 1, 3 * draw.material_index + 2 };
        cmd_buffer.push_constants(vk::ShaderStageFlagBits::eFragment, sizeof(uint32_t), sizeof(texture_indices), &texture_indices);
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
    default_color.destroy();
    default_normal.destroy();
    default_specular.destroy();
    skybox.destroy();
}

void Renderer::update_camera_data(CommandBuffer& cmd_buf, RenderGraph const* graph) {
    glm::mat4 pv = graph->projection * graph->view; 

    BufferSlice ubo = cmd_buf.allocate_scratch_ubo(2 * sizeof(glm::mat4) + sizeof(glm::vec4));
    std::byte* data_ptr = ubo.data;
    std::memcpy(data_ptr, &pv[0][0], 16 * sizeof(float));  
    std::memcpy(data_ptr + 16 * sizeof(float), &graph->view[0][0], 16 * sizeof(float));
    std::memcpy(data_ptr + 32 * sizeof(float), &graph->camera_pos.x, sizeof(glm::vec3));
    per_frame_buffers.camera = ubo;
}

void Renderer::update_model_matrices(CommandBuffer& cmd_buf, RenderGraph const* graph) {
    vk::DeviceSize size = 0;
    for (auto const& pass : graph->passes) {
        size += pass.transforms.size() * sizeof(glm::mat4);
    }

    vk::DeviceSize offset = 0;
    BufferSlice buffer = cmd_buf.allocate_scratch_ssbo(size);
    for (auto const& pass : graph->passes) {
        vk::DeviceSize this_size = pass.transforms.size() * sizeof(glm::mat4);
        std::memcpy(buffer.data + offset, pass.transforms.data(), this_size);
        offset += this_size;
    }
    per_frame_buffers.transform_ssbo = buffer;
}

void Renderer::update_lights(CommandBuffer& cmd_buf, RenderGraph const* graph) {
    vk::DeviceSize const size = 
        sizeof(PointLight) * graph->point_lights.size() 
        + sizeof(DirectionalLight) * graph->directional_lights.size()
        + 2 * sizeof(uint32_t);
    per_frame_buffers.lights = cmd_buf.allocate_scratch_ubo(size);
    std::byte* data_ptr = per_frame_buffers.lights.data;
    if (!graph->point_lights.empty()) {
        std::memcpy(data_ptr, &graph->point_lights[0].position.x, sizeof(PointLight) * graph->point_lights.size());
    }
    size_t const dir_light_offset = meta::max_lights_per_type * sizeof(PointLight);
    if (!graph->directional_lights.empty()) {
        std::memcpy(data_ptr + dir_light_offset, &graph->directional_lights[0].direction.x,
            sizeof(DirectionalLight) * graph->directional_lights.size());
    }
    // Update count variables. For this, we first need to calculate the offset into the UBO.
    // Note that we maintain the light structs to match the exact layout in the shader, so this sizeof() is fine.
    size_t const counts_offset = dir_light_offset + meta::max_lights_per_type * sizeof(DirectionalLight);
    uint32_t const counts[] = { (uint32_t)graph->point_lights.size(), (uint32_t)graph->directional_lights.size() };
    // Write data
    std::memcpy(data_ptr + counts_offset, &counts[0], sizeof(counts));
}

void Renderer::update_skybox_ubo(CommandBuffer& cmd_buf, RenderGraph const* graph) {
    per_frame_buffers.skybox_data = cmd_buf.allocate_scratch_ubo(16 * sizeof(float));
    std::byte* data_ptr = per_frame_buffers.skybox_data.data;
    std::memcpy(data_ptr, &graph->projection[0][0], 16 * sizeof(float));
}

vk::DescriptorSet Renderer::get_fixed_descriptor_set(FrameInfo& frame, RenderGraph const* graph) {
    PipelineCreateInfo const* pci = ctx.pipelines.get_named_pipeline("generic");
    STL_ASSERT(pci, "Generic pipeline not created");
    ShaderInfo const& shader_info = pci->shader_info;

    DescriptorSetBinding bindings;
    bindings.add(make_descriptor(shader_info["camera"], per_frame_buffers.camera));
    bindings.add(make_descriptor(shader_info["transforms"], per_frame_buffers.transform_ssbo));
    stl::vector<ImageView> texture_views;
    texture_views.reserve(graph->materials.size() * 4);
    for (auto const& mat : graph->materials) {
        if (mat.diffuse) {
            texture_views.push_back(mat.diffuse->view_handle());
        }
        else {
            texture_views.push_back(default_color.view_handle());
        }

        if (mat.specular) {
            texture_views.push_back(mat.specular->view_handle());
        }
        else {
            texture_views.push_back(default_specular.view_handle());
        }

        if (mat.normal) {
            texture_views.push_back(mat.normal->view_handle());
        }
        else {
            texture_views.push_back(default_normal.view_handle());
        }
    }
    bindings.add(make_descriptor(shader_info["textures"], texture_views, frame.default_sampler));
    bindings.add(make_descriptor(shader_info["lights"], per_frame_buffers.lights));
    
    // We need variable count to use descriptor indexing
    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
    uint32_t counts[1] { meta::max_unbounded_array_size };
    variable_count_info.descriptorSetCount = 1;
    variable_count_info.pDescriptorCounts = counts;

    // use default descriptor pool and internal version of get_descriptor to specify a pipeline manually
    return get_descriptor(frame, pci->layout.set_layout, stl::move(bindings), &variable_count_info);
}

} // namespace ph