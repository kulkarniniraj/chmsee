include_directories(${CHMSEE_INCLUDE_DIRS})
add_definitions(${CHMSEE_CFLAGS_OTHER})
link_directories(${CHMSEE_LIBRARY_DIRS})

add_executable(embed TestGtkEmbed.cpp)
target_link_libraries(embed ${CHMSEE_LIBRARIES})
