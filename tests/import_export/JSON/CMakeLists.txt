add_executable (JSONFilterTest JSONFilterTest.cpp)

target_link_libraries(JSONFilterTest labplot2backendlib labplot2lib labplot2nsllib labplot2test)

add_test(NAME JSONFilterTest COMMAND JSONFilterTest)
