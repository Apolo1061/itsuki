ifeq ($(OS),Windows_NT)
    RM = del /Q
    RMDIR = rmdir /s /q
    MKDIR = mkdir
    EXE = .exe
    INSTALL_DIR = C:\Program Files\itsuki
    PATH_SEP = \\
else
    RM = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    EXE =
    INSTALL_DIR = /usr/bin
    PATH_SEP = /
endif

CC = gcc
CFLAGS = -I./src/headers -O3 -Wall
ifeq ($(OS),Windows_NT)
    LDFLAGS = -lm -lws2_32
    MODULE_EXT = .dll
    MODULE_SHARED_FLAGS = -shared
    MODULE_LIBS = -lm -lws2_32
else
    LDFLAGS = -lm -ldl
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        MODULE_EXT = .dylib
    else
        MODULE_EXT = .so
    endif
    MODULE_SHARED_FLAGS = -shared -fPIC
    MODULE_LIBS = -lm -lpthread
endif

SRC_DIR = src
OBJ_DIR = obj
MODULES_DIR = modules

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
MODULE_SRCS = $(wildcard $(MODULES_DIR)/*.c)
MODULE_TARGETS = $(patsubst $(MODULES_DIR)/%.c, $(MODULES_DIR)/%$(MODULE_EXT), $(MODULE_SRCS))
TARGET = itsuki$(EXE)
TARGET_TOOL = itsuki-cy$(EXE)
SRC_TOOL = tools/itsuki-cy.c

all: $(TARGET) $(TARGET_TOOL)

modules-build: $(MODULE_TARGETS)

$(MODULES_DIR)/%$(MODULE_EXT): $(MODULES_DIR)/%.c
	$(CC) $(MODULE_SHARED_FLAGS) $(CFLAGS) $< -o $@ $(MODULE_LIBS)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(TARGET_TOOL): $(SRC_TOOL)
	$(CC) -O3 -Wall $(SRC_TOOL) -o $(TARGET_TOOL)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

install: $(TARGET) $(TARGET_TOOL) modules-build
ifeq ($(OS),Windows_NT)
	@if not exist "$(INSTALL_DIR)" $(MKDIR) "$(INSTALL_DIR)"
	@if exist "$(INSTALL_DIR)$(PATH_SEP)$(TARGET)" (echo Actualizando $(TARGET)... & del /Q "$(INSTALL_DIR)$(PATH_SEP)$(TARGET)")
	@if exist "$(INSTALL_DIR)$(PATH_SEP)$(TARGET_TOOL)" (echo Actualizando $(TARGET_TOOL)... & del /Q "$(INSTALL_DIR)$(PATH_SEP)$(TARGET_TOOL)")
	@copy /Y $(TARGET) "$(INSTALL_DIR)$(PATH_SEP)$(TARGET)" > nul
	@copy /Y $(TARGET_TOOL) "$(INSTALL_DIR)$(PATH_SEP)$(TARGET_TOOL)" > nul
	@echo.
	@echo [OK] $(TARGET) y $(TARGET_TOOL) se han instalado correctamente en $(INSTALL_DIR)
	@echo.
	@echo IMPORTANTE: Si es la primera vez, agrega "$(INSTALL_DIR)" a tu PATH:
	@echo Ejecuta como administrador: setx /M PATH "%PATH%;$(INSTALL_DIR)"
else
	@echo "Detectando versiones previas en Linux..."
	sudo rm -f $(INSTALL_DIR)/$(TARGET)
	sudo rm -f $(INSTALL_DIR)/$(TARGET_TOOL)
	sudo cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	sudo cp $(TARGET_TOOL) $(INSTALL_DIR)/$(TARGET_TOOL)
	sudo chmod 755 $(INSTALL_DIR)/$(TARGET)
	sudo chmod 755 $(INSTALL_DIR)/$(TARGET_TOOL)
	@echo "[OK] $(TARGET) y $(TARGET_TOOL) instalados en $(INSTALL_DIR)"
endif

uninstall:
ifeq ($(OS),Windows_NT)
	@if exist "$(INSTALL_DIR)$(PATH_SEP)$(TARGET)" del /Q "$(INSTALL_DIR)$(PATH_SEP)$(TARGET)"
	@if exist "$(INSTALL_DIR)$(PATH_SEP)$(TARGET_TOOL)" del /Q "$(INSTALL_DIR)$(PATH_SEP)$(TARGET_TOOL)"
	@if exist "$(INSTALL_DIR)" rmdir "$(INSTALL_DIR)"
	@echo Desinstalado
else
	sudo $(RM) $(INSTALL_DIR)/$(TARGET)
	sudo $(RM) $(INSTALL_DIR)/$(TARGET_TOOL)
	@echo "Desinstalado de $(INSTALL_DIR)"
endif

clean:
	-$(RMDIR) $(OBJ_DIR)
	-$(RM) $(TARGET)
	-$(RM) $(TARGET_TOOL)
	-$(RM) $(MODULE_TARGETS)

test: $(TARGET)
	python3 tests/runner.py

.PHONY: all clean install uninstall test modules-build
