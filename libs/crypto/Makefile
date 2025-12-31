# Crypto library Makefile
CC ?= gcc
AR ?= ar
CFLAGS = -w -O2 -fPIC -I ../../includes -I ../../external/nng/include

# All source files
SRCFILES = curve25519.c curve25519-donna.c tweetnacl.c scrypt.c \
           OS_portable.c OS_time.c OS_nonportable.c iguana_OS.c \
           iguana_serdes.c iguana_utils.c inet.c cJSON.c \
           bitcoind_RPC.c SaM.c

OBJFILES = $(SRCFILES:.c=.o)
LIBNAME = libcrypto.a

.PHONY: all clean

all: $(LIBNAME)

$(LIBNAME): $(OBJFILES)
	$(AR) rcu $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJFILES)
	$(RM) -f $(LIBNAME)
