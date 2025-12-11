#include "game.h"
#include <iostream>
#include <cmath> 
#include <fstream>
#include <queue>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

using namespace std;

Game::~Game() {
    delete board;
}

Game::Game(const Game& other)
    : black_captures(other.black_captures),
      white_captures(other.white_captures),
      consecutive_passes(other.consecutive_passes),
      board(new Board(*(other.board))),
      turn(other.turn),
      groups(other.groups),
      // history và future cố tình KHÔNG copy để tránh deep copy nặng
      m_lastCaptures(other.m_lastCaptures),
      m_lastInvalid(other.m_lastInvalid),
      m_lastSuicide(other.m_lastSuicide),
      m_lastKoViolation(other.m_lastKoViolation),
      m_lastKoThreat(other.m_lastKoThreat)
{
    // history và future mặc định rỗng cho mỗi bản copy (snapshot)
}


Game& Game::operator=(const Game& other) {
    if (this != &other) { 
        
        delete this->board; 
        
        this->board = new Board(*(other.board));

        this->turn = other.turn;
        this->groups = other.groups;
        this->black_captures = other.black_captures;
        this->white_captures = other.white_captures;
        this->consecutive_passes = other.consecutive_passes;
    }
    return *this;
}

Game::Game(Board* b) : board(b), turn(BLACK) {}


PieceColor Game::oppositeColor(PieceColor input) const {
    if(input == BLACK) return WHITE;
    else if(input == WHITE) return BLACK;
    else return NONE;
}

PieceColor Game::getPiece(int x, int y) const {
    return board->getPiece(x, y);
}



bool Game::valid(int x, int y) const{
    if(x < 0 || x >= 19 || y < 0 || y >= 19) return false;
    //std::cout << "Checking validity for (" << x << ", " << y << "): ";
    // if(board->getPiece(x,y) != NONE) {
    //     std::cout << "Spot occupied." << std::endl;
    //     return false;
    // }
    if(board->getPiece(x,y) != NONE) return false;
    return true;
}

void Game::processGroups(int x, int y, PieceColor c) {
    if(groups.size() <= 0) groups.push_back(PieceGroup(x, y, c));

    else {
        std::vector<int> connections; 

        for(unsigned int i = 0; i < groups.size(); ++i) {
            if(groups[i].getColor() == c && groups[i].isConnected(x, y)) {
                connections.push_back(i);
            }
        }

        if(connections.size() <= 0) groups.push_back(PieceGroup(x, y, c));

        else {
            PieceGroup temp = PieceGroup(c); 
            temp.addPiece(x,y); 

            for(std::vector<int>::iterator it = connections.begin(); it != connections.end(); ++it) {
                temp = groups[*it].combine(temp);
            }

            for(int i = connections.size()-1; i >= 0; --i) {
                groups.erase(groups.begin() + connections[i]);
            }

            groups.push_back(temp);
        }
    }
}

int Game::checkCaptures(int x, int y, PieceColor c) {
    int total_captures = 0;
    //std::cout << "Checking captures around (" << x << ", " << y << "):\n";
    if(board->getPiece(x-1,y) == oppositeColor(c)) {
        //std::cout << x - 1 << ' ' << y << '\n';
        //std::cout << findGroupIndex(x-1,y) << '\n';
        int groupIdx = findGroupIndex(x-1,y);
        if(groupIdx != -1){
            int tmp = calcLiberties(groupIdx);
            if(tmp == 0) removeGroup(x-1,y, total_captures);
        }
    }
    if(board->getPiece(x+1,y) == oppositeColor(c)) {
        //std:: cout << x + 1 << ' ' << y << '\n';
        //std:: cout << findGroupIndex(x+1,y) << '\n';
        int groupIdx = findGroupIndex(x+1,y);
        if(groupIdx != -1){
            int tmp = calcLiberties(groupIdx);
            if(tmp == 0) removeGroup(x+1,y, total_captures);
        }
    }
    if(board->getPiece(x,y-1) == oppositeColor(c)) {
        //std:: cout << x << ' ' << y - 1 << '\n';       
       // std:: cout << findGroupIndex(x,y-1) << '\n';
        int groupIdx = findGroupIndex(x,y-1);
        if (groupIdx != -1){
            int tmp = calcLiberties(groupIdx);
            if(tmp == 0) removeGroup(x,y-1, total_captures);
        }
    }
    if(board->getPiece(x,y+1) == oppositeColor(c)) {
        //std:: cout << x << ' ' << y + 1 << '\n';
        //std:: cout << findGroupIndex(x,y+1) << '\n';
        int groupIdx = findGroupIndex(x,y+1);
        if(groupIdx != -1){
            int tmp = calcLiberties(groupIdx);
            if(tmp == 0) removeGroup(x,y+1, total_captures);
        }
    }
    return total_captures;
}

