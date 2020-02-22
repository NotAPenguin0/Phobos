#ifndef PHOBOS_ASSET_MANAGER_HPP_
#define PHOBOS_ASSET_MANAGER_HPP_

#include <unordered_map>

#include <phobos/assets/handle.hpp>

#include <phobos/renderer/mesh.hpp>
#include <phobos/renderer/texture.hpp>

namespace ph {

// All assets are stored with unsigned 64-bit handles (to be consistent with Vulkan handles)
class AssetManager {
public:
    AssetManager() = default;
    AssetManager(AssetManager const&) = delete;
    AssetManager(AssetManager&&);

    AssetManager& operator=(AssetManager const&) = delete;
    AssetManager& operator=(AssetManager&&);

    ~AssetManager();

    void destroy_all();

    Handle<Mesh> add_mesh(Mesh::CreateInfo const& info);
    Mesh* get_mesh(Handle<Mesh> handle);
    Mesh const* get_mesh(Handle<Mesh> handle) const;

    Handle<Texture> add_texture(Texture::CreateInfo const& info);
    Texture* get_texture(Handle<Texture> handle);
    Texture const* get_texture(Handle<Texture> handle) const;

private:
    std::unordered_map<uint64_t, Mesh> meshes;
    std::unordered_map<uint64_t, Texture> textures;
};

} // namespace ph

#endif