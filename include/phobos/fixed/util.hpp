#ifndef PHOBOS_FIXED_UTIL_HPP_
#define PHOBOS_FIXED_UTIL_HPP_

#include <phobos/renderer/command_buffer.hpp>
#include <phobos/pipeline/pipeline.hpp>

#include <phobos/present/present_manager.hpp>

#include <string_view>

namespace ph::fixed {

// Sets viewport and scissor region to cover full render target
void auto_viewport_scissor(ph::CommandBuffer& cmd_buf);

}

#endif