int Game::findGroupIndex(int x, int y) {
    for(int i = 0; i < (int)groups.size(); ++i) {
        if(groups[i].contains(x, y)) {
            //std::cout << "Found group at index " << i << " for piece (" << x << ", " << y << ")\n";
            return i;
        }
    }
    return -1;
}

void Game::removeGroup(int x, int y, int &a) {
    int ind = -1;
    for(int i = 0; i < (int)groups.size(); ++i) {
        if(groups[i].contains(x, y)) {
            ind = i;
            break;
        }
    }

    if(ind == -1) {
        std::cout << "SOMETHING WENT WRONG: removeGroup failed to find group" << std::endl;
        return;
    }

    int tempx, tempy;
    for(int i = 0; i < groups[ind].getSize(); ++i) {
        int location = groups[ind].getLocation(i);
        tempx = location / 19;
        tempy = location % 19;
        a++;
        //std::cout << "Removing piece at (" << tempx << ", " << tempy << ")\n";
        board->removePiece(tempx, tempy); 
    }
    groups.erase(groups.begin() + ind);
}

int Game::calcLiberties(int id){ 
    if (id < 0 || id >= (int)groups.size()) {
        return 0; 
    }
    
    std::set<int> unique_liberties;
    PieceGroup& current_group = groups[id];

    int tempx, tempy;
    for(int i = 0; i < current_group.getSize(); ++i) {
        int location = current_group.getLocation(i);
        tempx = location / 19;
        tempy = location % 19;
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};

        for (int k = 0; k < 4; ++k) {
            int nx = tempx + dx[k];
            int ny = tempy + dy[k];
            //std::cout << "Calculating liberties for piece at (" << nx << ", " << ny << ")\n";
            if(valid(nx, ny)) {
                unique_liberties.insert(nx * 19 + ny);
            }
        }
    }
    //std::cout << "Total liberties for group " << id << ": " << unique_liberties.size() << std::endl;
    return unique_liberties.size();
}

bool Game::ended(int x, int y){
    if(x < 0 || y < 0) return true;
    return false;
}

