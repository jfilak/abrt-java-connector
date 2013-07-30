install(FILES java_event.conf java_event_fedora.conf java_event_rhel.conf
        DESTINATION ${SYSCONF_INSTALL_DIR}/libreport/events.d)

install(FILES report_fedora_java.conf report_rhel_java.conf
        DESTINATION ${SYSCONF_INSTALL_DIR}/libreport/workflows.d)

install(FILES java_event.conf.5 java_event_rhel.conf.5 report_rhel_java.conf.5 bugzilla_format_java.conf.5 bugzilla_formatdup_java.conf.5 java_event_fedora.conf.5 report_fedora_java.conf.5
        DESTINATION ${MAN_INSTALL_DIR}/man5)

install(FILES workflow_FedoraJava.xml workflow_RHELJava.xml
        DESTINATION ${SHARE_INSTALL_PREFIX}/libreport/workflows)

install(FILES bugzilla_format_java.conf bugzilla_formatdup_java.conf
        DESTINATION ${SYSCONF_INSTALL_DIR}/libreport/plugins)
