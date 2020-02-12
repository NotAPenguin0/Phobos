#ifndef PHOBOS_PIPELINE_HPP_
#define PHOBOS_PIPELINE_HPP_

#include <vulkan/vulkan.hpp>

namespace ph {

// TODO: 'flags' utility to store pipeline tags?

struct Pipeline {
    vk::Pipeline handle;
};

/*
https://discordapp.com/channels/331718482485837825/331913116621340674/676887942123225128

We need one VkPipeline for each "type" of rendering, for exampe transparant objects, unlit objects, skybox, etc
*/

}

#endif