bool Game::placeStone(int x, int y) {
    // --- reset last-move info ---
    m_lastCaptures    = 0;
    m_lastInvalid     = false;
    m_lastSuicide     = false;
    m_lastKoViolation = false;
    m_lastKoThreat    = false;

    if(!valid(x, y)){
        std::cout << "Invalid move: spot already occupied or out of bounds." << endl;
        m_lastInvalid = true;
        return false;
    }
    
    PieceColor current_color = turn;
    std::vector<PieceGroup> groups_backup = groups;
    Board temp_board_backup = *board; 
    int black_captures_backup = black_captures;
    int white_captures_backup = white_captures;

    board->setPiece(x, y, current_color); 
    processGroups(x, y, current_color); 
    int placed_group_id = findGroupIndex(x, y); 
    int captures = checkCaptures(x, y, current_color);

    int temp_black_captures = black_captures + ((current_color == BLACK) ? captures : 0);
    int temp_white_captures = white_captures + ((current_color == WHITE) ? captures : 0);
    
    bool is_suicide = (captures == 0 && calcLiberties(placed_group_id) == 0);
    bool is_ko_violation = false;
    
    if (history.size() >= 2) {
         const Board* board_i_minus_2 = history[history.size() - 1].board;
         if (this->board->isEqual(*board_i_minus_2)) { 
             is_ko_violation = true;
         }
    }

    if(is_suicide || is_ko_violation) {
        // restore
        groups = groups_backup;         
        *board = temp_board_backup; 
                
        if (is_suicide) {
            std::cout << "Invalid move: Suicide." << endl;
            m_lastSuicide = true;
        }
        if (is_ko_violation) {
            std::cout << "Invalid move: Ko Rule violation." << endl;
            m_lastKoViolation = true;
        }
        
        return false; 
    }
    
    // --- move is valid: record info for UI ---
    m_lastCaptures = captures;
    // Very simple heuristic: capture exactly 1 stone often = Ko shape
    m_lastKoThreat = (captures == 1);

    Game state_to_save = *this;

    *(state_to_save.board) = temp_board_backup; 
    state_to_save.groups = groups_backup;
    state_to_save.black_captures = black_captures_backup;
    state_to_save.white_captures = white_captures_backup;
    state_to_save.turn = current_color; 

    history.push_back(state_to_save); 
    
    black_captures = temp_black_captures;
    white_captures = temp_white_captures;

    future.clear(); 
    turn = oppositeColor(turn); 
    std::cout << white_captures << ' ' << black_captures << '\n';
    consecutive_passes = 0; 
    return true;
}


// Debug function
void Game::printDebug() const {
    cout << "--- GAME STATE ---" << endl;
    cout << "Current Turn: " << ((turn == BLACK) ? "BLACK (2)" : "WHITE (1)") << endl;
    cout << "Num groups: " << groups.size() << endl;
    
    board->printDebug();
    
    cout << "--- GROUP DETAILS ---" << endl;
    for(const auto& group : groups) {
        cout << "Color: " << group.getColor() << ", Size: " << group.getSize() << ", Locations: ";
        group.printSelf();
        cout << endl;
    }
    cout << "--------------------" << endl;
}

bool Game::pass() {
    history.push_back(*this);

    ++consecutive_passes;

    if (consecutive_passes >= 2) {
        std::cout << "Both players passed consecutively. The game has ended.\n";
        // Just signal that the game ended. DO NOT call calculateFinalScore here.
        return true;
    }

    std::cout << "PASS\n";
    turn = oppositeColor(turn);
    future.clear();

    return false;
}



bool Game::undo() {
    if (history.empty()) {
        std::cout << "Cannot undo: No previous states." << std::endl;
        return false;
    }
    
    Game prevState = history.back(); 
    history.pop_back(); 
    
    future.push_back(*this); 
    
    *this = prevState; 
    
    std::cout << "Undo successful. Game state reverted to previous turn." << std::endl;
    return true;
}
bool Game::redo() {
    if (future.empty()) {
        std::cout << "Cannot redo: No future states." << std::endl; 
        return false;
    }
    
    Game nextState = future.back(); 
    future.pop_back(); 
    
    history.push_back(*this); 
    
    *this = nextState; 

    std::cout << "Redo successful. Game state moved forward." << std::endl;
    return true;
}


// Hàm BFS bắt đầu từ (startX, startY) – một ô trống.
// Trả về:
//   - BLACK  nếu vùng trống chỉ chạm quân đen
//   - WHITE  nếu vùng trống chỉ chạm quân trắng
//   - NONE   nếu vùng trống chạm cả hai màu hoặc là vùng trung lập
// Đồng thời gán 'territory_size' = số ô trống trong vùng đó.
PieceColor Game::getTerritoryOwner(
    int startX,
    int startY,
    int& territory_size,
    std::vector<std::vector<bool>>& visited) const
{
    std::queue<std::pair<int, int>> q;
    q.push({ startX, startY });
    visited[startX][startY] = true;

    territory_size = 0;
    bool touchesBlack = false;
    bool touchesWhite = false;

    const int dx[4] = { -1, 1, 0, 0 };
    const int dy[4] = { 0, 0, -1, 1 };

    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();

        ++territory_size;   // mỗi ô trống trong vùng

        // Kiểm tra 4 hướng
        for (int dir = 0; dir < 4; ++dir) {
            int nx = x + dx[dir];
            int ny = y + dy[dir];

            // Ra ngoài bàn cờ thì bỏ qua
            if (nx < 0 || nx >= 19 || ny < 0 || ny >= 19)
                continue;

            PieceColor c = board->getPiece(nx, ny);

            if (c == NONE) {
                // Mở rộng BFS nếu là ô trống và chưa thăm
                if (!visited[nx][ny]) {
                    visited[nx][ny] = true;
                    q.push({ nx, ny });
                }
            }
            else if (c == BLACK) {
                touchesBlack = true;
            }
            else if (c == WHITE) {
                touchesWhite = true;
            }
        }
    }

    if (touchesBlack && !touchesWhite) return BLACK;
    if (touchesWhite && !touchesBlack) return WHITE;
    return NONE; // vùng chạm cả hai màu hoặc không chạm màu nào => trung lập
}


