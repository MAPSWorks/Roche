find_package(GLM REQUIRED)
find_package(GLFW REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

set(SOURCE
	game.cpp
	main.cpp
	shader.cpp
	planet.cpp
	util.cpp
	renderer_gl.cpp
	shader.cpp
	fence.cpp
	thirdparty/lodepng.cpp
	thirdparty/shaun/shaun.cpp
	thirdparty/shaun/sweeper.cpp)

project (roche)

add_executable(roche ${SOURCE})

target_compile_features(roche PRIVATE cxx_auto_type cxx_nullptr cxx_range_for)

target_include_directories(roche PRIVATE 
	${GLM_INCLUDE_DIRS}
	${GLFW_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR})

target_link_libraries(roche ${GLFW_LIBRARIES} ${GLEW_LIBRARY} ${OPENGL_gl_LIBRARY})