PROJNAME := bet
CASHIER := cashierd
LIB := libbet

# Updated source and include paths
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
BIN_DIR := bin

EXTERNAL_DIR := ../external

# Updated source files path
SRCFILES := $(SRC_DIR)/cards.c  $(SRC_DIR)/client.c  $(SRC_DIR)/commands.c  $(SRC_DIR)/gfshare.c  \
            $(SRC_DIR)/host.c  $(SRC_DIR)/network.c  $(SRC_DIR)/oracle.c  $(SRC_DIR)/payment.c  \
            $(SRC_DIR)/states.c  $(SRC_DIR)/table.c $(SRC_DIR)/poker.c $(SRC_DIR)/cashier.c \
            $(SRC_DIR)/storage.c $(SRC_DIR)/config.c $(SRC_DIR)/misc.c $(SRC_DIR)/heartbeat.c \
            $(SRC_DIR)/help.c $(SRC_DIR)/err.c $(SRC_DIR)/vdxf.c $(SRC_DIR)/poker_vdxf.c \
            $(SRC_DIR)/player.c $(SRC_DIR)/blinder.c $(SRC_DIR)/dealer.c $(SRC_DIR)/print.c \
            $(SRC_DIR)/test.c $(SRC_DIR)/deck.c $(SRC_DIR)/game.c $(SRC_DIR)/dealer_registration.c

# Object files go in build directory
OBJFILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCFILES))

# Build a couple of small vendored deps directly into the binary
EXT_OBJFILES := $(BUILD_DIR)/dlg.o $(BUILD_DIR)/jsmn.o
OBJFILES += $(EXT_OBJFILES)

CC := gcc

UNAME_S := $(shell uname -s)
UNAME_P := $(shell uname -p)
ifeq ($(UNAME_S),Darwin)
WARNINGS := -Werror -Qunused-arguments
else
#WARNINGS := -Werror -Wunused-variable
endif

CFLAGS := -g -fPIC -std=c11 $(WARNINGS) -pthread \
          -I$(INCLUDE_DIR) \
          -I../libs/crypto \
          -I../includes \
          -I../includes/curl \
          -I$(EXTERNAL_DIR)/dlg/include \
          -I$(EXTERNAL_DIR)/iniparser/src \
          -I$(EXTERNAL_DIR)/jsmn \
          -I/usr/local/include \
          $(CPPFLAGS)

.PHONY: all clean test

LIGHTNING := lightning

all: $(BUILD_DIR) $(BIN_DIR) some_clean $(BIN_DIR)/$(PROJNAME) build_dir_tree

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

some_clean:
	$(RM) bet bet.o $(BIN_DIR)/bet $(BIN_DIR)/cashierd

clean:
	$(RM) -rf $(BUILD_DIR) $(BIN_DIR)
	$(RM) $(PROJNAME)	    
	$(RM) $(CASHIER)
	$(RM) $(LIB).a
	$(RM) *.o
	$(RM) $(SRC_DIR)/*.o

ifeq ($(UNAME_S),Darwin)
	ifneq ($(filter arm%,$(UNAME_P)),)
		export LDFLAGS+="-L/opt/homebrew/opt/openssl/lib"
		export CPPFLAGS+="-I/opt/homebrew/opt/openssl/include"
	endif
endif

$(info $$CC is [${CC}])

$(BIN_DIR)/$(PROJNAME): $(OBJFILES) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/bet.c $^ ../external/iniparser/libiniparser.a ../libs/crypto/libcrypto.a \
	$(LDFLAGS) $(CPPFLAGS) -lcurl -ldl -lpthread -lm -lwebsockets -lgmp -lsqlite3 -lssl -lcrypto -levent -lstdc++
	cp $(BIN_DIR)/$(PROJNAME) $(BIN_DIR)/$(CASHIER)

# Pattern rule to compile .c files to .o files in build directory
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/dlg.o: ../external/dlg/src/dlg/dlg.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/jsmn.o: ../external/jsmn/jsmn.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) -c $< -o $@

build_dir_tree:
	sh ./scripts/build_dir_tree.sh

# Add test target
test:
	$(MAKE) -C tests test
