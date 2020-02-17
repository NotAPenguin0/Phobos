#include <phobos/pipeline/pipeline_layout.hpp>

namespace ph {


void PipelineLayouts::create_layout(vk::Device device, PipelineLayoutID id, vk::PipelineLayoutCreateInfo const& info) {
    vk::DescriptorSetLayout descriptor_set_layout = nullptr;
    if (info.setLayoutCount > 0) {
        descriptor_set_layout = *info.pSetLayouts;
    }

    layouts.emplace(static_cast<uint32_t>(id), PipelineLayout{device.createPipelineLayout(info), descriptor_set_layout});
}

PipelineLayout PipelineLayouts::get_layout(PipelineLayoutID id) { 
    std::optional<PipelineLayout> layout = find_layout(id);
    if (layout == std::nullopt) {
        throw std::out_of_range("Pipeline layout with requested id not found.");
    } 

    return layout.value();
}

std::optional<PipelineLayout> PipelineLayouts::find_layout(PipelineLayoutID id) const {
    auto it = layouts.find(static_cast<uint32_t>(id));
    if (it == layouts.end()) {
        return std::nullopt;
    }

    return it->second;
}

void PipelineLayouts::destroy(vk::Device device, PipelineLayoutID id) {
    std::optional<PipelineLayout> layout = find_layout(id);
    if (layout == std::nullopt) {
        return;
    } else {
        device.destroyPipelineLayout(layout.value().handle);
        if (layout.value().descriptor_set_layout) {
            device.destroyDescriptorSetLayout(layout.value().descriptor_set_layout);
        }
        // Remove the destroyed layout from the map
        layouts.erase(static_cast<uint32_t>(id));
    }
}

void PipelineLayouts::destroy_all(vk::Device device) {
    for (auto [id, layout] : layouts) {
        device.destroyPipelineLayout(layout.handle);
        if (layout.descriptor_set_layout) {
            device.destroyDescriptorSetLayout(layout.descriptor_set_layout);
        }
    }

    layouts.clear();
}

}