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
    default_textures.color = create_single_color_texture(ctx, 255, 0, 255, vk::Format::eR8G8B8A8Srgb);
    default_textures.specular = create_single_color_texture(ctx, 255, 255, 255, vk::Format::eR8G8B8A8Srgb);
    default_textures.normal = create_single_color_texture(ctx, 0, 255, 0, vk::Format::eR8G8B8A8Unorm);
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

Pipeline Renderer::get_pipeline(std::string_view name, RenderPass& pass) {
    ph::PipelineCreateInfo const* pci = ctx.pipelines.get_named_pipeline(std::string(name));
    STL_ASSERT(pci, "Pipeline not found");
    STL_ASSERT(pass.active, "Cannot get pipeline handle without an active renderpass");
    Pipeline pipeline = create_or_get_pipeline(&ctx, &pass, *pci);
    pipeline.name = name;
    return pipeline;
}

BuiltinUniforms Renderer::get_builtin_uniforms() {
    return per_frame_buffers;
}

DefaultTextures& Renderer::get_default_textures() {
    return default_textures;
}


void Renderer::destroy() {
    default_textures.color.destroy();
    default_textures.specular.destroy();
    default_textures.normal.destroy();
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
} // namespace ph