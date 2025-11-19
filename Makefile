CXX       := g++
CXX_FLAGS := -Wall -Wextra -Wno-unused-parameter -std=c++17 -O2

BIN_DIR   := bin
SRC_DIR   := src
INC_DIR   := include
LIB_DIR   := lib

SFML_INC  := C:/SFML-3.0.0/include
SFML_LIB  := C:/SFML-3.0.0/lib

LIBRARIES := -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
EXECUTABLE := main

MKDIR_P = mkdir -p

# Find all .cpp files
SRC := $(wildcard $(SRC_DIR)/*.cpp)
# Map src/xxx.cpp -> build/xxx.o
OBJ_DIR := build
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))

.PHONY: all run clean directories

all: directories $(BIN_DIR)/$(EXECUTABLE)

directories: $(BIN_DIR) $(OBJ_DIR)

$(BIN_DIR):
	$(MKDIR_P) $(BIN_DIR)

$(OBJ_DIR):
	$(MKDIR_P) $(OBJ_DIR)

# Link step: uses all .o files
$(BIN_DIR)/$(EXECUTABLE): $(OBJ)
	$(CXX) $(CXX_FLAGS) -I$(SFML_INC) -L$(SFML_LIB) -I$(INC_DIR) -L$(LIB_DIR) $^ -o $@ $(LIBRARIES)

# Compile step: one .cpp -> one .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXX_FLAGS) -I$(SFML_INC) -I$(INC_DIR) -c $< -o $@

# Build + run (no clean!)
run: all
	./$(BIN_DIR)/$(EXECUTABLE)

clean:
	-rm -rf $(BIN_DIR) $(OBJ_DIR)