set(PHOBOS_SOURCES
    ${PHOBOS_SOURCES}

    "${CMAKE_CURRENT_SOURCE_DIR}/core/vulkan_context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/window_context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/physical_device.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/swapchain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/device.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/events/event_dispatcher.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline/pipeline.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline/default_pipelines.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/present/present_manager.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/command_buffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/mesh.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/instancing_buffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/texture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/render_attachment.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/render_graph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/render_target.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/stb_image.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/util/cmdbuf_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/image_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/shader_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/memory_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/buffer_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/log_interface.cpp"
    PARENT_SCOPE
)