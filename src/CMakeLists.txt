add_subdirectory( multio )

if ( BUILD_MULTIOM )
    add_subdirectory( multiom )
    add_subdirectory( multio/mars2grib )
    add_subdirectory( multio/action/encode-mtg2 )
    
    if (HAVE_PYTHON) 
        add_subdirectory( pymars2grib_bindings )
    endif()
endif()

# Include api to defer dependencies
add_subdirectory( multio/api )
