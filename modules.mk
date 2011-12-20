mod_poc_rest.la: mod_poc_rest.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version  mod_poc_rest.lo
DISTCLEAN_TARGETS = modules.mk
shared =  mod_poc_rest.la
