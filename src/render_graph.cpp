#include <phobos/render_graph.hpp>
#include <cassert>

#include <exception>

namespace ph {

static bool is_output_attachment(ResourceUsage const& usage) {
    if (usage.type == ResourceType::Attachment && usage.access == ResourceAccess::ColorAttachmentOutput && usage.stage == PipelineStage::AttachmentOutput) return true;
    if (usage.type == ResourceType::Attachment && usage.access == ResourceAccess::DepthStencilAttachmentOutput && usage.stage == PipelineStage::AttachmentOutput) return true;

    return false;
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

            Attachment attachment = ctx.get_attachment(resource.attachment.name);
            // If a view is set, use that instead of the default ImageView.
            ImageView view = resource.attachment.view ? resource.attachment.view : attachment.view;
            VkAttachmentReference ref{};
            ref.attachment = attachment_index;
            if (is_depth_format(view.format)) {
                ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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
        build.handle = nullptr;

        // Figure out what barriers to create
        create_pass_barriers(ctx, *pass, build);

        // Only create renderpass in non-compute passes
        if (!pass->no_renderpass) {

            // Create subpass description
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = color_attachments.size();
            subpass.pColorAttachments = color_attachments.data();
            if (depth_ref != std::nullopt) {
                subpass.pDepthStencilAttachment = &*depth_ref;
            }

            std::vector<VkSubpassDependency> dependencies{};
            /*
            for (ResourceUsage const& resource : pass->resources) {
                if (resource.type != ResourceType::Attachment) continue;
                if (resource.access != ResourceAccess::ColorAttachmentOutput &&
                    resource.access != ResourceAccess::DepthStencilAttachmentOutput) continue;
                Attachment* attachment = ctx.get_attachment(resource.attachment.name);

                auto previous_usage_info = find_previous_usage(ctx, pass, attachment);
                if (previous_usage_info.second == nullptr) { // Not used earlier, no dependency needed
                    continue;
                }

                AttachmentUsage previous_usage = get_attachment_usage(previous_usage_info);

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
            */
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

            build.handle = ctx.get_or_create(rpci, "[Renderpass] " + pass->name);

            // Step 2: Create VkFramebuffer for this renderpass
            VkFramebufferCreateInfo fbci{};
            fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            std::vector<VkImageView> attachment_views{};
            for (ResourceUsage const& resource : pass->resources) {
                if (!is_output_attachment(resource)) { continue; }
                // If custom view was set, use that instead.
                VkImageView view = resource.attachment.view ? resource.attachment.view.handle : ctx.get_attachment(resource.attachment.name).view.handle;
                attachment_views.push_back(view);
            }
            fbci.attachmentCount = attachments.size();
            fbci.pAttachments = attachment_views.data();
            fbci.renderPass = build.handle;
            fbci.layers = 1;
            fbci.width = 1;
            fbci.height = 1;
            // Figure out width and height of the framebuffer. For this we need to find the largest size of all attachments.
            for (ResourceUsage const& resource : pass->resources) {
                if (!is_output_attachment(resource)) { continue; }
                Attachment attachment = ctx.get_attachment(resource.attachment.name);
                // Note that we can safely use the main ImageView here since all sizes for array layers of an image
                // must be the same
                fbci.width = std::max(attachment.view.size.width, fbci.width);
                fbci.height = std::max(attachment.view.size.height, fbci.height);
            }
            build.render_area = VkExtent2D{ .width = fbci.width, .height = fbci.height };
            build.framebuf = ctx.get_or_create(fbci, "[Framebuffer] " + pass->name);
        }
	}
}

std::vector<VkAttachmentDescription> RenderGraph::get_attachment_descriptions(Context& ctx, Pass* pass) {
	std::vector<VkAttachmentDescription> attachments;
	for (ResourceUsage const& resource : pass->resources) {
        if (!is_output_attachment(resource)) { continue; }
		VkAttachmentDescription description{};
		Attachment attachment = ctx.get_attachment(resource.attachment.name);
		assert(attachment && "Invalid attachment name");

		description.format = attachment.view.format;
		description.samples = attachment.view.samples;
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

static bool was_used(std::pair<ResourceUsage, Pass*> const& usage) {
    return usage.second != nullptr;
}

VkImageLayout RenderGraph::get_initial_layout(Context& ctx, Pass* pass, ResourceUsage const& resource) {
    // We need to find the most recent usage of this attachment as an output attachment and set the initial layout accordingly
  
    if (resource.attachment.load_op == LoadOp::DontCare) return VK_IMAGE_LAYOUT_UNDEFINED;

    auto previous_usage_info = find_previous_usage(ctx, pass, resource.attachment.name);
    // Not used previously
    if (!was_used(previous_usage_info)) {
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    AttachmentUsage previous_usage = get_attachment_usage(previous_usage_info);

    // Used as color output attachment previously
    if (previous_usage.access == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) {
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Used as depth output attachment previously
    if (previous_usage.access == VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) {
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    if (previous_usage.access == VK_ACCESS_SHADER_READ_BIT) {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    throw std::runtime_error("Invalid resource access");
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

    Attachment att = ctx.get_attachment(resource.attachment.name);
    auto next_usage_info = find_next_usage(ctx, pass, resource.attachment.name);

    if (!was_used(next_usage_info)) {
        // In this case, the attachments are not used later. We only need to check if this attachment is a swapchain attachment (in which case its used to present)
        if (ctx.is_swapchain_attachment(resource.attachment.name)) {
            // Case 2
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        // Case 3
        return get_output_layout_for_format(att.view.format);
    }

    AttachmentUsage next_usage = get_attachment_usage(next_usage_info);

    if (next_usage.access == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) {
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (next_usage.access == VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) {
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    if (next_usage.access == VK_ACCESS_SHADER_READ_BIT) {
//        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // Important note! We do not use the render pass's transition for images that are sampled later, since we will
        // automatically insert a barrier instead.
        return get_output_layout_for_format(att.view.format);
    }

    throw std::runtime_error("Invalid resource access");
}

std::pair<ResourceUsage, Pass*> RenderGraph::find_previous_usage(Context& ctx, Pass* current_pass, std::string_view attachment) {
    // First pass isn't used earlier
    if (current_pass == &passes.front()) {
        return {};
    }

    // Go over each earlier pass
    for (Pass* pass = current_pass - 1; pass >= &passes.front(); --pass) {
        // Look in this pass's resources
        auto find_resource = [&ctx, attachment](ph::ResourceUsage const& resource) {
            if (resource.type != ResourceType::Attachment) return false;
            // Since views are no longer always equal we compare by name.
            return attachment == resource.attachment.name;
        };

        auto usage = std::find_if(pass->resources.begin(), pass->resources.end(), find_resource);
        if (usage != pass->resources.end()) {
            return { *usage, pass };
        }
    }
    // Not used in an earlier pass. Return empty ResourceUsage
    return {};
}

std::pair<ResourceUsage, Pass*> RenderGraph::find_next_usage(Context& ctx, Pass* current_pass, std::string_view attachment) {
    // Last pass isn't used later
    if (current_pass == &passes.back()) {
        return {};
    }

    // Go over each later pass
    for (Pass* pass = current_pass + 1; pass <= &passes.back(); ++pass) {

        auto find_resource = [&ctx, attachment](ph::ResourceUsage const& resource) {
            if (resource.type != ResourceType::Attachment) return false;
            return attachment == resource.attachment.name;
        };

        auto usage = std::find_if(pass->resources.begin(), pass->resources.end(), find_resource);
        if (usage != pass->resources.end()) {
            return { *usage, pass };
        }
    }
    // Not used in a later pass. Return empty ResourceUsage
    return {};
}

std::pair<ResourceUsage, Pass*> RenderGraph::find_previous_usage(Pass* current_pass, BufferSlice const* buffer) {
    // First pass isn't used earlier
    if (current_pass == &passes.front()) {
        return {};
    }

    // Go over each earlier pass
    for (Pass* pass = current_pass - 1; pass >= &passes.front(); --pass) {
        // Look in this pass's resources
        auto find_resource = [buffer](ph::ResourceUsage const& resource) {
            if (resource.type != ResourceType::Buffer) return false;
            return *buffer == resource.buffer.slice;
        };

        auto usage = std::find_if(pass->resources.begin(), pass->resources.end(), find_resource);
        if (usage != pass->resources.end()) {
            return { *usage, pass };
        }
    }
    // Not used in an earlier pass. Return empty ResourceUsage
    return {};
}

std::pair<ResourceUsage, Pass*> RenderGraph::find_next_usage(Pass* current_pass, BufferSlice const* buffer) {
    // Last pass isn't used later
    if (current_pass == &passes.back()) {
        return {};
    }

    // Go over each later pass
    for (Pass* pass = current_pass + 1; pass <= &passes.back(); ++pass) {
        // Look in this pass's resources
        auto find_resource = [buffer](ph::ResourceUsage const& resource) {
            if (resource.type != ResourceType::Buffer) return false;
            return *buffer == resource.buffer.slice;
        };

        auto usage = std::find_if(pass->resources.begin(), pass->resources.end(), find_resource);
        if (usage != pass->resources.end()) {
            return { *usage, pass };
        }
    }
    // Not used in a later pass. Return empty ResourceUsage
    return {};
}

std::pair<ResourceUsage, Pass*> RenderGraph::find_previous_usage(Context& ctx, Pass* current_pass, ImageView const* image) {
    // First pass isn't used earlier
    if (current_pass == &passes.front()) {
        return {};
    }

    // Go over each earlier pass
    for (Pass* pass = current_pass - 1; pass >= &passes.front(); --pass) {
        // Look in this pass's resources
        auto find_resource = [image, &ctx](ph::ResourceUsage const& resource) {
            if (resource.type == ResourceType::Attachment) {
                return *image == ctx.get_attachment(resource.attachment.name).view;
            }
            if (resource.type != ResourceType::Image && resource.type != ResourceType::StorageImage) return false;
            return *image == resource.image.view;
        };

        auto usage = std::find_if(pass->resources.begin(), pass->resources.end(), find_resource);
        if (usage != pass->resources.end()) {
            return { *usage, pass };
        }
    }
    // Not used in an earlier pass. Return empty ResourceUsage
    return {};
}

std::pair<ResourceUsage, Pass*> RenderGraph::find_next_usage(Context& ctx, Pass* current_pass, ImageView const* image) {
    // Last pass isn't used later
    if (current_pass == &passes.back()) {
        return {};
    }

    // Go over each later pass
    for (Pass* pass = current_pass + 1; pass <= &passes.back(); ++pass) {
        // Look in this pass's resources
        auto find_resource = [image, &ctx](ph::ResourceUsage const& resource) {
            if (resource.type == ResourceType::Attachment) {
                return *image == ctx.get_attachment(resource.attachment.name).view;
            }
            if (resource.type != ResourceType::Image && resource.type != ResourceType::StorageImage) return false;
            return *image == resource.image.view;
        };

        auto usage = std::find_if(pass->resources.begin(), pass->resources.end(), find_resource);
        if (usage != pass->resources.end()) {
            return { *usage, pass };
        }
    }
    // Not used in a later pass. Return empty ResourceUsage
    return {};
}

RenderGraph::AttachmentUsage RenderGraph::get_attachment_usage(std::pair<ResourceUsage, Pass*> const& res_usage) {
    auto const& usage = res_usage.first;
    auto const& pass = res_usage.second;
    if (usage.access == ResourceAccess::ShaderRead) {
        return AttachmentUsage{
            .stage = static_cast<VkPipelineStageFlags>(usage.stage.value()),
            .access = static_cast<VkAccessFlags>(usage.access.value()),
            .pass = pass
        };
    }
    if (usage.access == ResourceAccess::ColorAttachmentOutput || usage.access == ResourceAccess::DepthStencilAttachmentOutput) {
        return AttachmentUsage{
            .stage = static_cast<VkPipelineStageFlags>(usage.stage.value()),
            .access = static_cast<VkAccessFlags>(usage.access.value()),
            .pass = pass
        };
    }
    return {};
}

void RenderGraph::create_pass_barriers(Context& ctx, Pass& pass, BuiltPass& result) {
    // We go over this pass's resources and look for buffers, find their next usage and insert barriers where necessary.
    // Note that we don't have to look at the previous usage, since there will already be a barrier inserted.
    for (ResourceUsage const& resource : pass.resources) {
        if (resource.type == ResourceType::Buffer) {
            ph::BufferSlice const* buffer = &resource.buffer.slice;
            auto next_usage = find_next_usage(&pass, buffer);

            // No barrier needed if this buffer is never used again
            if (next_usage.second) { // second is the pass, it will be nullptr when it's not used
                ph::ResourceUsage const& next = next_usage.first;

                VkBufferMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.buffer = buffer->buffer;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.offset = buffer->offset;
                barrier.size = buffer->range;
                barrier.pNext = nullptr;
                if (resource.access & ResourceAccess::ShaderRead) {
                    // When reading, we only need a barrier if the next usage is a write
                    if (next.access & ResourceAccess::ShaderWrite) {
                        barrier.srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                        barrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                    }
                }
                if (resource.access & ResourceAccess::ShaderWrite) {
                    // We always need a barrier when writing
                    barrier.srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                    if (next.access & ResourceAccess::ShaderRead) { barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT; }
                    if (next.access & ResourceAccess::ShaderWrite) { barrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT; }
                }

                // Only actually insert the barrier if we set any values (meaning we need it)
                if (barrier.srcAccessMask != VkAccessFlags{} && barrier.dstAccessMask != VkAccessFlags{}) {
                    Barrier final_barrier;
                    final_barrier.buffer = barrier;
                    final_barrier.type = BarrierType::Buffer;
                    final_barrier.src_stage = resource.stage;
                    final_barrier.dst_stage = next.stage;
                    result.post_barriers.push_back(final_barrier);
                }
            }
        }

        if (resource.type == ResourceType::StorageImage || resource.type == ResourceType::Image) {
            ph::ImageView const* view = &resource.image.view;

            // If there is no previous usage, we will insert an additional barrier to ensure the proper layout is used at the beginning of the frame
            auto previous_usage = find_previous_usage(ctx, &pass, view);
            if (!previous_usage.second) {
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.image = view->image;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(view->aspect);
                barrier.subresourceRange.baseMipLevel = view->base_level;
                barrier.subresourceRange.levelCount = view->level_count;
                barrier.subresourceRange.baseArrayLayer = view->base_layer;
                barrier.subresourceRange.layerCount = view->layer_count;
                barrier.pNext = nullptr;

                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // We don't know the last layout.
                if (resource.type == ResourceType::Image) barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                else if (resource.type == ResourceType::StorageImage) barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

                // No access since it wasn't used earlier
                barrier.srcAccessMask = {};
                barrier.dstAccessMask = static_cast<VkAccessFlagBits>(resource.access.value());

                Barrier final_barrier;
                final_barrier.image = barrier;
                final_barrier.type = BarrierType::Image;
                final_barrier.src_stage = ph::PipelineStage::AllCommands;
                final_barrier.dst_stage = resource.stage;
                // Note that this is a PRE barrier!
                result.pre_barriers.push_back(final_barrier);
            }

            auto next_usage = find_next_usage(ctx, &pass, view);

            if (next_usage.second) {
                ph::ResourceUsage const& next = next_usage.first;
                
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.image = view->image;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(view->aspect);
                barrier.subresourceRange.baseMipLevel = view->base_level;
                barrier.subresourceRange.levelCount = view->level_count;
                barrier.subresourceRange.baseArrayLayer = view->base_layer;
                barrier.subresourceRange.layerCount = view->layer_count;
                barrier.pNext = nullptr;

                if (resource.type == ResourceType::StorageImage) { barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL; }
                else if (resource.type == ResourceType::Image) { barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }
                else if (resource.type == ResourceType::Attachment) { 
                    Attachment att = ctx.get_attachment(resource.attachment.name);
                    if (is_depth_format(att.view.format)) {
                        barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    }
                    else {
                        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    }
                }

                if (resource.access & ResourceAccess::ShaderRead) {
                    // Read -> Read, we need a barrier for layout transition only if the next usage is not of the same usage type
                    if (next.access & ResourceAccess::ShaderRead) {
                        if (resource.type == ResourceType::StorageImage && next.type == ResourceType::Image) {
                            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            barrier.srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                            barrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                        }
                        else if (resource.type == ResourceType::Image && next.type == ResourceType::StorageImage) {
                            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                            barrier.srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                            barrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                        }
                    }
                    // Read -> Write, we need a barrier for synchronization, which will optionally do a layout transition
                    if (next.access & ResourceAccess::ShaderWrite) {
                        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // Writing only happens on storage images so we know this has to be VK_IMAGE_LAYOUT_GENERAL
                        barrier.srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                        barrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                    }
                }

                if (resource.access & ResourceAccess::ShaderWrite) {
                    // If we write and there is a next usage, we need a barrier
                    barrier.srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                    // Write -> Read, add transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL if next is not a storage image
                    if (next.access & ResourceAccess::ShaderRead) {
                        if (next.type == ResourceType::Image || next.type == ResourceType::Attachment) { barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }
                        else if (next.type == ResourceType::StorageImage) { barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; }
                        barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                    }
                    // Write -> Write, no transition needed since this is always a storage image, but we need a barrier for synchronization
                    if (next.access & ResourceAccess::ShaderWrite) {
                        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                        barrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                    }
                }

                // Insert barrier if necessary.
                if (barrier.srcAccessMask != VkAccessFlags{} && barrier.dstAccessMask != VkAccessFlags{}) {
                    Barrier final_barrier;
                    final_barrier.image = barrier;
                    final_barrier.type = BarrierType::Image;
                    final_barrier.src_stage = resource.stage;
                    final_barrier.dst_stage = next.stage;
                    result.post_barriers.push_back(final_barrier);
                }
            }
        }

        if (resource.type == ResourceType::Attachment) {
            Attachment attachment = ctx.get_attachment(resource.attachment.name);
            ph::ImageView view = resource.attachment.view ? resource.attachment.view : attachment.view;

            // If there is no previous usage, we will insert an additional barrier to ensure the proper layout is used at the beginning of the frame
            auto previous_usage = find_previous_usage(ctx, &pass, &view);
            if (!previous_usage.second) {
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.image = view.image;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(view.aspect);
                barrier.subresourceRange.baseMipLevel = view.base_level;
                barrier.subresourceRange.levelCount = view.level_count;
                barrier.subresourceRange.baseArrayLayer = view.base_layer;
                barrier.subresourceRange.layerCount = view.layer_count;
                barrier.pNext = nullptr;

                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // We don't know the last layout.
                // resource.type == Attachment
                if (resource.access == ResourceAccess::ColorAttachmentOutput) barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                if (resource.access == ResourceAccess::DepthStencilAttachmentOutput) barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                // Only proceed if we actually want a barrier.
                if (barrier.newLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
                    // We can assume resource.access is either colorattachmentoutput or depthstencilattachmentoutput here.

                    // No access since it wasn't used earlier
                    barrier.srcAccessMask = {};
                    barrier.dstAccessMask = static_cast<VkAccessFlagBits>(resource.access.value());

                    Barrier final_barrier;
                    final_barrier.image = barrier;
                    final_barrier.type = BarrierType::Image;
                    final_barrier.src_stage = ph::PipelineStage::AllCommands;
                    if (resource.access == ResourceAccess::ColorAttachmentOutput) {
                        final_barrier.dst_stage = ph::PipelineStage::AttachmentOutput;
                    }
                    else if (resource.access == ResourceAccess::DepthStencilAttachmentOutput) {
                        final_barrier.dst_stage = plib::bit_flag<ph::PipelineStage>{ ph::PipelineStage::LateFragmentTests } | ph::PipelineStage::EarlyFragmentTests;
                    }

                    // Note that this is a PRE barrier!
                    result.pre_barriers.push_back(final_barrier);
                }
            }

            auto next_usage = find_next_usage(ctx, &pass, &view);
            if (next_usage.second) {

                ph::ResourceUsage const& next = next_usage.first;

                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.image = view.image;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(view.aspect);
                barrier.subresourceRange.baseMipLevel = view.base_level;
                barrier.subresourceRange.levelCount = view.level_count;
                barrier.subresourceRange.baseArrayLayer = view.base_layer;
                barrier.subresourceRange.layerCount = view.layer_count;
                barrier.pNext = nullptr;

                if (resource.access & ResourceAccess::ColorAttachmentOutput ||
                    resource.access & ResourceAccess::DepthStencilAttachmentOutput) {

                    // Note that we do not want a barrier if the next usage is an output attachment,
                    // since this is taken care of by the renderpass.
                    if (next.access & ResourceAccess::ColorAttachmentOutput ||
                        next.access & ResourceAccess::DepthStencilAttachmentOutput) {

                    }
                    else {
                        // This is a write operation so we will need a barrier
                        // Use a cast so we don't need to switch between color/depth
                        barrier.srcAccessMask |= static_cast<VkAccessFlags>(resource.access.value());
                        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        if (is_depth_format(view.format)) {
                            barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        }

                        // Write -> Read, add transition
                        if (next.access & ResourceAccess::ShaderRead) {
                            if (next.type == ResourceType::Image) { barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }
                            else if (next.type == ResourceType::StorageImage) { barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; }
                            else if (next.type == ResourceType::Attachment) { barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }
                            barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                        }

                        // Write -> Write
                        if (next.access & ResourceAccess::ShaderWrite) {
                            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                            barrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                        }
                    }
                }

                // Insert barrier if necessary.
                if (barrier.srcAccessMask != VkAccessFlags{} && barrier.dstAccessMask != VkAccessFlags{}) {
                    Barrier final_barrier;
                    final_barrier.image = barrier;
                    final_barrier.type = BarrierType::Image;
                    if (resource.access == ResourceAccess::ColorAttachmentOutput) {
                        final_barrier.src_stage = ph::PipelineStage::AttachmentOutput;
                    }
                    else if (resource.access == ResourceAccess::DepthStencilAttachmentOutput) {
                        final_barrier.src_stage = plib::bit_flag<ph::PipelineStage>{ ph::PipelineStage::LateFragmentTests } | ph::PipelineStage::EarlyFragmentTests;
                    }
                    final_barrier.dst_stage = next.stage;
                    result.post_barriers.push_back(final_barrier);
                }
            }
        }
    }
}

void RenderGraphExecutor::execute(ph::CommandBuffer& cmd_buf, RenderGraph& graph) {
    for (size_t i = 0; i < graph.passes.size(); ++i) { 
        Pass& pass = graph.passes[i];
        RenderGraph::BuiltPass& build = graph.built_passes[i];
        // Before the pass begins we execute all pre barriers
        for (auto const& barrier : build.pre_barriers) {
            switch (barrier.type) {
            case RenderGraph::BarrierType::Buffer:
                cmd_buf.barrier(barrier.src_stage, barrier.dst_stage, barrier.buffer);
                break;
            case RenderGraph::BarrierType::Image:
                cmd_buf.barrier(barrier.src_stage, barrier.dst_stage, barrier.image);
                break;
            case RenderGraph::BarrierType::Memory:
                cmd_buf.barrier(barrier.src_stage, barrier.dst_stage, barrier.memory);
                break;
            }
        }
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
        if (pass.execute != nullptr) {
            pass.execute(cmd_buf);
        }
        if (build.handle) {
            cmd_buf.end_renderpass();
        }
        // Once the execution and optionally renderpass is complete we add all barriers.
        for (auto const& barrier : build.post_barriers) {
            switch (barrier.type) {
            case RenderGraph::BarrierType::Buffer:
                cmd_buf.barrier(barrier.src_stage, barrier.dst_stage, barrier.buffer);
                break;
            case RenderGraph::BarrierType::Image:
                cmd_buf.barrier(barrier.src_stage, barrier.dst_stage, barrier.image);
                break;
            case RenderGraph::BarrierType::Memory:
                cmd_buf.barrier(barrier.src_stage, barrier.dst_stage, barrier.memory);
                break;
            }
        }
	}
}

}