std::pair<float, float> Game::calculateFinalScore(float komi) const
{
    const int size = board->getSize();  // 19 cho bàn 19x19
    std::vector<std::vector<bool>> visited(
        size, std::vector<bool>(size, false));

    int black_territory = 0;
    int white_territory = 0;

    // Dò toàn bộ ô trống trên bàn, gom thành vùng và xác định chủ sở hữu
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            if (board->getPiece(x, y) == NONE && !visited[x][y]) {
                int territory_size = 0;
                PieceColor owner = getTerritoryOwner(x, y, territory_size, visited);

                if (owner == BLACK) {
                    black_territory += territory_size;
                }
                else if (owner == WHITE) {
                    white_territory += territory_size;
                }
                // owner == NONE => vùng trung lập, không cộng cho ai
            }
        }
    }

    // Tính điểm cuối cùng (kiểu Nhật: territory + captures + komi cho trắng)
    float black_score = static_cast<float>(black_territory + black_captures);
    float white_score = static_cast<float>(white_territory + white_captures) + komi;

    // In ra console để debug (nếu cần, có thể comment đi)
    std::cout << "\n===== FINAL SCORE =====\n";
    std::cout << "Black territory : " << black_territory
              << " | captures: " << black_captures
              << " | total: " << black_score << "\n";
    std::cout << "White territory : " << white_territory
              << " | captures: " << white_captures
              << " | komi: " << komi
              << " | total: " << white_score << "\n";

    if (black_score > white_score) {
        std::cout << "=> Black wins by "
                  << (black_score - white_score) << " points.\n";
    }
    else if (white_score > black_score) {
        std::cout << "=> White wins by "
                  << (white_score - black_score) << " points.\n";
    }
    else {
        std::cout << "=> Draw.\n";
    }

    std::cout << "========================\n";

    return { black_score, white_score };
}


bool Game::saveToFile(const std::string& filename) const
{
    std::ofstream out(filename);
    if (!out.is_open())
    {
        std::cerr << "Failed to open save file: " << filename << std::endl;
        return false;
    }

    const int BOARD_SIZE = 19; // đồng bộ với phần còn lại

    // 1) Header đơn giản: kích thước board
    out << BOARD_SIZE << '\n';

    // 2) Metadata cho trạng thái hiện tại
    out << static_cast<int>(turn) << ' '
        << black_captures << ' '
        << white_captures << ' '
        << consecutive_passes << '\n';

    // 3) Bàn cờ hiện tại (B/W/.)
    for (int y = 0; y < BOARD_SIZE; ++y)
    {
        for (int x = 0; x < BOARD_SIZE; ++x)
        {
            PieceColor p = board->getPiece(x, y);
            char c = '.';
            if (p == BLACK)      c = 'B';
            else if (p == WHITE) c = 'W';
            out << c;
        }
        out << '\n';
    }

    // 4) Lưu history (toàn bộ timeline phía trước)
    out << history.size() << '\n';
    for (const Game& st : history)
    {
        out << static_cast<int>(st.turn) << ' '
            << st.black_captures << ' '
            << st.white_captures << ' '
            << st.consecutive_passes << '\n';

        for (int y = 0; y < BOARD_SIZE; ++y)
        {
            for (int x = 0; x < BOARD_SIZE; ++x)
            {
                PieceColor p = st.board->getPiece(x, y);
                char c = '.';
                if (p == BLACK)      c = 'B';
                else if (p == WHITE) c = 'W';
                out << c;
            }
            out << '\n';
        }
    }

    // 5) Lưu future (các trạng thái có thể redo)
    out << future.size() << '\n';
    for (const Game& st : future)
    {
        out << static_cast<int>(st.turn) << ' '
            << st.black_captures << ' '
            << st.white_captures << ' '
            << st.consecutive_passes << '\n';

        for (int y = 0; y < BOARD_SIZE; ++y)
        {
            for (int x = 0; x < BOARD_SIZE; ++x)
            {
                PieceColor p = st.board->getPiece(x, y);
                char c = '.';
                if (p == BLACK)      c = 'B';
                else if (p == WHITE) c = 'W';
                out << c;
            }
            out << '\n';
        }
    }

    out.close();
    return true;
}


