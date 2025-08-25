pybind11_add_module(pymars2grib_bindings MODULE pymars2grib_bindings.cc)

target_link_libraries(pymars2grib_bindings
    PRIVATE pybind11::module 
            pybind11::lto 
            multio
            multio-mars2grib
            multiom-encoders
            eccodes
            atlas
)

# pybind11_extension(pychunked_data_view)
if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    # Strip unnecessary sections of the binary on Linux/macOS
    pybind11_strip(pymars2grib_bindings)
endif()

set_target_properties(pymars2grib_bindings
  PROPERTIES 
  #CXX_VISIBILITY_PRESET "hidden" 
  # CUDA_VISIBILITY_PRESET "hidden"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
  )

install(
  TARGETS pymars2grib_bindings
  COMPONENT python
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
