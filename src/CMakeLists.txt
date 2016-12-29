find_package(GLM REQUIRED)
find_package(GLFW REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(NANOGUI REQUIRED)
find_package(Eigen3 REQUIRED)

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

if (CMAKE_BUILD_TYPE MATCHES Debug)
	message("KHR_Debug enabled")
	set(COMPILE_DEFS ${COMPILE_DEFS} -DUSE_KHR_DEBUG)
endif()

target_compile_definitions(roche PRIVATE ${COMPILE_DEFS})

target_compile_features(roche PRIVATE cxx_auto_type cxx_nullptr cxx_range_for)

target_include_directories(roche PRIVATE 
	${GLM_INCLUDE_DIRS}
	${GLFW_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR}
	${NANOGUI_INCLUDE_DIR}
	${EIGEN3_INCLUDE_DIRS})

target_link_libraries(roche 
	${GLFW_LIBRARIES} 
	${GLEW_LIBRARY} 
	${OPENGL_gl_LIBRARY}
	${NANOGUI_LIBRARY})