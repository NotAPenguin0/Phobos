#include <phobos/util/shader_util.hpp>

#include <fstream>
#include <vector>
#include <string>
#include <atomic>

#include <phobos/core/vulkan_context.hpp>

namespace ph {

std::vector<uint32_t> load_shader_code(std::string_view file_path) {
    std::ifstream file(file_path.data(), std::ios::binary);
    std::string code = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    
    std::vector<uint32_t> spv(code.size() / 4);
    std::memcpy(spv.data(), &code[0], code.size());
    return spv;
}

namespace {

struct ShaderHandleId {
    static inline std::atomic<uint32_t> cur = 0;
    static inline uint32_t next() {
        return cur++;
    }
};

}

ShaderHandle create_shader(VulkanContext& ctx, std::vector<uint32_t> const& code, std::string_view entry, vk::ShaderStageFlagBits stage) {
    uint32_t id = ShaderHandleId::next();

    ph::ShaderModuleCreateInfo info;
    info.code = code;
    ph::hash_combine(info.code_hash, code);
    info.entry_point = entry;
    info.stage = stage;

    ctx.shader_module_info_cache.insert(ShaderHandle{ id }, std::move(info));

    return { id };
}

}