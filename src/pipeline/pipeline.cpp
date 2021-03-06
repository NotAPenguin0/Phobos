#include <phobos/pipeline/pipeline.hpp>
#include <phobos/renderer/render_pass.hpp>
#include <phobos/util/memory_util.hpp>

namespace ph {

DescriptorBinding make_image_descriptor(uint32_t binding, ImageView view, vk::Sampler sampler, vk::ImageLayout layout) {
    DescriptorBinding descriptor_binding;
    descriptor_binding.binding = binding;
    // Support for separate image + sampler?
    // #Tag(Sampler)
    descriptor_binding.type = vk::DescriptorType::eCombinedImageSampler;
    DescriptorImageInfo info;
    info.view = view;
    info.layout = layout;
    info.sampler = sampler;
    auto& descriptor = descriptor_binding.descriptors.emplace_back();
    descriptor.image = info;

    return descriptor_binding;
}

DescriptorBinding make_image_descriptor_array(uint32_t binding, stl::span<ImageView> views, vk::Sampler sampler, vk::ImageLayout layout) {
    DescriptorBinding descriptor_binding;
    descriptor_binding.binding = binding;
    descriptor_binding.type = vk::DescriptorType::eCombinedImageSampler;
    descriptor_binding.descriptors.reserve(views.size());
    for (auto img : views) {
        DescriptorImageInfo info;
        info.view = img;
        info.layout = layout;
        info.sampler = sampler;
        auto& descriptor = descriptor_binding.descriptors.emplace_back();
        descriptor.image = info;
    }

    return descriptor_binding;
}

DescriptorBinding make_buffer_descriptor(uint32_t binding, vk::DescriptorType type, vk::Buffer buffer, vk::DeviceSize size, vk::DeviceSize offset) {
    DescriptorBinding descriptor_binding;
    descriptor_binding.binding = binding;
    descriptor_binding.type = type;
    auto& descriptor = descriptor_binding.descriptors.emplace_back();
    descriptor.buffer.buffer = buffer;
    descriptor.buffer.offset = offset;
    descriptor.buffer.range = size;
    return descriptor_binding;
}

DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, ImageView view, vk::Sampler sampler, vk::ImageLayout layout) {
    DescriptorBinding descriptor_binding;
    descriptor_binding.binding = binding.binding;
    descriptor_binding.type = binding.type;
    auto& descriptor = descriptor_binding.descriptors.emplace_back();
    descriptor.image.view = view;
    descriptor.image.layout = layout;
    descriptor.image.sampler = sampler;
    return descriptor_binding;
}

DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, ImageView view, vk::ImageLayout layout) {
    DescriptorBinding descriptor_binding;
    descriptor_binding.binding = binding.binding;
    descriptor_binding.type = binding.type;
    auto& descriptor = descriptor_binding.descriptors.emplace_back();
    descriptor.image.view = view;
    descriptor.image.layout = layout;
    return descriptor_binding;
}

DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, stl::span<ImageView> views, vk::Sampler sampler, vk::ImageLayout layout) {
    DescriptorBinding descriptor_binding;
    descriptor_binding.binding = binding.binding;
    descriptor_binding.type = binding.type;
    descriptor_binding.descriptors.reserve(views.size());
    for (auto img : views) {
        auto& descriptor = descriptor_binding.descriptors.emplace_back();
        descriptor.image.view = img;
        descriptor.image.layout = layout;
        descriptor.image.sampler = sampler;
    }

    return descriptor_binding;
}

DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, vk::Buffer buffer, vk::DeviceSize size, vk::DeviceSize offset) {
    DescriptorBinding descriptor_binding;
    descriptor_binding.binding = binding.binding;
    descriptor_binding.type = binding.type;
    auto& descriptor = descriptor_binding.descriptors.emplace_back();
    descriptor.buffer.buffer = buffer;
    descriptor.buffer.offset = offset;
    descriptor.buffer.range = size;
    return descriptor_binding;
}

DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, RawBuffer& buffer, vk::DeviceSize offset) {
    return make_descriptor(binding, buffer.buffer, buffer.size, offset);
}

DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, BufferSlice slice) {
    return make_descriptor(binding, slice.buffer, slice.range, slice.offset);
}

vk::GraphicsPipelineCreateInfo PipelineCreateInfo::vk_info() const {
    vk::GraphicsPipelineCreateInfo info;

    info.layout = pipeline_layout.layout;
    info.pVertexInputState = &vertex_input;
    info.pInputAssemblyState = &input_assembly;
    info.pViewportState = &viewport_state;
    info.pRasterizationState = &rasterizer;
    info.pMultisampleState = &multisample;
    info.pColorBlendState = &blending;
    info.pDepthStencilState = &depth_stencil;
    info.pDynamicState = &dynamic_state;
    info.renderPass = render_pass;
    info.subpass = subpass;

    return info;
}

