libmod_poc_rest_static.la: mod_poc_rest_static.lo
	$(MOD_LINK) mod_poc_rest_static.lo $(MOD_POC_REST_STATIC_LDADD)
DISTCLEAN_TARGETS = modules.mk
static =  libmod_poc_rest_static.la
shared = 
