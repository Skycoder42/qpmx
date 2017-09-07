qpmx_verbose: QPMX_EXTRA_GLOBAL_PARAMS += --verbose
qpmx_clean {
	QPMX_EXTRA_INSTALL_PARAMS += --renew
	QPMX_EXTRA_COMPILE_PARAMS += --recompile
	QPMX_EXTRA_GENERATE_PARAMS += --recreate
}

system(qpmx -d $$shell_quote($$PWD) $$QPMX_EXTRA_GLOBAL_PARAMS install $$QPMX_EXTRA_INSTALL_PARAMS)
system(qpmx -d $$shell_quote($$PWD) $$QPMX_EXTRA_GLOBAL_PARAMS compile $$QPMX_EXTRA_COMPILE_PARAMS)
system(qpmx -d $$shell_quote($$PWD) $$QPMX_EXTRA_GLOBAL_PARAMS generate $$QPMX_EXTRA_GENERATE_PARAMS $$shell_quote($$OUT_PWD))
include($$OUT_PWD/qpmx_generated.pri)
