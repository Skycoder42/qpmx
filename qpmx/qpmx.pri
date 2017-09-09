qpmx_verbose: QPMX_EXTRA_GLOBAL_PARAMS += --verbose
qpmx_clean {
	QPMX_EXTRA_INSTALL_PARAMS += --renew
	QPMX_EXTRA_COMPILE_PARAMS += --recompile
	QPMX_EXTRA_GENERATE_PARAMS += --recreate
}

!system(qpmx -d $$shell_quote($$_PRO_FILE_PWD_) $$QPMX_EXTRA_GLOBAL_PARAMS install $$QPMX_EXTRA_INSTALL_PARAMS): error(qpmx install step failed)
!system(qpmx -d $$shell_quote($$_PRO_FILE_PWD_) $$QPMX_EXTRA_GLOBAL_PARAMS compile --qmake $$shell_quote($$QMAKE_QMAKE) $$QPMX_EXTRA_COMPILE_PARAMS): error(qpmx compile step failed)
!system(qpmx -d $$shell_quote($$_PRO_FILE_PWD_) $$QPMX_EXTRA_GLOBAL_PARAMS generate $$QPMX_EXTRA_GENERATE_PARAMS $$shell_quote($$OUT_PWD)): error(qpmx generate step failed)
include($$OUT_PWD/qpmx_generated.pri)
