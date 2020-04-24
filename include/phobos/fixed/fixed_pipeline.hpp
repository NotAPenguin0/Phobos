#ifndef PHOBOS_FIXED_PIPELINE_HPP_
#define PHOBOS_FIXED_PIPELINE_HPP_

#include <phobos/fixed/generic_renderer.hpp>
#include <phobos/fixed/skybox_renderer.hpp>
#include <phobos/fixed/util.hpp>
#include <phobos/fixed/deferred.hpp>

/* The Phobos fixed pipeline implements a deferred renderer on top of the Phobos API. 
 * All fixed pipeline functions and classes can be found inside the ph::fixed namespace. For the fixed pipeline to work, you have
 * to create an instance of ph::fixed::DeferredRenderer after creating your vulkan context.
 * 
 * The deferred pipeline needs two renderpasses to work: A deferred main pass and a resolve pass. You can create these by calling the
 * DeferredRenderer::build_XX_pass() functions. This function builds the renderpass and inserts it into the rendergraph.
 */

#endif