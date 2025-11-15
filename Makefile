CXX		  := g++
CXX_FLAGS := -Wall -Wextra -Wno-unused-parameter -std=c++17 -ggdb

BIN		:= bin
SRC		:= src
INCLUDE	:= include
LIB		:= lib

SFML_INC := C:/SFML-3.0.0/include
SFML_LIB := C:/SFML-3.0.0/lib

LIBRARIES	:= -lsfml-graphics -lsfml-window -lsfml-system
EXECUTABLE	:= main

MKDIR_P = mkdir -p

.PHONY: directories

all: directories $(BIN)/$(EXECUTABLE)

directories: ${BIN}

${BIN}:
	${MKDIR_P} ${BIN}

run: clean all
	clear
	./$(BIN)/$(EXECUTABLE)

$(BIN)/$(EXECUTABLE): $(SRC)/*.cpp
	$(CXX) $(CXX_FLAGS) -I$(SFML_INC) -L$(SFML_LIB) -I$(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES)

clean:
	-rm $(BIN)/*