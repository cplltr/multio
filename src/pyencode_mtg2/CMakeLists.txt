pybind11_add_module(pyencode_mtg2 MODULE pyencode_mtg2.cc)

target_link_libraries(pyencode_mtg2
    PRIVATE pybind11::module 
            pybind11::lto 
            multio-action-encode-mtg2
            multio
            multiom-encoders
            eccodes
            atlas
)

# pybind11_extension(pychunked_data_view)
if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    # Strip unnecessary sections of the binary on Linux/macOS
    pybind11_strip(pyencode_mtg2)
endif()

set_target_properties(pyencode_mtg2
  PROPERTIES 
  #CXX_VISIBILITY_PRESET "hidden" 
  # CUDA_VISIBILITY_PRESET "hidden"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
  )

install(
  TARGETS pyencode_mtg2
  COMPONENT python
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

