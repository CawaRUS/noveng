-include config.cfg

DIR_RES ?= res
DIST_DIR ?= dist
SRC_DIR = cpp
HDR_DIR = hpp
OBJ_DIR = build

APP_NAME ?= NovEng
APP_VERSION ?= 0.5
TARGET_NAME ?= game.exe

DECRYPT ?= false
ASSET_KEY ?= $(NOVENG_ASSET_KEY)
OBFUSCATE ?= false

# Require a key when encryption is enabled.
ifeq ($(strip $(DECRYPT)),true)
ifeq ($(strip $(ASSET_KEY)),)
  $(error ASSET_KEY is required when DECRYPT=true. Set NOVENG_ASSET_KEY environment variable or ASSET_KEY in config.cfg.)
endif
endif

CXX = g++

CXXFLAGS = -std=c++17 -Wall -I$(HDR_DIR) -MMD -MP \
           -DAPP_VERSION=\"$(APP_VERSION)\" \
           -DAPP_NAME=\"$(APP_NAME)\" \
           -DDEFAULT_LANG=\"$(DEFAULT_LANG)\" \
           -DDIR_RES=\"$(DIR_RES)\" \
           -DDIR_SCENARIO=\"$(DIR_SCENARIO)\" \
           -DDIR_MUSIC=\"$(DIR_MUSIC)\" \
           -DDIR_SFX=\"$(DIR_SFX)\" \
           -DDIR_SAVE=\"$(DIR_SAVE)\" \
           -DUSE_CUSTOM_ABOUT=$(if $(filter true,$(USE_CUSTOM_ABOUT)),1,0) \
           -DUSE_DECRYPT=$(if $(filter true,$(strip $(DECRYPT))),1,0) \
           -DASSET_KEY=\"$(ASSET_KEY)\"

LDFLAGS = -lole32 -lwinmm -static -static-libgcc -static-libstdc++

CORE_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
CMD_SOURCES  = $(wildcard $(SRC_DIR)/cmds/*.cpp)

CORE_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CORE_SOURCES))
CMD_OBJECTS  = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CMD_SOURCES))
ALL_OBJECTS = $(CORE_OBJECTS) $(CMD_OBJECTS)

TARGET = $(DIST_DIR)/$(TARGET_NAME)
PACKER_SRC = scripts/packer.cpp
PACKER_EXE = novpack.exe

all: deploy

deploy: prepare $(PACKER_EXE) $(TARGET)

obfuscate_if_needed:
ifeq ($(OBFUSCATE),true)
	@echo [OBFUSCATE] Running code obfuscator...
	@python scripts/obfuscator.py --source cpp --headers hpp --output build/obfuscated
	@echo [OBFUSCATE] Complete! Using obfuscated sources.
else
	@echo [OBFUSCATE] Disabled. Using original sources.
endif

$(PACKER_EXE): $(PACKER_SRC) $(OBJ_DIR)/crypto_wrapper.o
	@echo [1/4] Building packer...
	@rm -f $(PACKER_EXE)
	@$(CXX) -std=c++17 -I$(HDR_DIR) $(PACKER_SRC) $(OBJ_DIR)/crypto_wrapper.o -o $(PACKER_EXE) $(LDFLAGS)

prepare:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/cmds
	@mkdir -p $(DIST_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp config.cfg
	@mkdir -p $(dir $@)
	@echo Compiling: $<...
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(ALL_OBJECTS)
	@echo [3/4] Linking Game: $@
	@$(CXX) $(ALL_OBJECTS) -o $@ $(LDFLAGS)

deploy:
	@echo [2/4] Deploying assets to $(DIST_DIR)...
	@mkdir -p $(DIST_DIR)/$(DIR_RES)
	@cp -r $(DIR_RES)/* $(DIST_DIR)/$(DIR_RES)/
ifeq ($(strip $(DECRYPT)),true)
	@echo [!] Encryption is ENABLED. Processing files...
	@$(PACKER_EXE) $(DIST_DIR)/$(DIR_RES) $(ASSET_KEY)
else
	@echo [?] Encryption is DISABLED. Files stay plain.
endif
	@echo [4/4] Build Complete!

clean:
	@echo Cleaning...
	@rm -rf $(OBJ_DIR)
	@rm -rf $(DIST_DIR)
	@rm -f $(PACKER_EXE)

run: all
	@echo Running $(TARGET_NAME)...
	@cd $(DIST_DIR) && ./$(TARGET_NAME)

-include $(ALL_OBJECTS:.o=.d)

.PHONY: all prepare clean run deploy
