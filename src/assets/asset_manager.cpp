#include <phobos/assets/asset_manager.hpp>

#include <functional>

template<typename VkT>
struct underlying_vk_type {
    using type = typename VkT::CType;
};

template<typename T>
using underlying_vk_type_t = typename underlying_vk_type<T>::type;

template<typename VkT>
static uint64_t to_u64(VkT vulkan_handle) {
    return reinterpret_cast<uint64_t>(static_cast<underlying_vk_type_t<VkT>>(vulkan_handle));
}

namespace std {

// TODO: Use proper hash combine: seed ^= hash_value(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
// See also https://stackoverflow.com/questions/8513911/how-to-create-a-good-hash-combine-with-64-bit-output-inspired-by-boosthash-co

template<>
struct hash<ph::Mesh> {
    uint64_t operator()(ph::Mesh const& mesh) const {
        return to_u64(mesh.get_vertices()) ^ to_u64(mesh.get_memory_handle());
    }
};

template<>
struct hash<ph::Texture> {
    uint64_t operator()(ph::Texture const& tex) const {
        return to_u64(tex.handle()) ^ to_u64(tex.memory_handle());
    }  
};

}

namespace ph {

AssetManager::AssetManager(AssetManager&& rhs) {
    meshes = std::move(rhs.meshes);
    textures = std::move(rhs.textures);
}

AssetManager& AssetManager::operator=(AssetManager&& rhs) {
    if (this != &rhs) {
       meshes = std::move(rhs.meshes);
       textures = std::move(rhs.textures);
    }
    return *this;
}

AssetManager::~AssetManager() {
    destroy_all();
}

void AssetManager::destroy_all() {
    for (auto&[id, mesh] : meshes) {
        mesh.destroy();
    }
    
    for (auto&[id, texture] : textures) {
        texture.destroy();
    }
}

Handle<Mesh> AssetManager::add_mesh(Mesh::CreateInfo const& info) {
    Mesh m(info);
    // Calculate ID by hashing the mesh
    uint64_t id = std::hash<Mesh>()(m);
    // Add it to our map
    meshes.emplace(id, std::move(m));
    // Return a handle to this mesh
    return Handle<Mesh> { id };
}

Mesh* AssetManager::get_mesh(Handle<Mesh> handle) {
    return &meshes.at(handle.id);
}

Mesh const* AssetManager::get_mesh(Handle<Mesh> handle) const {
    return &meshes.at(handle.id);
}

Handle<Texture> AssetManager::add_texture(Texture::CreateInfo const& info) {
    Texture t(info);
    // Calculate ID by hashing the texture
    uint64_t id = std::hash<Texture>()(t);
    // Add it to our map
    textures.emplace(id, std::move(t));
    // Return a handle to this mesh
    return Handle<Texture> { id };
}

Texture* AssetManager::get_texture(Handle<Texture> handle) {
    return &textures.at(handle.id);
}

Texture const* AssetManager::get_texture(Handle<Texture> handle) const {
    return &textures.at(handle.id);
}


} // namespace ph