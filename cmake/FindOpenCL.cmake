
FIND_PACKAGE( PackageHandleStandardArgs )

	FIND_LIBRARY(OPENCL_LIBRARIES OpenCL ENV LD_LIBRARY_PATH "/opt/rocm/opencl/lib/x86_64/")

	GET_FILENAME_COMPONENT(OPENCL_LIB_DIR ${OPENCL_LIBRARIES} PATH)
	GET_FILENAME_COMPONENT(_OPENCL_INC_CAND ${OPENCL_LIB_DIR}/../../include ABSOLUTE)

	FIND_PATH(OPENCL_INCLUDE_DIRS CL/cl.h PATHS ${_OPENCL_INC_CAND}
                        "/opt/AMDAPP/include/"
                        "/opt/rocm/opencl/include/"
                        "/usr/local/cuda/include")
	FIND_PATH(_OPENCL_CPP_INCLUDE_DIRS CL/cl.hpp PATHS ${_OPENCL_INC_CAND}
                        "/opt/AMDAPP/include/"
                        "/opt/rocm/opencl/include/"
                        "/usr/local/cuda/include")

FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenCL DEFAULT_MSG OPENCL_LIBRARIES OPENCL_INCLUDE_DIRS)

MARK_AS_ADVANCED(
  OPENCL_INCLUDE_DIRS
)

