# Find all the input files
file(GLOB inFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.in")

set(system_specific_dir "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

foreach(infileName ${inFiles})
    string(REGEX REPLACE ".in\$" "" outfileName "${infileName}")
    set(outfile "${CMAKE_CURRENT_BINARY_DIR}/${outfileName}")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${system_specific_dir}/${infileName}")
        set(infile "${CMAKE_CURRENT_SOURCE_DIR}/${system_specific_dir}/${infileName}")
    else()
        set(infile "${CMAKE_CURRENT_SOURCE_DIR}/${infileName}")
    endif()

    configure_file("${infile}" "${outfile}")
endforeach(infileName)
