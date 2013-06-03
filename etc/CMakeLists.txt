install(FILES java_event.conf
        DESTINATION ${SYSCONF_INSTALL_DIR}/libreport/events.d)

install(FILES bugzilla_format_java.conf bugzilla_formatdup_java.conf
        DESTINATION ${SYSCONF_INSTALL_DIR}/libreport/plugins)
