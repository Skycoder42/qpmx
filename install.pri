INSTALL_PREFIX = $$(PREFIX)
isEmpty(INSTALL_PREFIX) {
	INSTALL_PREFIX = $$[QT_INSTALL_PREFIX]
	isEmpty($$INSTALL_BINS): INSTALL_BINS = $$[QT_INSTALL_BINS]
	isEmpty($$INSTALL_HEADERS): INSTALL_HEADERS = $$[QT_INSTALL_HEADERS]
	isEmpty($$INSTALL_PLUGINS): INSTALL_PLUGINS = $$[QT_INSTALL_PLUGINS]
} else {
	isEmpty($$INSTALL_BINS): INSTALL_BINS = $${INSTALL_PREFIX}/bin
	isEmpty($$INSTALL_HEADERS): INSTALL_HEADERS = $${INSTALL_PREFIX}/include
	isEmpty($$INSTALL_PLUGINS): INSTALL_PLUGINS = $${INSTALL_PREFIX}/plugins
}
isEmpty($$INSTALL_SHARE): INSTALL_SHARE = $${INSTALL_PREFIX}/share

message(Installing to prefix: $$INSTALL_PREFIX)
