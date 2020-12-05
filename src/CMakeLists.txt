
target_sources(Phobos PRIVATE 
	"command_buffer.cpp"
	"context.cpp"
	"image.cpp"
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