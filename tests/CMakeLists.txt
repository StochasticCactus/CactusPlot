# SPDX-License-Identifier: GPL-2.0-or-later
function(cactus_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE cactuscore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

cactus_add_test(test_math)
cactus_add_test(test_grace)

# Renders the same data with every style preset (used for visual review)
add_executable(cactus-gallery gallery.cpp)
target_link_libraries(cactus-gallery PRIVATE cactuscore)
