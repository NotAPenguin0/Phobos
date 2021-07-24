
target_sources(Phobos PRIVATE 
	"impl/context.cpp"
	"impl/pipeline.cpp"
	"impl/attachment.cpp"
	"impl/frame.cpp"
	"impl/cache.cpp"
	"impl/image.cpp"
	"impl/buffer.cpp"

	"scratch_allocator.cpp"
	"command_buffer.cpp"
	"context.cpp"
	"image.cpp"
	"buffer.cpp"
	"log_interface.cpp"
	"memory.cpp"
	"pass.cpp"
	"pipeline.cpp"
	"queue.cpp"
	"render_graph.cpp"
	"shader.cpp"
)

if (${PHOBOS_ENABLE_RAY_TRACING}) 
	target_sources(Phobos PRIVATE "acceleration_structure.cpp")
endif()

# if (NOT ${PHOBOS_NO_FIXED_PIPELINE}) 
#    message("Phobos: Compiling fixed pipeline")
#    set(PHOBOS_SOURCES
#        ${PHOBOS_SOURCES}
#        "${CMAKE_CURRENT_SOURCE_DIR}/fixed/lighting_pass.cpp"
#        "${CMAKE_CURRENT_SOURCE_DIR}/fixed/skybox_renderer.cpp"
#        "${CMAKE_CURRENT_SOURCE_DIR}/fixed/util.cpp"
#        "${CMAKE_CURRENT_SOURCE_DIR}/fixed/deferred.cpp"
#        PARENT_SCOPE
#    )
# endif()