
target_sources(Phobos PRIVATE 
	"context/context.cpp"
	"context/pipeline.cpp"
	"context/attachment.cpp"
	"context/frame.cpp"
	"context/cache.cpp"
	"context/image.cpp"
	"context/buffer.cpp"

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