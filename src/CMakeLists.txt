find_package(GLM REQUIRED)
find_package(GLFW REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

set(SOURCE
	game.cpp
	main.cpp
	planet.cpp
	ddsloader.cpp
	flare.cpp
	thirdparty/shaun/shaun.cpp
	thirdparty/shaun/sweeper.cpp)

set(SOURCE_GL
	renderer_gl.cpp
	fence.cpp
	gl_util.cpp
	gl_profiler.cpp
	screenshot.cpp)

project (roche)

add_executable(roche ${SOURCE} ${SOURCE_GL})

if (CMAKE_BUILD_TYPE MATCHES Release)
	message("Coherent mapping enabled - Not supported by apitrace!")
	set(COMPILE_DEFS ${COMPILE_DEFS} -DUSE_COHERENT_MAPPING)
endif()

target_compile_definitions(roche PRIVATE ${COMPILE_DEFS})

target_compile_features(roche PRIVATE 
	cxx_auto_type 
	cxx_nullptr 
	cxx_range_for
	cxx_lambdas
	cxx_override)

target_include_directories(roche PRIVATE 
	${GLM_INCLUDE_DIRS}
	${GLFW_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR}
	${OPENGL_INCLUDE_DIR})

target_link_libraries(roche 
	${GLFW_LIBRARIES} 
	${GLEW_LIBRARY} 
	${OPENGL_gl_LIBRARY})