# go-game
hcmus cs160 apcs2025 project    

Requirements :
+ Compiler : 
MinGW-w64 (GCC 14+) OR other g++ distributions that support C++17
+ SFML version : 
SFML 3.0.0 MinGW build , Download SFML 3.0.0 from: https://www.sfml-dev.org/download.php
+ Tools : 
CMake 3.16+
PowerShell or Git Bash
VSCode recommended (with CMake Tools extension)

Building the Game (Windows + MinGW)
1. Clone the repo
git clone https://github.com/yourname/go-game.git
cd go-game

2. Create build folder
mkdir build
cd build

3. Configure CMake using MinGW
cmake .. -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++

4. Build the executable
cmake --build .

5. Copy SFML DLLs into bin/
copy all DLLs in your SFML-3.0.0 into bin/

cd ..
.\bin\main.exe