void PipelineCreateInfo::finalize() {
    vertex_input.vertexBindingDescriptionCount = vertex_input_bindings.size();
    vertex_input.pVertexBindingDescriptions = vertex_input_bindings.data();
    vertex_input.vertexAttributeDescriptionCount = vertex_attributes.size();
    vertex_input.pVertexAttributeDescriptions = vertex_attributes.data();

    blending.logicOpEnable = blend_logic_op_enable;
    blending.attachmentCount = blend_attachments.size();
    blending.pAttachments = blend_attachments.data();

    dynamic_state.dynamicStateCount = dynamic_states.size();
    dynamic_state.pDynamicStates = dynamic_states.data();

    viewport_state.viewportCount = viewports.size();
    viewport_state.pViewports = viewports.data();
    viewport_state.scissorCount = scissors.size();
    viewport_state.pScissors = scissors.data();
}

void ComputePipelineCreateInfo::finalize() {
    // No work to do 
}

vk::ComputePipelineCreateInfo ComputePipelineCreateInfo::vk_info() const {
    vk::ComputePipelineCreateInfo pci;
    pci.layout = pipeline_layout.layout;
    return pci;
}

static vk::DescriptorSetLayout create_or_get_descriptor_set_layout(VulkanContext* ctx, PerThreadContext* ptc, 
    DescriptorSetLayoutCreateInfo const& info) {
    auto set_layout_opt = ptc->set_layout_cache.get(info);
    if (!set_layout_opt) {
        // We have to create the descriptor set layout here
        vk::DescriptorSetLayoutCreateInfo set_layout_info;
        set_layout_info.bindingCount = info.bindings.size();
        set_layout_info.pBindings = info.bindings.data();
        vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info;
        if (!info.flags.empty()) {
            STL_ASSERT(info.bindings.size() == info.flags.size(), "flag count doesn't match binding count");
            flags_info.bindingCount = info.bindings.size();
            flags_info.pBindingFlags = info.flags.data();
            set_layout_info.pNext = &flags_info;
        }
        vk::DescriptorSetLayout set_layout = ctx->device.createDescriptorSetLayout(set_layout_info); 
        // Store for further use when creating the pipeline layout
        vk::DescriptorSetLayout backup = set_layout;
        ptc->set_layout_cache.insert(info, stl::move(backup));
        return set_layout;
    } else {
        return *set_layout_opt;
    }
}

static PipelineLayout create_or_get_pipeline_layout(VulkanContext* ctx, PerThreadContext* ptc, PipelineLayoutCreateInfo const& info, 
    vk::DescriptorSetLayout set_layout) {
    // Create or get pipeline layout from cache
    auto pipeline_layout_opt = ptc->pipeline_layout_cache.get(info);
    if (!pipeline_layout_opt) {
        // We have to create a new pipeline layout
        vk::PipelineLayoutCreateInfo layout_create_info;
        layout_create_info.pushConstantRangeCount = info.push_constants.size();
        layout_create_info.pPushConstantRanges = info.push_constants.data();
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = &set_layout;

        vk::PipelineLayout vk_layout = ctx->device.createPipelineLayout(layout_create_info);
        PipelineLayout layout;
        layout.layout = vk_layout;
        layout.set_layout = set_layout;

        PipelineLayout backup = layout;
        ptc->pipeline_layout_cache.insert(info, stl::move(backup));
        return layout;
    } else {
        return *pipeline_layout_opt;
    }
}

static vk::ShaderModule create_shader_module(VulkanContext* ctx, ShaderModuleCreateInfo const& info) {
    vk::ShaderModuleCreateInfo vk_info;
    constexpr size_t bytes_per_spv_element = 4;
    vk_info.codeSize = info.code.size() * bytes_per_spv_element;
    vk_info.pCode = info.code.data();
    return ctx->device.createShaderModule(vk_info);
}

