#include <phobos/fixed/generic_renderer.hpp>

#include <phobos/fixed/util.hpp>

namespace ph::fixed {

uint32_t GenericRenderer::add_material(Material const& mat) {
	materials.push_back(mat);
	return materials.size() - 1;
}

void GenericRenderer::add_draw(Mesh* mesh, uint32_t material_index, glm::mat4 const& transform) {
	draws.push_back({ mesh, material_index, (uint32_t)transforms.size() });
	transforms.push_back(transform);
}

void GenericRenderer::execute(FrameInfo& frame, CommandBuffer& cmd_buf) {
	if (draws.empty()) return;

	ph::RenderPass* pass_ptr = cmd_buf.get_active_renderpass();
	STL_ASSERT(pass_ptr, "GenericRenderer::execute called outside an active renderpass");
	ph::RenderPass& pass = *pass_ptr;

	Pipeline pipeline = get_pipeline(ctx, "generic", pass);
	cmd_buf.bind_pipeline(pipeline);
	
	update_transforms(cmd_buf);

	vk::DescriptorSet set = get_descriptor_set(frame, pass);
	cmd_buf.bind_descriptor_set(0, set);

	// Submit draws
	for (auto& draw : draws) {
		Mesh* mesh = draw.mesh;
		// Bind draw data
		cmd_buf.bind_vertex_buffer(0, mesh->get_vertices());
		cmd_buf.bind_index_buffer(mesh->get_indices());

		// update push constant ranges
		stl::uint32_t const transform_index = draw.transform_index;
		cmd_buf.push_constants(vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint32_t), &transform_index);
		// First texture is diffuse, second is specular. See also get_fixed_descriptor_set() where we fill the textures array
		stl::uint32_t const texture_indices[] = { 3 * draw.material_index, 3 * draw.material_index + 1, 3 * draw.material_index + 2 };
		cmd_buf.push_constants(vk::ShaderStageFlagBits::eFragment, sizeof(uint32_t), sizeof(texture_indices), &texture_indices);
		// Execute drawcall
		cmd_buf.draw_indexed(mesh->get_index_count(), 1, 0, 0, 0);
		frame.draw_calls++;
	}
}

void GenericRenderer::update_transforms(ph::CommandBuffer& cmd_buf) {
	vk::DeviceSize const size = transforms.size() * sizeof(glm::mat4);
	transform_ssbo = cmd_buf.allocate_scratch_ssbo(size);
	
	std::memcpy(transform_ssbo.data, transforms.data(), size);
}

vk::DescriptorSet GenericRenderer::get_descriptor_set(FrameInfo& frame, ph::RenderPass& pass) {
	PipelineCreateInfo const* pci = ctx->pipelines.get_named_pipeline("generic");
	STL_ASSERT(pci, "Generic pipeline not created");
	ShaderInfo const& shader_info = pci->shader_info;

	BuiltinUniforms builtin_buffers = renderer->get_builtin_uniforms();
	DefaultTextures& textures = renderer->get_default_textures();

	DescriptorSetBinding bindings;
	bindings.add(make_descriptor(shader_info["camera"], builtin_buffers.camera));
	bindings.add(make_descriptor(shader_info["transforms"], transform_ssbo));
	stl::vector<ImageView> texture_views;
	texture_views.reserve(materials.size() * 4);

	for (auto const& mat : materials) {
		auto push_or_default = [&texture_views](ph::Texture* tex, ph::Texture& def) {
			if (tex) { texture_views.push_back(tex->view_handle()); }
			else { texture_views.push_back(def.view_handle()); }
		};

		push_or_default(mat.diffuse, textures.color);
		push_or_default(mat.specular, textures.specular);
		push_or_default(mat.normal, textures.normal);
	}

	bindings.add(make_descriptor(shader_info["textures"], texture_views, frame.default_sampler));
	bindings.add(make_descriptor(shader_info["lights"], builtin_buffers.lights));

	// We need variable count to use descriptor indexing
	vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
	uint32_t counts[1]{ meta::max_unbounded_array_size };
	variable_count_info.descriptorSetCount = 1;
	variable_count_info.pDescriptorCounts = counts;

	return renderer->get_descriptor(frame, pass, bindings, &variable_count_info);
}

}