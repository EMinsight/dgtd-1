message(STATUS "Creating build system for maxwell-mfemExtension")

add_library(maxwell-mfemExtension STATIC 
	"BilinearIntegrators.cpp"
	"LinearIntegrators.cpp" 
	"IntegratorFunctions.cpp" 
	)

target_link_libraries(maxwell-mfemExtension mfem)

if (SEMBA_DGTD_ENABLE_OPENMP)
	message(STATUS "Linking maxwell-mfemExtension with OpenMP libraries")
	target_link_libraries(maxwell-mfemExtension OpenMP::OpenMP_C OpenMP::OpenMP_CXX)
endif()
