#include <phobos/render_graph.hpp>
#include <cassert>

namespace ph {

void RenderGraph::add_pass(Pass pass) {
	passes.push_back(std::move(pass));
}

void RenderGraph::build(Context& ctx) {
	for (auto it = passes.begin(); it != passes.end(); ++it) {
        Pass* pass = &*it;
        auto attachments = get_attachment_descriptions(ctx, pass);
        (void)0;
	}
}

std::vector<VkAttachmentDescription> RenderGraph::get_attachment_descriptions(Context& ctx, Pass* pass) {
	std::vector<VkAttachmentDescription> attachments;
	for (size_t i = 0; i < pass->outputs.size(); ++i) {
		VkAttachmentDescription description{};
		Attachment* attachment = ctx.get_attachment(pass->outputs[i].name);
		assert(attachment && "Invalid attachment name");

		description.format = attachment->view.format;
		description.samples = attachment->view.samples;
		description.loadOp = static_cast<VkAttachmentLoadOp>(pass->outputs[i].load_op);
		description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// Stencil operations are currently not supported
		description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        description.initialLayout = get_initial_layout(ctx, pass, &pass->outputs[i]);
        description.finalLayout = get_final_layout(ctx, pass, &pass->outputs[i]);

		attachments.push_back(description);
	}
	return attachments;
}

static bool is_depth_format(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return true;
    default:
        return false;
    }
}

static VkImageLayout get_output_layout_for_format(VkFormat format) {
    if (is_depth_format(format)) { return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; }

    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

VkImageLayout RenderGraph::get_initial_layout(Context& ctx, Pass* pass, PassOutput* attachment) {
    // We need to find the most recent usage of this attachment as an output attachment and set the initial layout accordingly

    if (attachment->load_op == LoadOp::DontCare) return VK_IMAGE_LAYOUT_UNDEFINED;

    // If this pass is the first pass in the list of renderpasses, we can simply put the initialLayout as UNDEFINED
    if (pass == &passes.front()) { return VK_IMAGE_LAYOUT_UNDEFINED; }

    // For each earlier pass ...
    for (auto it = pass - 1; it >= &passes.front(); --it) {
        // ... we check if this pass contains the current attachment as an output attachment

        // Custom comparator to compare ph::PassOutputs by their attachment's name. See also get_final_layout
        auto find_func = [&ctx, attachment](ph::PassOutput const& candidate) {
            return attachment->name == candidate.name;
        };

        Attachment* att = ctx.get_attachment(attachment->name);

        auto found_it = std::find_if(it->outputs.begin(), it->outputs.end(), find_func);

        if (found_it != it->outputs.end()) {
            // The attachment is used as an output attachment in a previous pass. Return the matching layout.
            return get_output_layout_for_format(att->view.format);
        }

        // If it wasn't found in the outputs vector, check if it was sampled instead
        auto find_in_sampled_func = [&ctx, attachment](std::string_view candidate) {
            return attachment->name == candidate;
        };

        auto found_sampled = std::find_if(it->sampled_attachments.begin(), it->sampled_attachments.end(), find_in_sampled_func);
        if (found_sampled != it->sampled_attachments.end()) {
            // The attachment was sampled in te previous pass, this means ShaderReadOnlyOptimal was the last layout
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        if (it == &passes.front()) { break; }
    }

    // If it was not used as an output attachment anywhere, we don't need to load and we can specify Undefined as layout
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkImageLayout RenderGraph::get_final_layout(Context& ctx, Pass* pass, PassOutput* attachment) {
    // For the final layout, there are 3 options:
    // 1. The attachment is used later as an input attachment.
    //      In this case, finalLayout becomes eShaderReadOnlyOptimal
    // 2. The attachement is used later to present
    //      In this case, finalLayout becomes ePresentSrcKHR
    // 3. The attachment is not used later
    //      In this case, finalLayout becomes the corresponding format's output attachment layout

    // Note that 'later used' in this case only means the NEXT usage of this attachment. Any further usage can be taken care of by
    // further renderpasses

    // First, we create a span of passes following the current renderpass. We check if this pass is the last pass first.
    Attachment* att = ctx.get_attachment(attachment->name);
    if (pass == &passes.back()) {
        // In this case, the attachments are not used later. We only need to check if this attachment is a swapchain attachment (in which case its used to present)
        if (ctx.is_swapchain_attachment(attachment->name)) {
            // Case 2
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        // Case 3
        return get_output_layout_for_format(att->view.format);
    }

    // Now we have verified the current pass isn't the final renderpass, we can continue.

    // For each following pass ...
    for (Pass* later_pass = pass + 1; later_pass <= &passes.back(); ++later_pass) {
        // ... we check if this pass contains the current attachment as an input attachment

        auto find_in_sampled_func = [&ctx, attachment](std::string_view candidate) {
            return attachment->name == candidate;
        };

        auto found_it = std::find_if(later_pass->sampled_attachments.begin(), later_pass->sampled_attachments.end(), find_in_sampled_func);

        if (found_it != later_pass->sampled_attachments.end()) {
            // The attachment is used as an input attachment in a later pass. This is case 1
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        // Custom comparator to compare ph::PassOutputs by their attachment's name. See also get_intial_layout
        auto find_func = [&ctx, attachment](ph::PassOutput const& candidate) {
            return attachment->name == candidate.name;
        };
        auto found_in_outputs = std::find_if(later_pass->outputs.begin(), later_pass->outputs.end(), find_func);
        if (found_in_outputs != later_pass->outputs.end()) {
            return get_output_layout_for_format(att->view.format);
        }
    }

    // If we haven't encountered a single return in the previous loop, the attachment is not used anywhere else, 
    // or only again as an output attachment.
    // Case 3
    return get_output_layout_for_format(att->view.format);
}

void RenderGraphExecutor::execute(ph::CommandBuffer& cmd_buf, RenderGraph& graph) {
	for (Pass const& pass : graph.passes) {
		pass.execute(cmd_buf);
	}
}

}