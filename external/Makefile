SUBMODULES =					\
	external/libwebsockets			\
	external/dlg 				\
	external/iniparser

TOP := ../..
ifdef BUILD
CROSSCOMPILE_OPTS := --host="$(MAKE_HOST)" --build="$(BUILD)"
TARGET_DIR := external/$(MAKE_HOST)
else
TARGET_DIR := external/$(shell ${CC} -dumpmachine)
endif

UNAME_S := $(shell uname -s)
UNAME_P := $(shell uname -p)
ifeq ($(UNAME_S),Darwin)
	ifneq ($(filter arm%,$(UNAME_P)),)
        export OPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl
    endif
endif

# LIBWALLY_HEADERS := external/libwally-core/include/wally_bip32.h				\
# 			external/libwally-core/include/wally_core.h				\
# 			external/libwally-core/include/wally_psbt.h				\
# 			external/libwally-core/include/wally_crypto.h
# LIBSECP_HEADERS := external/libwally-core/src/secp256k1/include/secp256k1_ecdh.h		\
# 		external/libwally-core/src/secp256k1/include/secp256k1_recovery.h		\
# 		external/libwally-core/src/secp256k1/include/secp256k1.h
LIBWEBSOCKETS_HEADERS := external/libwebsockets/include/libwebsockets.h
DLG_HEADERS := external/dlg/include/dlg/dlg.h
INIPARSER_HEADERS := external/iniparser/src/dictionary.h \
		external/iniparser/src/iniparser.h

EXTERNAL_HEADERS := $(LIBWEBSOCKETS_HEADERS) $(DLG_HEADERS) $(INIPARSER_HEADERS)
EXTERNAL_LIBS := ${TARGET_DIR}/libwebsockets.a $(TARGET_DIR)/libdlg.a ${TARGET_DIR}/libiniparser.a

EXTERNAL_LIBS_CLEAN := ${TARGET_DIR}/libwebsockets.a $(TARGET_DIR)/libdlg.a ${TARGET_DIR}/libiniparser.a

EXTERNAL_INCLUDE_FLAGS :=					\
	-I external/libwebsockets/include/			\
	-I external/dlg/include/				\
	-I external/iniparser/src/

EXTERNAL_LDLIBS := -L${TARGET_DIR} $(patsubst lib%.a,-l%,$(notdir $(EXTERNAL_LIBS)))

submodcheck: 
	@tools/refresh-submodules.sh $(SUBMODULES)
	@patch -p0 --forward external/libwebsockets/include/libwebsockets/lws-lecp.h < tools/libwebsockets.patch || true

$(EXTERNAL_HEADERS): submodcheck

# libsecp included in libwally.
# Wildcards here are magic.  See http://stackoverflow.com/questions/2973445/gnu-makefile-rule-generating-a-few-targets-from-a-single-source-file
# $(TARGET_DIR)/libsecp256k1.% $(TARGET_DIR)/libwallycore.%: $(TARGET_DIR)/libwally-core-build/src/secp256k1/libsecp256k1.la $(TARGET_DIR)/libwally-core-build/src/libwallycore.la
# 	$(MAKE) -C $(TARGET_DIR)/libwally-core-build DESTDIR=$$(pwd)/$(TARGET_DIR) install-exec

# Build libwally-core.
# $(TARGET_DIR)/libwally-core-build/src/libwallycore.% $(TARGET_DIR)/libwally-core-build/src/secp256k1/libsecp256k1.%: $(LIBWALLY_HEADERS) $(LIBSECP_HEADERS)
# 	cd external/libwally-core && ./tools/autogen.sh
# 	mkdir -p ${TARGET_DIR}/libwally-core-build
# 	cd ${TARGET_DIR}/libwally-core-build \
# 	&& PYTHON_VERSION=3 CFLAGS=-std=c99 ${TOP}/libwally-core/configure CC="$(CC)" \
# 		--enable-static=yes \
# 		$(CROSSCOMPILE_OPTS) \
# 		--enable-module-recovery \
# 		--enable-module-extrakeys \
# 		--enable-module-schnorrsig \
# 		--enable-elements \
# 		--enable-shared=no \
# 		--prefix=/ \
# 		--libdir=/ \
# 		--enable-debug \
# 	&& $(MAKE)

external/dlg/src/dlg/dlg.c: external/dlg/include/dlg/dlg.h
	[ -f $@ ]

$(TARGET_DIR)/dlg-build/dlg.o: external/dlg/src/dlg/dlg.c Makefile
	@mkdir -p $(@D)
	$(COMPILE.c) -o $@ $<

$(TARGET_DIR)/libdlg.a: $(TARGET_DIR)/dlg-build/dlg.o
	$(AR) rc $@ $<

$(TARGET_DIR)/libiniparser.a:
	cd external/iniparser && $(MAKE) && cp -f libiniparser.a ../../$(TARGET_DIR)/

${TARGET_DIR}/libwebsockets.a: external/libwebsockets/include/libwebsockets.h
	mkdir -p $(TARGET_DIR)/libwebsockets-build
	cd $(TARGET_DIR)/libwebsockets-build && cmake -DLWS_WITH_HTTP2=1 -DLWS_WITH_NO_LOGS=ON $(TOP)/libwebsockets \
	&& $(MAKE) -j4 && cp -av lib/libwebsockets.a ..

distclean: external-distclean
clean: external-clean

external-clean:
	$(RM) $(EXTERNAL_LIBS_CLEAN) $(TARGET_DIR)/*.la $(TARGET_DIR)/*.o
	$(RM) $(TARGET_DIR)/dlg-build/dlg.o
	# Wipe the cmake build trees entirely (not just `make clean` inside them).
	# `make clean` keeps CMakeCache.txt, which caches negative results from
	# CHECK_FUNCTION_EXISTS probes (e.g. LWS_HAVE_HMAC_CTX_new). If the
	# system OpenSSL changes between configures, the stale "not found" entry
	# survives and the source falls through to the broken `HMAC_CTX ctx`
	# branch in lws-genhash.h, breaking the build. Removing the build dir
	# forces cmake to re-probe from scratch on the next make.
	$(RM) -rf ${TARGET_DIR}/libwebsockets-build

external-distclean:
	$(RM) -rf $(TARGET_DIR)/dlg-build
	$(RM) -rf ${TARGET_DIR}/libwebsockets-build
	$(info $$TARGET_DIR is [${TARGET_DIR}])
