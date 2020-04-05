#ifndef IMGUI_IMPL_PHOBOS_H_
#define IMGUI_IMPL_PHOBOS_H_

#include <imgui/imgui.h>
#include <phobos/forward.hpp>

// meh
#include <vulkan/vulkan.hpp>

struct ImGui_ImplPhobos_InitInfo {
    ph::VulkanContext* context;
    size_t max_frames_in_flight;
};

void ImGui_ImplPhobos_Init(ImGui_ImplPhobos_InitInfo* init_info);
void ImGui_ImplPhobos_RenderDrawData(ImDrawData* draw_data, ph::FrameInfo* frame, ph::RenderGraph* render_graph, ph::Renderer* renderer);
void ImGui_ImplPhobos_Shutdown();

bool ImGui_ImplPhobos_CreateFontsTexture(vk::CommandBuffer cmd_buf);
void ImGui_ImplPhobos_DestroyFontUploadObjects();

// Thread safe
ImTextureID ImGui_ImplPhobos_AddTexture(vk::ImageView image_view, vk::ImageLayout image_layout);
// Thread safe
void ImGui_ImplPhobos_RemoveTexture(ImTextureID tex_id);

#endif