# go-game
hcmus cs160 apcs2025 project    

Requirements :
+ Compiler : 
MinGW-w64 (GCC 14+) OR other g++ distributions that support C++17
+ SFML version : 
SFML 3.0.0 (Included in repository - No download required)
+ Tools : 
CMake 3.16+
PowerShell or Git Bash
VSCode recommended (with CMake Tools extension)

Building the Game (Windows + MinGW)
1. Clone the repo
git clone https://github.com/Risa4224/go-game.git
cd go-game

2. Create build folder
mkdir build
cd build

3. Configure CMake using MinGW
cmake .. -G "MinGW Makefiles"

4. Build the executable
cmake --build .
(Note: This step will automatically compile the code and copy necessary SFML DLLs to the bin/ folder)

5. Run the Game
cd ..
.\bin\main.exe