bool Game::loadFromFile(const std::string& filename)
{
    std::ifstream in(filename);
    if (!in.is_open())
    {
        std::cerr << "Failed to open save file: " << filename << std::endl;
        return false;
    }

    const int BOARD_SIZE_EXPECTED = 19;
    int boardSize = 0;

    if (!(in >> boardSize))
    {
        std::cerr << "Invalid save file (cannot read board size).\n";
        return false;
    }

    if (boardSize != BOARD_SIZE_EXPECTED)
    {
        std::cerr << "Unsupported board size in save file.\n";
        return false;
    }

    int turnInt = 0;
    if (!(in >> turnInt >> black_captures >> white_captures >> consecutive_passes))
    {
        std::cerr << "Invalid save file metadata.\n";
        return false;
    }

    std::string line;
    std::getline(in, line); // ăn nốt phần newline

    // 1) Đọc bàn cờ hiện tại
    for (int y = 0; y < boardSize; ++y)
    {
        if (!std::getline(in, line) || (int)line.size() < boardSize)
        {
            std::cerr << "Invalid board row in save file.\n";
            return false;
        }

        for (int x = 0; x < boardSize; ++x)
        {
            char c = line[x];
            PieceColor p = NONE;
            if (c == 'B')      p = BLACK;
            else if (c == 'W') p = WHITE;

            board->setPiece(x, y, p);
        }
    }

    rebuildGroupsFromBoard();
    turn = static_cast<PieceColor>(turnInt);

    std::size_t historySize = 0;
    if (!(in >> historySize))
    {
        history.clear();
        future.clear();
        return true;
    }
    std::getline(in, line); 

    history.clear();
    history.reserve(historySize);

    for (std::size_t idx = 0; idx < historySize; ++idx)
    {
        int hTurnInt = 0;
        int hBlack = 0, hWhite = 0, hPasses = 0;
        if (!(in >> hTurnInt >> hBlack >> hWhite >> hPasses))
        {
            std::cerr << "Invalid history metadata in save file.\n";
            return false;
        }
        std::getline(in, line);
        Board* snapBoard = new Board(*board);

        for (int y = 0; y < boardSize; ++y)
        {
            if (!std::getline(in, line) || (int)line.size() < boardSize)
            {
                std::cerr << "Invalid history board row.\n";
                delete snapBoard;
                return false;
            }

            for (int x = 0; x < boardSize; ++x)
            {
                char c = line[x];
                PieceColor p = NONE;
                if (c == 'B')      p = BLACK;
                else if (c == 'W') p = WHITE;

                snapBoard->setPiece(x, y, p);
            }
        }

        Game snapshot(snapBoard);
        snapshot.turn              = static_cast<PieceColor>(hTurnInt);
        snapshot.black_captures    = hBlack;
        snapshot.white_captures    = hWhite;
        snapshot.consecutive_passes = hPasses;

        snapshot.history.clear();
        snapshot.future.clear();
        snapshot.rebuildGroupsFromBoard();

        history.push_back(snapshot);
    }

    std::size_t futureSize = 0;
    if (!(in >> futureSize))
    {
        future.clear();
        return true;
    }
    std::getline(in, line); // newline

    future.clear();
    future.reserve(futureSize);

    for (std::size_t idx = 0; idx < futureSize; ++idx)
    {
        int fTurnInt = 0;
        int fBlack = 0, fWhite = 0, fPasses = 0;
        if (!(in >> fTurnInt >> fBlack >> fWhite >> fPasses))
        {
            std::cerr << "Invalid future metadata in save file.\n";
            return false;
        }
        std::getline(in, line); // newline

        Board* snapBoard = new Board(*board);

        for (int y = 0; y < boardSize; ++y)
        {
            if (!std::getline(in, line) || (int)line.size() < boardSize)
            {
                std::cerr << "Invalid future board row.\n";
                delete snapBoard;
                return false;
            }

            for (int x = 0; x < boardSize; ++x)
            {
                char c = line[x];
                PieceColor p = NONE;
                if (c == 'B')      p = BLACK;
                else if (c == 'W') p = WHITE;

                snapBoard->setPiece(x, y, p);
            }
        }

        Game snapshot(snapBoard);
        snapshot.turn               = static_cast<PieceColor>(fTurnInt);
        snapshot.black_captures     = fBlack;
        snapshot.white_captures     = fWhite;
        snapshot.consecutive_passes = fPasses;

        snapshot.history.clear();
        snapshot.future.clear();
        snapshot.rebuildGroupsFromBoard();

        future.push_back(snapshot);
    }

    in.close();
    return true;
}

