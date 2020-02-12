#include <phobos/pipeline/pipeline_layout.hpp>

namespace ph {


void PipelineLayouts::create_layout(vk::Device device, PipelineLayoutID id, vk::PipelineLayoutCreateInfo const& info) {
    layouts.emplace(static_cast<uint32_t>(id), device.createPipelineLayout(info));
}

vk::PipelineLayout PipelineLayouts::get_layout(PipelineLayoutID id) { 
    std::optional<vk::PipelineLayout> layout = find_layout(id);
    if (layout == std::nullopt) {
        throw std::out_of_range("Pipeline layout with requested id not found.");
    } 

    return layout.value();
}

std::optional<vk::PipelineLayout> PipelineLayouts::find_layout(PipelineLayoutID id) const {
    auto it = layouts.find(static_cast<uint32_t>(id));
    if (it == layouts.end()) {
        return std::nullopt;
    }

    return it->second;
}

void PipelineLayouts::destroy(vk::Device device, PipelineLayoutID id) {
    std::optional<vk::PipelineLayout> layout = find_layout(id);
    if (layout == std::nullopt) {
        return;
    } else {
        device.destroyPipelineLayout(layout.value());
        // Remove the destroyed layout from the map
        layouts.erase(static_cast<uint32_t>(id));
    }
}

void PipelineLayouts::destroy_all(vk::Device device) {
    for (auto [id, layout] : layouts) {
        device.destroyPipelineLayout(layout);
    }

    layouts.clear();
}

}