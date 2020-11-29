
target_sources(Phobos PRIVATE 
	"core/context.cpp"
	"core/log_interface.cpp"
	"core/queue.cpp"
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