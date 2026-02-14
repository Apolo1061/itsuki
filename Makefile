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
LDFLAGS = -lm

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = itsuki$(EXE)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

install: $(TARGET)
ifeq ($(OS),Windows_NT)
	@if not exist "$(INSTALL_DIR)" $(MKDIR) "$(INSTALL_DIR)"
	@copy /Y $(TARGET) "$(INSTALL_DIR)$(PATH_SEP)$(TARGET)"
	@echo $(TARGET) instalado en $(INSTALL_DIR)
	@echo.
	@echo IMPORTANTE: Agrega "$(INSTALL_DIR)" a tu PATH para usarlo desde cualquier lugar
	@echo Ejecuta como administrador: setx /M PATH "%PATH%;$(INSTALL_DIR)"
else
	sudo cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	sudo chmod 755 $(INSTALL_DIR)/$(TARGET)
	@echo "$(TARGET) instalado en $(INSTALL_DIR)"
endif

uninstall:
ifeq ($(OS),Windows_NT)
	@if exist "$(INSTALL_DIR)$(PATH_SEP)$(TARGET)" del /Q "$(INSTALL_DIR)$(PATH_SEP)$(TARGET)"
	@if exist "$(INSTALL_DIR)" rmdir "$(INSTALL_DIR)"
	@echo $(TARGET) desinstalado
	@echo.
	@echo NOTA: Debes remover "$(INSTALL_DIR)" de tu PATH manualmente si lo agregaste
else
	sudo $(RM) $(INSTALL_DIR)/$(TARGET)
	@echo "$(TARGET) desinstalado de $(INSTALL_DIR)"
endif

clean:
	-$(RMDIR) $(OBJ_DIR)
	-$(RM) $(TARGET)

.PHONY: all clean install uninstall
