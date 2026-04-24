# Get CMake PreDefined Variables https://cmake.org/cmake/help/latest/manual/cmake-variables.7.html
# This CMake File for OpenGL Projects is compiled by Kushagra
# I do not claim that this is the best CMake Configuration you will get but it does work.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR)

project(PurePool)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add all Source Files
file(GLOB_RECURSE SOURCE_FILES 
	${CMAKE_SOURCE_DIR}/code/Src/*.cpp
	${CMAKE_SOURCE_DIR}/code/*.cpp
)

# Add all Header Files
file(GLOB_RECURSE HEADER_FILES 
	${CMAKE_SOURCE_DIR}/code/Inc/*.h
	${CMAKE_SOURCE_DIR}/code/Inc/*.hpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})


# -----------------
## 查找系统包
# -----------------
find_package(OpenGL REQUIRED)



# Variable for the Libs to add to the Linkers
if (WIN32)
	set(LIBS glfw opengl32 glad)
elseif (APPLE)
	set(LIBS glfw glad "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreFoundation")
elseif (UNIX)
	set(LIBS glfw GL glad X11 pthread dl)
endif ()

# -----------------
## 外部库路径
# -----------------
set(GLFW_ROOT_DIR external/glfw)
set(GLAD_ROOT_DIR external/glad)
set(GLM_ROOT_DIR external/glm)
set(STB_ROOT_DIR external/stb)
set(TINYGLTF_ROOT_DIR external/tinygltf)

# -----------------
## 引入外部库
# -----------------
add_subdirectory(${GLFW_ROOT_DIR})
add_subdirectory(${GLAD_ROOT_DIR})

# -----------------
## 接口库 (Header-Only)
# -----------------
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${GLM_ROOT_DIR})

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${STB_ROOT_DIR})

add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE ${TINYGLTF_ROOT_DIR})



# -----------------
## 包含目录 (Include Directories)
# -----------------
target_include_directories(${PROJECT_NAME}
	PUBLIC ${GLAD_ROOT_DIR}/include/glad
	PUBLIC ${CMAKE_SOURCE_DIR}/code/Inc
	PRIVATE ${GLFW_ROOT_DIR}/include
)

# Add the Link Directories for the Libraries / Files
# Used for creation of Static/Dynamic Libraries

# Include Files From Within the Workspace Folder
target_link_directories(${PROJECT_NAME} 
	PRIVATE ${GLFW_ROOT_DIR}/src
	PRIVATE ${GLAD_ROOT_DIR}/src
)

# Add the Libs to the Linker for the Project

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${LIBS}
		glm
        stb
        tinygltf
)

# EXPLANATION FOR SOME STUFF

# GLOB_RECURSE -> Recursively go through all subdirectories
#		of the given path and get all the files matching the extension provided
# file() -> Store the Files matching the RegEx into a variable
# set() -> Alternative to file() if you want to set variables for a custom behaviour
