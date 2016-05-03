# check for Eigen
if (NOT DEFINED EIGEN_INCLUDE_DIR)
  message (FATAL_ERROR "'EIGEN_INCLUDE_DIR' variable not set
  include directory of Eigen library is not set
  set it using 'cmake <path/to/ACTS> -DEIGEN_INCLUDE_DIR=path/to/Eigen")
endif (NOT DEFINED EIGEN_INCLUDE_DIR)

# we need boost
find_package (Boost REQUIRED)

# get list of all source files
file (GLOB src_files "src/*/*.cpp")

# define library target
add_library (ACTSCore SHARED ${src_files})

# setup include directories
target_include_directories (ACTSCore PUBLIC ${EIGEN_INCLUDE_DIR})
target_include_directories (ACTSCore PUBLIC ${BOOST_INCLUDE_DIR})
target_include_directories (ACTSCore PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/> $<INSTALL_INTERFACE:include>)

# set compiler flags
target_compile_options (ACTSCore PUBLIC -std=c++1y)
target_compile_definitions (ACTSCore PUBLIC -DACTS_PARAMETER_DEFINITIONS_PLUGIN="${ACTS_PARAMETER_DEFINITIONS_PLUGIN}")
target_compile_definitions (ACTSCore PUBLIC -DACTS_MSG_MACROS_PLUGIN="${ACTS_MSG_MACROS_PLUGIN}")

# set installation directories
install (TARGETS ACTSCore EXPORT ACTSTargets
	 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install (DIRECTORY include/ACTS DESTINATION include)