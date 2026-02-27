-include config.cfg

DIR_RES ?= res
DIR_SCENARIO ?= scenario
DIR_MUSIC ?= music
DIR_SFX ?= sfx
DIR_SAVE ?= save

CXX = g++
CXXFLAGS = -std=c++17 -Wall -I$(HDR_DIR) \
           -DAPP_VERSION=\"$(APP_VERSION)\" \
           -DAPP_NAME=\"$(APP_NAME)\" \
           -DDEFAULT_LANG=\"$(DEFAULT_LANG)\" \
           -DDIR_RES=\"$(DIR_RES)\" \
           -DDIR_SCENARIO=\"$(DIR_SCENARIO)\" \
           -DDIR_MUSIC=\"$(DIR_MUSIC)\" \
           -DDIR_SFX=\"$(DIR_SFX)\" \
           -DDIR_SAVE=\"$(DIR_SAVE)\"

LDFLAGS = -lole32 -lwinmm -static -static-libgcc -static-libstdc++
TARGET = $(if $(TARGET_NAME),$(TARGET_NAME),game.exe)

SRC_DIR = cpp
HDR_DIR = hpp
OBJ_DIR = build

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))

all: prepare $(TARGET)

$(TARGET): $(OBJECTS)
	@echo [3/4] Linking: $@
	@$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo [4/4] Success! Project: $(APP_NAME) v$(APP_VERSION)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo Compiling: $<...
	@$(CXX) $(CXXFLAGS) -c $< -o $@

prepare:
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)

clean:
	@echo Cleaning...
	@if exist $(OBJ_DIR) rd /s /q $(OBJ_DIR)
	@if exist $(TARGET) del $(TARGET)

run: all
	@echo Running $(TARGET)...
	@./$(TARGET)

.PHONY: all prepare clean run