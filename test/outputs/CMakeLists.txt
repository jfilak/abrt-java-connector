# Find all the input files
file(GLOB inFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.in")

foreach(infileName ${inFiles})
    string(REGEX REPLACE ".in\$" "" outfileName "${infileName}")
    set(outfile "${CMAKE_CURRENT_BINARY_DIR}/${outfileName}")
    set(infile "${CMAKE_CURRENT_SOURCE_DIR}/${infileName}")
    configure_file("${infile}" "${outfile}")
endforeach(infileName)
