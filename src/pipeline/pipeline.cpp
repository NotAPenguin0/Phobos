#include <phobos/pipeline/pipeline.hpp>

namespace ph {


void PipelineManager::create_pipeline(vk::Device device, PipelineID id, vk::GraphicsPipelineCreateInfo const& info) {
    vk::Pipeline pipeline = device.createGraphicsPipeline(nullptr, info);
    pipelines.emplace(static_cast<uint32_t>(id), pipeline);
}

vk::Pipeline PipelineManager::get_pipeline(PipelineID id) { 
    std::optional<vk::Pipeline> pipeline = find_pipeline(id);
    if (pipeline == std::nullopt) {
        throw std::out_of_range("Pipeline with requested id not found.");
    } 

    return pipeline.value();
}

std::optional<vk::Pipeline> PipelineManager::find_pipeline(PipelineID id) const {
    auto it = pipelines.find(static_cast<uint32_t>(id));
    if (it == pipelines.end()) {
        return std::nullopt;
    }

    return it->second;
}

void PipelineManager::destroy(vk::Device device, PipelineID id) {
    std::optional<vk::Pipeline> pipeline = find_pipeline(id);
    if (pipeline == std::nullopt) {
        return;
    } else {
        device.destroyPipeline(pipeline.value());
        // Remove the destroyed layout from the map
        pipelines.erase(static_cast<uint32_t>(id));
    }
}

void PipelineManager::destroy_all(vk::Device device) {
    for (auto [id, pipeline] : pipelines) {
        device.destroyPipeline(pipeline);
    }

    pipelines.clear();
}

}