bool Game::saveNamed(const std::string& name) const
{
    std::error_code ec;

    fs::path dir(Game::SAVE_DIR);
    fs::create_directories(dir, ec); 

    fs::path path = dir / name;
    if (!path.has_extension()) {
        path.replace_extension(".go");
    }

    return saveToFile(path.string());
}

bool Game::loadNamed(const std::string& name)
{
    fs::path path(Game::SAVE_DIR);
    path /= name;
    if (!path.has_extension()) {
        path.replace_extension(".go");
    }

    return loadFromFile(path.string());
}

bool Game::saveToNewSlot(std::string& outFilename) const
{
    std::error_code ec;
    fs::path dir(Game::SAVE_DIR);
    fs::create_directories(dir, ec);

    int maxIndex = 0;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string stem = entry.path().stem().string(); // ví dụ "save_001"
        if (stem.rfind("save_", 0) == 0) {               // bắt đầu bằng "save_"
            std::string numStr = stem.substr(5);         // phần sau "save_"
            try {
                int idx = std::stoi(numStr);
                if (idx > maxIndex) maxIndex = idx;
            } catch (...) {
                // tên file không theo dạng save_X => bỏ qua
            }
        }
    }

    int newIndex = maxIndex + 1;

    std::ostringstream oss;
    oss << "save_" << std::setfill('0') << std::setw(3) << newIndex; 

    fs::path path = dir / (oss.str() + ".go");

    outFilename = path.filename().string();

    if (!saveToFile(path.string())) {
        outFilename.clear();
        return false;
    }

    return true;
}

std::vector<std::string> Game::listSaveFiles()
{
    std::vector<std::string> result;

    std::error_code ec;
    fs::path dir(Game::SAVE_DIR);

    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return result;
    }

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        // Lọc theo extension bạn dùng cho save
        if (ext == ".go" || ext == ".txt" || ext == ".sav") {
            result.push_back(entry.path().filename().string());
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

void Game::rebuildGroupsFromBoard()
{
    groups.clear();

    // 19x19 – đúng với phần còn lại của code em
    const int BOARD_SIZE = 19;

    for (int y = 0; y < BOARD_SIZE; ++y)
    {
        for (int x = 0; x < BOARD_SIZE; ++x)
        {
            PieceColor p = board->getPiece(x, y);
            if (p != NONE)
            {
                // Dùng lại logic nhóm đã có
                processGroups(x, y, p);
            }
        }
    }
}
