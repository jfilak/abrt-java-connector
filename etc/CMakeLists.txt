install(FILES java_event.conf
        DESTINATION ${SYSCONF_INSTALL_DIR}/libreport/events.d)

install(FILES java_event.conf.5 bugzilla_format_java.conf.5 bugzilla_formatdup_java.conf.5
        DESTINATION ${MAN_INSTALL_DIR}/man5)

install(FILES bugzilla_format_java.conf bugzilla_formatdup_java.conf
        DESTINATION ${SYSCONF_INSTALL_DIR}/libreport/plugins)
