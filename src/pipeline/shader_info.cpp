#include <phobos/pipeline/shader_info.hpp>
#include <phobos/pipeline/pipeline.hpp>

#include <vector>
#include <fstream>
#include <string_view>

#include <phobos/renderer/meta.hpp>
#include <phobos/util/cache.hpp>
#include <phobos/core/vulkan_context.hpp>

#include <SPIRV-Cross/spirv_cross.hpp>


namespace ph {

ShaderInfo::BindingInfo ShaderInfo::operator[](std::string_view name) const {
    return bindings.at(std::string(name));
}

void ShaderInfo::add_binding(std::string_view name, BindingInfo info) {
    bindings[std::string(name)] = info;
}

static vk::ShaderStageFlags get_shader_stage(spirv_cross::Compiler& refl) {
    auto entry_point_name = refl.get_entry_points_and_stages()[0];
    auto entry_point = refl.get_entry_point(entry_point_name.name, entry_point_name.execution_model);

    switch (entry_point.model) {
        case spv::ExecutionModel::ExecutionModelVertex: return vk::ShaderStageFlagBits::eVertex;
        case spv::ExecutionModel::ExecutionModelFragment: return vk::ShaderStageFlagBits::eFragment;
        case spv::ExecutionModel::ExecutionModelGLCompute: return vk::ShaderStageFlagBits::eCompute;
        default: return vk::ShaderStageFlagBits::eAll;
    }
}

static vk::Format get_vk_format(uint32_t vecsize) {
    switch (vecsize) {
        case 1: return vk::Format::eR32Sfloat;
        case 2: return vk::Format::eR32G32Sfloat;
        case 3: return vk::Format::eR32G32B32Sfloat;
        case 4: return vk::Format::eR32G32B32A32Sfloat;
        default: return vk::Format::eUndefined;
    }
}

static uint32_t get_byte_size(vk::Format fmt) {
    switch (fmt) {
        case vk::Format::eR32Sfloat: return 4;
        case vk::Format::eR32G32Sfloat: return 8;
        case vk::Format::eR32G32B32Sfloat: return 12;
        case vk::Format::eR32G32B32A32Sfloat: return 16;
        default: return 0;
    }
}

static std::unique_ptr<spirv_cross::Compiler> reflect_shader_stage(ShaderModuleCreateInfo& shader) {
    // Precalculate hash since this is a rather expensive process to do multiple times per frame
    if (shader.code_hash == 0) {
        ph::hash_combine(shader.code_hash, shader.code);
    }

    auto refl = std::make_unique<spirv_cross::Compiler>(shader.code);

    STL_ASSERT(refl, "Failed to reflect shader");
    spirv_cross::ShaderResources res = refl->get_shader_resources();
    
    return refl;
}

// This function merges all push constant ranges in a single shader stage into one push constant range.
// We need this because spirv-cross reports one push constant range for each variable, instead of for each block.
static void merge_ranges(stl::vector<vk::PushConstantRange>& out, stl::vector<vk::PushConstantRange> const& in,
    vk::ShaderStageFlags stage) {

    vk::PushConstantRange merged;
    merged.offset = 1000000; // some arbitrarily large value to start with, we'll make this smaller using std::min() later
    merged.stageFlags = stage;
    for (auto& range : in) {
        if (range.stageFlags == stage) {
            merged.offset = std::min(merged.offset, range.offset);
            merged.size += range.size;
        }
    }

    if (merged.size != 0) {
        out.push_back(merged);
    }
}

static stl::vector<vk::PushConstantRange> get_push_constants(stl::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders) {
    stl::vector<vk::PushConstantRange> pc_ranges;
    stl::vector<vk::PushConstantRange> final;
    for (auto& refl : reflected_shaders) {
        spirv_cross::ShaderResources res = refl->get_shader_resources();
        auto const stage = get_shader_stage(*refl);
        for (auto& pc : res.push_constant_buffers) {
            auto ranges = refl->get_active_buffer_ranges(pc.id);
            for (auto& range : ranges) {
                vk::PushConstantRange pc_range;
                pc_range.offset = range.offset;
                pc_range.size = range.range;
                pc_range.stageFlags = stage;
                pc_ranges.push_back(pc_range);
            }
        }
        merge_ranges(final, pc_ranges, stage);
    }
    return final;
}   

static void find_uniform_buffers(ShaderInfo& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
    // TODO: Refer to a UBO in multiple stages
    vk::ShaderStageFlags const stage = get_shader_stage(refl);
    spirv_cross::ShaderResources res = refl.get_shader_resources();
    for (auto& ubo : res.uniform_buffers) {
        vk::DescriptorSetLayoutBinding binding;
        binding.binding = refl.get_decoration(ubo.id, spv::DecorationBinding);
        binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        binding.descriptorCount = 1;
        binding.stageFlags = stage;
        dslci.bindings.push_back(binding);

        info.add_binding(refl.get_name(ubo.id), { binding.binding, binding.descriptorType });
    }
}

static void find_shader_storage_buffers(ShaderInfo& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
    vk::ShaderStageFlags const stage = get_shader_stage(refl);
    spirv_cross::ShaderResources res = refl.get_shader_resources();
    for (auto& ssbo : res.storage_buffers) {
        vk::DescriptorSetLayoutBinding binding;
        binding.binding = refl.get_decoration(ssbo.id, spv::DecorationBinding);
        binding.descriptorType = vk::DescriptorType::eStorageBuffer;
        binding.descriptorCount = 1;
        binding.stageFlags = stage;
        dslci.bindings.push_back(binding);

        info.add_binding(refl.get_name(ssbo.id), { binding.binding, binding.descriptorType });
    }
}

static void find_sampled_images(ShaderInfo& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
    vk::ShaderStageFlags const stage = get_shader_stage(refl);
    spirv_cross::ShaderResources res = refl.get_shader_resources();
    for (auto& si : res.sampled_images) {
        vk::DescriptorSetLayoutBinding binding;
        binding.binding = refl.get_decoration(si.id, spv::DecorationBinding);
        binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        binding.stageFlags = stage;
        auto type = refl.get_type(si.type_id);
        // type.array has the dimensions of the array. If this is zero, we don't have an array.
        // If it's larger than zero, we have an array.
        if (type.array.size() > 0) {
            // Now the dimensions of the array are in the first value of the array field.
            // 0 means unbounded
            if (type.array[0] == 0) { 
                binding.descriptorCount = meta::max_unbounded_array_size; 
                // An unbounded array of samplers means descriptor indexing, we have to set the PartiallyBound and VariableDescriptorCount
                // flags for this binding
                
                // Reserve enough space to hold all flags and this one
                dslci.flags.resize(dslci.bindings.size() + 1);
                dslci.flags.back() = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eVariableDescriptorCount;
            }
            else {
                binding.descriptorCount = type.array[0];
            }
        }
        else {
            // If it' not an array, there is only one descriptor
            binding.descriptorCount = 1;
        }

        info.add_binding(refl.get_name(si.id), { binding.binding, binding.descriptorType });
        dslci.bindings.push_back(binding);
    }
}

static void find_storage_images(ShaderInfo& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
    vk::ShaderStageFlags const stage = get_shader_stage(refl);
    spirv_cross::ShaderResources res = refl.get_shader_resources();

    for (auto& si : res.storage_images) {
        vk::DescriptorSetLayoutBinding binding;
        binding.binding = refl.get_decoration(si.id, spv::DecorationBinding);
        binding.descriptorType = vk::DescriptorType::eStorageImage;
        binding.stageFlags = stage;
        binding.descriptorCount = 1;
        info.add_binding(refl.get_name(si.id), { binding.binding, binding.descriptorType });
        dslci.bindings.push_back(binding);
    }
}

static void collapse_bindings(DescriptorSetLayoutCreateInfo& info) {
    stl::vector<vk::DescriptorSetLayoutBinding> final_bindings;

    for (size_t i = 0; i < info.bindings.size(); ++i) {
        auto binding = info.bindings[i];

        // Before doing anything, check if we already processed this binding
        bool process = true;
        for (auto const& b : final_bindings) { 
            if (binding.binding == b.binding) { 
                process = false; 
                break; 
            }
        }
        if (!process) { continue; }

        vk::ShaderStageFlags stages = binding.stageFlags;
        for (size_t j = i + 1; j < info.bindings.size(); ++j) {
            auto const& other_binding = info.bindings[j];
            if (binding.binding == other_binding.binding) {
                stages |= other_binding.stageFlags;

            }
        }
        binding.stageFlags = stages;
        final_bindings.push_back(binding);
    }

    info.bindings = final_bindings;
}

static DescriptorSetLayoutCreateInfo get_descriptor_set_layout(ShaderInfo& info, stl::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders) {
    DescriptorSetLayoutCreateInfo dslci;
    for (auto& refl : reflected_shaders) {
        find_uniform_buffers(info, *refl, dslci);
        find_shader_storage_buffers(info, *refl, dslci);
        find_sampled_images(info, *refl, dslci);
        find_storage_images(info, *refl, dslci);
    }
    collapse_bindings(dslci);
    return dslci;
}

static PipelineLayoutCreateInfo make_pipeline_layout(stl::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders, ShaderInfo& shader_info) {  
    PipelineLayoutCreateInfo layout;
    layout.push_constants = get_push_constants(reflected_shaders);
    layout.set_layout = get_descriptor_set_layout(shader_info, reflected_shaders);
    return layout;
}

void reflect_shaders(VulkanContext& ctx, PipelineCreateInfo& pci) {
    stl::vector<std::unique_ptr<spirv_cross::Compiler>> reflected_shaders;
    for (auto handle : pci.shaders) {
        ph::ShaderModuleCreateInfo* shader = ctx.shader_module_info_cache.get(handle);
        STL_ASSERT(shader, "Invalid shader");
        reflected_shaders.push_back(reflect_shader_stage(*shader));
    }
    pci.layout = make_pipeline_layout(reflected_shaders, pci.shader_info);
}

void reflect_shaders(VulkanContext& ctx, ComputePipelineCreateInfo& pci) {
    stl::vector<std::unique_ptr<spirv_cross::Compiler>> reflected_shaders;
    ph::ShaderModuleCreateInfo* shader = ctx.shader_module_info_cache.get(pci.shader);
    STL_ASSERT(shader, "Invalid shader");
    reflected_shaders.push_back(reflect_shader_stage(*shader));
    pci.layout = make_pipeline_layout(reflected_shaders, pci.shader_info);
}

}