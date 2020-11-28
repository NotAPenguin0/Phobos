add_executable(TestApp)
target_link_libraries(TestApp PRIVATE Phobos)
target_sources(TestApp PRIVATE "main.cpp")