#include <phobos/renderer/render_graph.hpp>

#include <stl/enumerate.hpp>
#include <stl/span.hpp>

#include <phobos/pipeline/pipeline.hpp>
#include <phobos/util/memory_util.hpp>

/*

TODO: 

[X] 1) Create vkRenderPass for every ph::RenderPass
[X] 2) Create ph::RenderTarget for every ph::RenderPass
[X] 3) Cache vkRenderPass (and vkFrameBuffer? )
[X] 4) Integrate imgui pass
[-] 5) Synchronization if necessary

*/

namespace ph {

RenderGraph::RenderGraph(VulkanContext* ctx) : ctx(ctx) {

}


void RenderGraph::add_pass(RenderPass&& pass) {
    passes.push_back(stl::move(pass));
}

void RenderGraph::build() {
    create_render_passes();
}

static bool is_swapchain_attachment(VulkanContext* ctx, RenderAttachment& attachment) {
    for (auto img : ctx->swapchain.images) {
        if (attachment.image_handle() == img) { return true; }
    }
    return false;
}

static bool is_depth_format(vk::Format fmt) {
    switch(fmt) {
        case vk::Format::eD32Sfloat:
        case vk::Format::eD16Unorm:
            return true;
        default:
            return false;
    }
}

static bool is_color_format(vk::Format fmt) {
    switch(fmt) {
        case vk::Format::eB8G8R8A8Unorm:
        case vk::Format::eR8G8B8A8Unorm:
        case vk::Format::eR16G16B16A16Unorm:
        case vk::Format::eR8G8B8A8Srgb:
        case vk::Format::eB8G8R8Snorm:
            return true;
        default:
            return false;
    }
}

static vk::ImageLayout get_output_layout_for_format(vk::Format fmt) {
    if (is_depth_format(fmt)) { return vk::ImageLayout::eDepthStencilAttachmentOptimal; }
    // TODO: Stencil attachment, #Tag(Stencil)

    return vk::ImageLayout::eColorAttachmentOptimal;
}

static vk::ImageLayout get_final_layout(VulkanContext* ctx, stl::span<RenderPass> passes, stl::span<RenderPass>::iterator pass, 
    stl::vector<RenderAttachment>::iterator attachment) {

    // For the final layout, there are 3 (4?) options:
    // 1. The attachment is used later as an input attachment.
    //      In this case, finalLayout becomes eShaderReadOnlyOptimal
    // 2. The attachement is used later to present
    //      In this case, finalLayout becomes ePresentSrcKHR
    // 3. The attachment is not used later
    //      In this case, finalLayout becomes the corresponding format's output attachment layout
    // (4. The attachment is used later as an output attachment.)
    //      In this case, finalLayout becomes the corresponding format's output attachment layout, 
    //      but we have to watch out with the load/store operations. We will implement this later

    // Note that 'later used' in this case only means the NEXT usage of this attachment. Any further usage can be taken care of by
    // further renderpasses

    // First, we create a span of passes following the current renderpass. We check if this pass is the last pass first.
    if (pass == passes.end() - 1) {
        // In this case, the attachments are not used later. We only need to check if this attachment is a swapchain attachment.
        if (is_swapchain_attachment(ctx, *attachment)) {
            // Case 2
            return vk::ImageLayout::ePresentSrcKHR;
        } 

        // Case 3
        return get_output_layout_for_format(attachment->get_format());
    }

    // Now we have verified the current pass isn't the final renderpass, we can continue.
    stl::span<RenderPass> next_passes = stl::span(pass + 1, passes.end());

    // For each following pass ...
    for (auto const& later_pass : next_passes) {
        // ... we check if this pass contains the current attachment as an input attachment
        
        // Custom comparator to compare RenderAttachments by their vkImage handle
        auto find_func = [attachment](RenderAttachment const& candidate) { 
            return attachment->image_handle() == candidate.image_handle(); 
        };

        auto found_it = std::find_if(later_pass.sampled_attachments.begin(), later_pass.sampled_attachments.end(), find_func);

        if (found_it != later_pass.sampled_attachments.end()) {
            // The attachment is used as an input attachment in a later pass. This is case 1
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        }
    }

    // If we haven't encountered a single return in the previous loop, the attachment is not used anywhere else.
    // Case 3
    return get_output_layout_for_format(attachment->get_format());
}

static stl::vector<vk::AttachmentDescription> get_attachment_descriptions(VulkanContext* ctx, stl::span<RenderPass> passes, 
    stl::span<RenderPass>::iterator pass) {

    stl::vector<vk::AttachmentDescription> attachments;
    for (auto& attachment : pass->outputs) {
        vk::AttachmentDescription description;
        description.format = attachment.get_format();
        // #Tag(Multisample)
        description.samples = vk::SampleCountFlagBits::e1;
        // Attachments are for output, so we generally want to clear and store.
        description.loadOp = vk::AttachmentLoadOp::eClear;
        description.storeOp = vk::AttachmentStoreOp::eStore;
        // We don't care about stencil attachments right now.
        // #Tag(Stencil)
        description.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        description.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

        // Now we have to find the correct attachment layouts to use.
        description.initialLayout = vk::ImageLayout::eUndefined;
        description.finalLayout = get_final_layout(ctx, passes, pass, &attachment);

        attachments.push_back(stl::move(description));
    }
    return attachments;
}

static stl::vector<vk::AttachmentReference> get_color_references(stl::span<vk::AttachmentDescription> attachments) { 
    stl::vector<vk::AttachmentReference> references;
    for (auto[index, attachment] : stl::enumerate(attachments.begin(), attachments.end())) {
        if (is_color_format(attachment.format)) {
            vk::AttachmentReference reference;
            reference.attachment = index;
            // Since this only creates color references, we can specify eColorAttachmentOptimal
            reference.layout = vk::ImageLayout::eColorAttachmentOptimal;
            references.push_back(reference);
        } 
    }
    return references;
}

static std::optional<vk::AttachmentReference> get_depth_reference(stl::span<vk::AttachmentDescription> attachments) {
    for (auto[index, attachment] : stl::enumerate(attachments.begin(), attachments.end())) {
        if (is_depth_format(attachment.format)) {
            vk::AttachmentReference reference;
            reference.attachment = index;
            reference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            return reference;
        }
    }
    return std::nullopt;
}


void RenderGraph::create_render_passes() {
    stl::size_t current_transform_offset = 0;
    for (auto it = passes.begin(); it != passes.end(); ++it) {
        auto attachments = get_attachment_descriptions(ctx, passes, it);
        auto color_refs = get_color_references(attachments);
        // Note that get_depth_reference returns an optional, since depth is optional
        auto depth_ref_opt = get_depth_reference(attachments);

        // Create the subpass
        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = color_refs.size();
        subpass.pColorAttachments = color_refs.data();

        if (depth_ref_opt) {
            subpass.pDepthStencilAttachment = &depth_ref_opt.value();
        }

        // All color attachemnts depend on their previous usage
        vk::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

        vk::RenderPassCreateInfo info;
        info.attachmentCount = attachments.size();
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        // Now we have all the info to look up this renderpass in the renderpass cache
        auto pass = ctx->renderpass_cache.get(info);
        if (pass) { it->render_pass = *pass; }
        else { 
            // Create the renderpass and insert it into the caches
            vk::RenderPass render_pass = ctx->device.createRenderPass(info);
            if (ctx->has_validation && it->debug_name != "") {
                vk::DebugUtilsObjectNameInfoEXT name_info;
                name_info.pObjectName = it->debug_name.c_str();
                name_info.objectHandle = memory_util::vk_to_u64(render_pass);
                name_info.objectType = vk::ObjectType::eRenderPass;
                ctx->device.setDebugUtilsObjectNameEXT(name_info, ctx->dynamic_dispatcher);
            }
            it->render_pass = render_pass;
            ctx->renderpass_cache.insert(info, stl::move(render_pass));
        }

        // Next we have to create the vkFramebuffer for this renderpass. This is abstracted away in the RenderTarget class
        it->target = RenderTarget(ctx, it->render_pass, it->outputs);
    }
}

}