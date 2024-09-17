cmake_minimum_required(VERSION 3.10)

# Set project name and version
project(SimpleDPDKGPU VERSION 1.0 LANGUAGES C CXX CUDA)

# Set C and C++ standards
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_CXX_STANDARD 14)  # Set C++ standard for CUDA support
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add project source files
set(SOURCES
    ./simple_dpdk_gpu.cu # Replace with your actual CUDA source files
)

# Find DPDK using PkgConfig (System-installed DPDK)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# Add DPDK include directories
include_directories(${DPDK_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add CUDA include directory (if needed)
include_directories(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

# Link DPDK libraries to the executable
link_directories(${DPDK_LIBRARY_DIRS})

# Add the project executable
add_executable(SimpleDPDKGPU ${SOURCES})

# Link the DPDK and CUDA libraries to the target
target_link_libraries(SimpleDPDKGPU ${DPDK_LIBRARIES} cuda cudart)

# Optionally, add optimization flags for performance
target_compile_options(SimpleDPDKGPU PRIVATE -march=native -O3)

# Set CUDA-specific compilation flags (if needed)
set_target_properties(SimpleDPDKGPU PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON  # Enables separate compilation for CUDA
    CUDA_STANDARD 11               # Sets the CUDA standard
)
