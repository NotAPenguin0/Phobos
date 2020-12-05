#include <phobos/render_graph.hpp>
#include <cassert>

namespace ph {

static bool is_output_attachment(ResourceUsage const& usage) {
    return usage.type == ResourceType::Attachment && usage.access == ResourceAccess::AttachmentOutput && usage.stage == PipelineStage::AttachmentOutput;
}

void RenderGraph::add_pass(Pass pass) {
	passes.push_back(std::move(pass));
}

void RenderGraph::build(Context& ctx) {
	for (auto it = passes.begin(); it != passes.end(); ++it) {
        Pass* pass = &*it;
        auto attachments = get_attachment_descriptions(ctx, pass);
       
        // Create attachment references
        std::vector<VkAttachmentReference> color_attachments{};
        std::optional<VkAttachmentReference> depth_ref{};
        size_t attachment_index = 0;
        for (ResourceUsage const& resource : pass->resources) {
            // Skip non-attachment resources
            if (!is_output_attachment(resource)) continue;

            Attachment* attachment = ctx.get_attachment(resource.attachment.name);
            VkAttachmentReference ref{};
            ref.attachment = attachment_index;
            if (is_depth_format(attachment->view.format)) {
                ref.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                depth_ref = ref;
            }
            else {
                // If an attachment is not a depth attachment, it's a color attachment
                ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                color_attachments.push_back(ref);
            }

            ++attachment_index;
        }

        BuiltPass& build = built_passes.emplace_back();
        // If there are no attachments, we don't need to create a VkRenderPass
        if (color_attachments.empty() && depth_ref == std::nullopt) {
            continue;
        }

        // Create subpass description
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachments.size();
        subpass.pColorAttachments = color_attachments.data();
        if (depth_ref != std::nullopt) {
            subpass.pDepthStencilAttachment = &*depth_ref;
        }
        
        std::vector<VkSubpassDependency> dependencies{};
        for (ResourceUsage const& resource : pass->resources) {
            if (resource.type != ResourceType::Attachment) continue;
            Attachment* attachment = ctx.get_attachment(resource.attachment.name);

            AttachmentUsage previous_usage = find_previous_usage(ctx, pass, attachment);
            if (previous_usage.pass == nullptr) { // Not used earlier, no dependency needed
                continue;
            }

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            if (is_depth_format(attachment->view.format)) {
                dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }
            else {
                dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = previous_usage.access;
            dependency.srcStageMask = previous_usage.stage;
            dependencies.push_back(dependency);
        }

        // Now that we have the dependencies sorted we need to create the VkRenderPass
        VkRenderPassCreateInfo rpci{};
        rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = attachments.size();
        rpci.pAttachments = attachments.data();
        rpci.dependencyCount = dependencies.size();
        rpci.pDependencies = dependencies.data();
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass;
        rpci.pNext = nullptr;

        build.handle = ctx.get_or_create_renderpass(rpci);

        // Step 2: Create VkFramebuffer for this renderpass
        VkFramebufferCreateInfo fbci{};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        std::vector<VkImageView> attachment_views{};
        for (ResourceUsage const& resource : pass->resources) {
            if (!is_output_attachment(resource)) { continue; }
            attachment_views.push_back(ctx.get_attachment(resource.attachment.name)->view.handle);
        }
        fbci.attachmentCount = attachments.size();
        fbci.pAttachments = attachment_views.data();
        fbci.renderPass = build.handle;
        fbci.layers = 1;
        // Figure out width and height of the framebuffer. For this we need to find the largest size of all attachments.
        for (ResourceUsage const& resource : pass->resources) {
            if (!is_output_attachment(resource)) { continue; }
            Attachment* attachment = ctx.get_attachment(resource.attachment.name);
            fbci.width = std::max(attachment->view.size.width, fbci.width);
            fbci.height = std::max(attachment->view.size.height, fbci.height);
        }
        build.render_area = VkExtent2D{ .width = fbci.width, .height = fbci.height };
        build.framebuf = ctx.get_or_create_framebuffer(fbci);
	}
}

std::vector<VkAttachmentDescription> RenderGraph::get_attachment_descriptions(Context& ctx, Pass* pass) {
	std::vector<VkAttachmentDescription> attachments;
	for (ResourceUsage const& resource : pass->resources) {
        if (!is_output_attachment(resource)) { continue; }
		VkAttachmentDescription description{};
		Attachment* attachment = ctx.get_attachment(resource.attachment.name);
		assert(attachment && "Invalid attachment name");

		description.format = attachment->view.format;
		description.samples = attachment->view.samples;
		description.loadOp = static_cast<VkAttachmentLoadOp>(resource.attachment.load_op);
		description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// Stencil operations are currently not supported
		description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        description.initialLayout = get_initial_layout(ctx, pass, resource);
        description.finalLayout = get_final_layout(ctx, pass, resource);
        
		attachments.push_back(description);
	}
	return attachments;
}

static VkImageLayout get_output_layout_for_format(VkFormat format) {
    if (is_depth_format(format)) { return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; }

    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

VkImageLayout RenderGraph::get_initial_layout(Context& ctx, Pass* pass, ResourceUsage const& resource) {
    // We need to find the most recent usage of this attachment as an output attachment and set the initial layout accordingly
  
    if (resource.attachment.load_op == LoadOp::DontCare) return VK_IMAGE_LAYOUT_UNDEFINED;

    AttachmentUsage previous_usage = find_previous_usage(ctx, pass, ctx.get_attachment(resource.attachment.name));
    // Not used previously
    if (previous_usage.pass == nullptr) {
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    // Used as color output attachment previously
    if (previous_usage.access == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) {
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Used as depth output attachment previously
    if (previous_usage.access == VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) {
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    }

    if (previous_usage.access == VK_ACCESS_SHADER_READ_BIT) {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

VkImageLayout RenderGraph::get_final_layout(Context& ctx, Pass* pass, ResourceUsage const& resource) {
    // For the final layout, there are 3 options:
    // 1. The attachment is used later as an input attachment.
    //      In this case, finalLayout becomes eShaderReadOnlyOptimal
    // 2. The attachement is used later to present
    //      In this case, finalLayout becomes ePresentSrcKHR
    // 3. The attachment is not used later
    //      In this case, finalLayout becomes the corresponding format's output attachment layout

    // Note that 'later used' in this case only means the NEXT usage of this attachment. Any further usage can be taken care of by
    // further renderpasses

    Attachment* att = ctx.get_attachment(resource.attachment.name);
    AttachmentUsage next_usage = find_next_usage(ctx, pass, att);

    if (next_usage.pass == nullptr) {
        // In this case, the attachments are not used later. We only need to check if this attachment is a swapchain attachment (in which case its used to present)
        if (ctx.is_swapchain_attachment(resource.attachment.name)) {
            // Case 2
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        // Case 3
        return get_output_layout_for_format(att->view.format);
    }

    if (next_usage.access == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) {
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (next_usage.access == VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) {
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    }

    if (next_usage.access == VK_ACCESS_SHADER_READ_BIT) {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

RenderGraph::AttachmentUsage RenderGraph::find_previous_usage(Context& ctx, Pass* current_pass, Attachment* attachment) {
    // First pass isn't used earlier
    if (current_pass == &passes.front()) {
        return AttachmentUsage{};
    }

    // Go over each earlier pass
    for (Pass* pass = current_pass - 1; pass != &passes.front(); --pass) {

        // Look in this pass's resources

        auto find_resource = [&ctx, attachment](ph::ResourceUsage const& resource) {
            if (resource.type != ResourceType::Attachment) return false;
            return attachment->view.id == ctx.get_attachment(resource.attachment.name)->view.id;
        };

        auto usage = std::find_if(pass->resources.begin(), pass->resources.end(), find_resource);
        if (usage != pass->resources.end()) {
            // Sampled in a previous pass
            if (usage->access == ResourceAccess::ShaderRead) {
                return AttachmentUsage{
                    .stage = static_cast<VkPipelineStageFlags>(usage->stage),
                    .access = static_cast<VkAccessFlags>(usage->access),
                    .pass = pass
                };
            }
            // Used as output attachment in previous pass
            if (usage->access == ResourceAccess::AttachmentOutput) {
                VkAccessFlags access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                if (is_depth_format(attachment->view.format)) {
                    access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
                return AttachmentUsage{
                    .stage = static_cast<VkPipelineStageFlags>(usage->stage),
                    .access = access,
                    .pass = pass
                };
            }
        }
    }
    // Not used in an earlier pass. Return empty AttachmentUsage
    return AttachmentUsage{};
}

RenderGraph::AttachmentUsage RenderGraph::find_next_usage(Context& ctx, Pass* current_pass, Attachment* attachment) {
    // Last pass isn't used later
    if (current_pass == &passes.back()) {
        return AttachmentUsage{};
    }

    // Go over each earlier pass
    for (Pass* pass = current_pass + 1; pass != &passes.back(); ++pass) {

        auto find_resource = [&ctx, attachment](ph::ResourceUsage const& resource) {
            if (resource.type != ResourceType::Attachment) return false;
            return attachment->view.id == ctx.get_attachment(resource.attachment.name)->view.id;
        };

        auto usage = std::find_if(pass->resources.begin(), pass->resources.end(), find_resource);
        if (usage != pass->resources.end()) {
            // Sampled in a later pass
            if (usage->access == ResourceAccess::ShaderRead) {
                return AttachmentUsage{
                    .stage = static_cast<VkPipelineStageFlags>(usage->stage),
                    .access = static_cast<VkAccessFlags>(usage->access),
                    .pass = pass
                };
            }
            // Used as output attachment in later pass
            if (usage->access == ResourceAccess::AttachmentOutput) {
                VkAccessFlags access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                if (is_depth_format(attachment->view.format)) {
                    access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
                return AttachmentUsage{
                    .stage = static_cast<VkPipelineStageFlags>(usage->stage),
                    .access = access,
                    .pass = pass
                };
            }
        }
    }
    // Not used in a later pass. Return empty AttachmentUsage
    return AttachmentUsage{};
}


void RenderGraphExecutor::execute(ph::CommandBuffer& cmd_buf, RenderGraph& graph) {
    for (size_t i = 0; i < graph.passes.size(); ++i) { 
        Pass& pass = graph.passes[i];
        RenderGraph::BuiltPass& build = graph.built_passes[i];
        if (build.handle) {
            VkRenderPassBeginInfo pass_begin_info{};
            pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            pass_begin_info.framebuffer = build.framebuf;
            pass_begin_info.renderArea.offset = VkOffset2D{ .x = 0, .y = 0 };
            pass_begin_info.renderArea.extent = build.render_area;
            pass_begin_info.renderPass = build.handle;
            std::vector<VkClearValue> clear_values;
            for (ResourceUsage const& resource : pass.resources) {
                if (!is_output_attachment(resource)) { continue; }
                if (resource.attachment.load_op == LoadOp::Clear) {
                    VkClearValue cv{};
                    static_assert(sizeof(VkClearValue) == sizeof(ph::ClearValue));
                    std::memcpy(&cv, &resource.attachment.clear, sizeof(VkClearValue));
                    clear_values.push_back(cv);
                }
            }
            pass_begin_info.clearValueCount = clear_values.size();
            pass_begin_info.pClearValues = clear_values.data();

            cmd_buf.begin_renderpass(pass_begin_info);
        }
		pass.execute(cmd_buf);
        if (build.handle) {
            cmd_buf.end_renderpass();
        }
	}
}

}