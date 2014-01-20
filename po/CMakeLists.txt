project(po)

if (AJC_ENABLE_NLS)
    find_program(INTLTOOL_UPDATE_CMD intltool-update)
    mark_as_advanced(INTLTOOL_UPDATE_CMD)

    find_program(XGETTEXT_CMD xgettext)
    mark_as_advanced(XGETTEXT_CMD)

    find_program(MSGMERGE_CMD msgmerge)
    mark_as_advanced(MSGMERGE_CMD)

    find_program(MSGFMT_CMD msgfmt)
    mark_as_advanced(MSGFMT_CMD)

    find_program(MSGFMT_CMD cat)
    mark_as_advanced(CAT_CMD)

    file(STRINGS ${po_SOURCE_DIR}/LINGUAS AJC_LINGUAS)
    set(AJC_LINGUAS_TARGETS)
    set(AJC_POTFILE ${CMAKE_PROJECT_NAME}.pot)
    set(AJC_POTFILE_OUTPUT ${po_BINARY_DIR}/${AJC_POTFILE})

    add_custom_target(nls-update-sources
            ${INTLTOOL_UPDATE_CMD} -m
            WORKING_DIRECTORY ${po_SOURCE_DIR})

    add_custom_command(OUTPUT ${AJC_POTFILE_OUTPUT}
            COMMAND ${XGETTEXT_CMD} --files-from ${po_SOURCE_DIR}/POTFILES.in --keyword=_ -o ${AJC_POTFILE_OUTPUT} --copyright-holder="ABRT Team" --msgid-bugs-address="crash-catcher at lists.fedorahosted.org" --no-wrap --no-location
            DEPENDS POTFILES.in
            WORKING_DIRECTORY ${abrt-java-connector_SOURCE_DIR}
            COMMENT "Extract translatable messages to ${AJC_POTFILE}"
    )

    foreach(language ${AJC_LINGUAS})
        set(language_SOURCE ${po_SOURCE_DIR}/${language}.po)
        set(language_OUTPUT ${po_BINARY_DIR}/${language}/LC_MESSAGES/${CMAKE_PROJECT_NAME}.mo)

        add_custom_target(nls-update-${language}.po
                ${MSGMERGE_CMD} ${language} ${AJC_POTFILE_OUTPUT} -o ${language_SOURCE} --no-wrap
                DEPENDS ${language_SOURCE} ${AJC_POTFILE_OUTPUT}
                WORKING_DIRECTORY ${po_SOURCE_DIR}
        )

        file(MAKE_DIRECTORY "${po_BINARY_DIR}/${language}/LC_MESSAGES")

        add_custom_command(OUTPUT ${language_OUTPUT}
                COMMAND ${MSGFMT_CMD} -c -o ${language_OUTPUT} ${language_SOURCE}
                DEPENDS ${language_SOURCE}
        )

        install(FILES ${language_OUTPUT}
                DESTINATION share/${CMAKE_PROJECT_NAME}/locale/${language}/LC_MESSAGES
        )

        set(AJC_LINGUAS_TARGETS ${AJC_CATALOG_TARGETS} ${language_OUTPUT})
    endforeach(language)

    add_custom_target(nls ALL
                      DEPENDS ${AJC_POTFILE_OUTPUT} ${AJC_LINGUAS_TARGETS}
    )
else(AJC_ENABLE_NLS)
    message("Native Language Support is disabled")
endif (AJC_ENABLE_NLS)
