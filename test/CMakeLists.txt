include(FetchContent)

FetchContent_Declare(glfw
	GIT_REPOSITORY https://github.com/glfw/glfw
)

FetchContent_MakeAvailable(glfw)

add_executable(TestApp)
target_link_libraries(TestApp PRIVATE Phobos glfw)
target_sources(TestApp PRIVATE "main.cpp")