Pipeline create_or_get_pipeline(VulkanContext* ctx, PerThreadContext* ptc, RenderPass* pass, PipelineCreateInfo pci) {
    Pipeline pipeline;
   
    // This has to be called before setting up any other pipeline stuff
    pci.finalize();
    pci.render_pass = pass->render_pass;
    // #Tag(Subpass)
    pci.subpass = 0;

    vk::DescriptorSetLayout set_layout = create_or_get_descriptor_set_layout(ctx, ptc, pci.layout.set_layout);
    PipelineLayout pipeline_layout = create_or_get_pipeline_layout(ctx, ptc, pci.layout, set_layout);
    // Make sure to set pipeline layout before lookup
    pci.pipeline_layout = pipeline_layout;
    pipeline.layout = pipeline_layout;
    pipeline.type = PipelineType::Graphics;
        

    auto pipeline_ptr = ptc->pipeline_cache.get(pci);
    if (pipeline_ptr) { pipeline.pipeline = *pipeline_ptr; }
    else {
        vk::GraphicsPipelineCreateInfo vk_pci = pci.vk_info();
        // Create shader modules
        std::vector<vk::PipelineShaderStageCreateInfo> shader_infos;
        for (auto const& handle : pci.shaders) {
            auto shader_info = ctx->shader_module_info_cache.get(handle);
            STL_ASSERT(shader_info, "Invalid shader handle");
            vk::PipelineShaderStageCreateInfo ssci;
            ssci.module = create_shader_module(ctx, *shader_info);
            ssci.pName = shader_info->entry_point.c_str();
            ssci.stage = shader_info->stage;
            shader_infos.push_back(ssci);
        }
        vk_pci.stageCount = shader_infos.size();
        vk_pci.pStages = shader_infos.data();
        vk::Pipeline ppl = ctx->device.createGraphicsPipeline(nullptr, vk_pci);
        // Pipeline was created, destroy shader modules as we don't need them anymore
        for (auto& shader : shader_infos) {
            ctx->device.destroyShaderModule(shader.module);
        }
        if (ctx->has_validation && pci.debug_name != "") {
            vk::DebugUtilsObjectNameInfoEXT name_info;
            name_info.objectType = vk::ObjectType::ePipeline;
            name_info.pObjectName = pci.debug_name.c_str();
            name_info.objectHandle = memory_util::vk_to_u64(ppl);
            ctx->device.setDebugUtilsObjectNameEXT(name_info, ctx->dynamic_dispatcher);
        }
        pipeline.pipeline = ppl;
        ptc->pipeline_cache.insert(stl::move(pci), stl::move(ppl));
    }

    return pipeline;
}

Pipeline create_or_get_compute_pipeline(VulkanContext* ctx, PerThreadContext* ptc, ComputePipelineCreateInfo pci) {
    Pipeline pipeline;

    // This has to be called before setting up any other pipeline stuff
    pci.finalize();

    vk::DescriptorSetLayout set_layout = create_or_get_descriptor_set_layout(ctx, ptc, pci.layout.set_layout);
    PipelineLayout pipeline_layout = create_or_get_pipeline_layout(ctx, ptc, pci.layout, set_layout);
    // Make sure to set pipeline layout before lookup
    pci.pipeline_layout = pipeline_layout;
    pipeline.layout = pipeline_layout;
    pipeline.type = PipelineType::Compute;


    auto pipeline_ptr = ptc->compute_pipeline_cache.get(pci);
    if (pipeline_ptr) { pipeline.pipeline = *pipeline_ptr; }
    else {
        vk::ComputePipelineCreateInfo vk_pci = pci.vk_info();
        // Create shader modules
        vk::PipelineShaderStageCreateInfo ssci;
        auto shader_info = ctx->shader_module_info_cache.get(pci.shader);
        STL_ASSERT(shader_info, "Invalid shader handle");
        ssci.module = create_shader_module(ctx, *shader_info);
        ssci.pName = shader_info->entry_point.c_str();
        ssci.stage = shader_info->stage;
        vk_pci.stage = ssci;
        vk::Pipeline ppl = ctx->device.createComputePipeline(nullptr, vk_pci);
        // Pipeline was created, destroy shader modules as we don't need them anymore
        ctx->device.destroyShaderModule(ssci.module);
        if (ctx->has_validation && pci.debug_name != "") {
            vk::DebugUtilsObjectNameInfoEXT name_info;
            name_info.objectType = vk::ObjectType::ePipeline;
            name_info.pObjectName = pci.debug_name.c_str();
            name_info.objectHandle = memory_util::vk_to_u64(ppl);
            ctx->device.setDebugUtilsObjectNameEXT(name_info, ctx->dynamic_dispatcher);
        }
        pipeline.pipeline = ppl;
        ptc->compute_pipeline_cache.insert(stl::move(pci), stl::move(ppl));
    }

    return pipeline;
}

void PipelineManager::create_named_pipeline(std::string name, PipelineCreateInfo&& info) {
    std::lock_guard lock(mutex);
    if (info.debug_name == "") { info.debug_name = "Pipeline - " + name; }
    pipeline_infos.emplace(stl::move(name), stl::move(info));
}

void PipelineManager::create_named_pipeline(std::string name, ComputePipelineCreateInfo&& info) {
    std::lock_guard lock(compute_mutex);
    if (info.debug_name == "") { info.debug_name = "Compute Pipeline - " + name; }
    compute_pipelines.emplace(stl::move(name), stl::move(info));
}

PipelineCreateInfo const* PipelineManager::get_named_pipeline(std::string const& name) const {
    std::lock_guard lock(mutex);
    auto it = pipeline_infos.find(name);
    if (it != pipeline_infos.end()) return &it->second;
    return nullptr;
}

ComputePipelineCreateInfo const* PipelineManager::get_named_compute_pipeline(std::string const& name) const {
    std::lock_guard lock(compute_mutex);
    auto it = compute_pipelines.find(name);
    if (it != compute_pipelines.end()) return &it->second;
    return nullptr;
}

}