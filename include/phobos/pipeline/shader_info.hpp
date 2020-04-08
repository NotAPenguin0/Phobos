#ifndef PHOBOS_SHADER_INFO_HPP_
#define PHOBOS_SHADER_INFO_HPP_

#include <phobos/pipeline/pipeline.hpp>

namespace ph {

class ShaderInfo {
public:
    
};

// Provides meta info like descriptor bindings on shaders in the pipeline, and updates members of the pipeline 
// create info to match these shaders (like vertex attributes). This information will only be gathered from shaders specified in the
// PipelineCreateInfo::reflected_shaders field.
ShaderInfo reflect_shaders(ph::PipelineCreateInfo& pci);

}

#endif