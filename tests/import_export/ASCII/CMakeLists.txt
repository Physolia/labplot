add_executable (AsciiFilterTest AsciiFilterTest.cpp)

target_link_libraries(AsciiFilterTest labplot2backendlib labplot2lib labplot2nsllib labplot2test)

add_test(NAME AsciiFilterTest COMMAND AsciiFilterTest)
