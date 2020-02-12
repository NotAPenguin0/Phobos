set(PHOBOS_SOURCES
    ${PHOBOS_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/core/vulkan_context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/window_context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/physical_device.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/swapchain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/device.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline/layouts.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline/pipeline.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline/pipelines.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline/pipeline_layout.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/present/present_manager.cpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/util/image_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/shader_util.cpp"
    PARENT_SCOPE
)