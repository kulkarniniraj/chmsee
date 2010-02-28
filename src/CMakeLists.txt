include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CHMSEE_INCLUDE_DIRS}
        )
add_definitions(
        ${CHMSEE_CFLAGS_OTHER}
        )
add_custom_command(
        OUTPUT marshal.h
        COMMAND ${GLIB_GENMARSHAL} --prefix=marshal ${CMAKE_CURRENT_SOURCE_DIR}/marshal.list --header > ${CMAKE_CURRENT_BINARY_DIR}/marshal.h
        DEPENDS marshal.list
        )

add_custom_command(
        OUTPUT marshal.c
        COMMAND ${GLIB_GENMARSHAL} --prefix=marshal ${CMAKE_CURRENT_SOURCE_DIR}/marshal.list --body > ${CMAKE_CURRENT_BINARY_DIR}/marshal.c
        DEPENDS marshal.list
        )

link_directories(
        ${CHMSEE_LIBRARY_DIRS}
        )

set(CHMSEE_SRCS
        chmsee.h chmsee.c
        startup.h startup.c
        setup.h setup.c
        ihtml.h ihtml.c
        html-factory.h html-factory.c
        utils.h utils.c
        ${CMAKE_CURRENT_BINARY_DIR}/marshal.h ${CMAKE_CURRENT_BINARY_DIR}/marshal.c
        )
set(CHMSEE_SRCS
        ${CHMSEE_SRCS}
        components/book.h components/book.c
        components/toc.h components/toc.c
        components/index.h components/index.c
        components/bookmarks.h components/bookmarks.c
        components/treeview.h components/treeview.c
        components/html-gecko.h components/html-gecko.c
        components/gecko-utils.h components/gecko-utils.cpp
        )
set(CHMSEE_SRCS
        ${CHMSEE_SRCS}
        models/bookmarksfile.h models/bookmarksfile.c
        models/chmfile.h models/chmfile.c
        models/parser.h models/parser.c
        models/link.h models/link.c
        )

add_library(chmseelib STATIC ${CHMSEE_SRCS})
target_link_libraries(chmseelib
        ${CHMSEE_LIBRARIES}
        )

add_executable(chmsee main.c)
target_link_libraries(chmsee
        chmseelib
        )

install(TARGETS chmsee DESTINATION bin)
