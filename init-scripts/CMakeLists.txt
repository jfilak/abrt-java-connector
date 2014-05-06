install(FILES abrt-java.service
        DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR})

install(PROGRAMS abrt-java-connector
        DESTINATION ${LIBEXEC_INSTALL_DIR})


