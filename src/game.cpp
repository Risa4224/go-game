// game.cpp (FILE MỚI)
#include "game.h"
#include <iostream>
#include <cmath> 
#include <set>
#include <fstream>


using namespace std;

Game::~Game() {
    delete board;
}

Game::Game(const Game& other) 
    : board(new Board(*(other.board))), 
      turn(other.turn), 
      groups(other.groups)
{
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
    if(!valid(x, y)){
        std::cout << "Invalid move: spot already occupied or out of bounds." << endl;
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
        
        groups = groups_backup;         
        *board = temp_board_backup; 
                
        if (is_suicide) std::cout << "Invalid move: Suicide." << endl;
        if (is_ko_violation) std::cout << "Invalid move: Ko Rule violation." << endl;
        
        return false; 
    }
    
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


PieceColor Game::getTerritoryOwner(int x, int y, std::set<int>& territory_set) const {
    
    // 1. Kiểm tra biên
    if (x < 0 || x >= 19 || y < 0 || y >= 19) return NONE;
    
    int pos = x * 19 + y;
    if (territory_set.count(pos)) return NONE; 

    PieceColor piece = board->getPiece(x, y);

    if (piece != NONE) return piece; 

    territory_set.insert(pos);
    
    std::set<PieceColor> boundary_colors;
    
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    for (int i = 0; i < 4; ++i) {
        PieceColor neighbor_owner = getTerritoryOwner(x + dx[i], y + dy[i], territory_set);
        
        if (neighbor_owner != NONE) {
            boundary_colors.insert(neighbor_owner);
        }
    }
    
    if (boundary_colors.empty()) {
        return NONE; 
    } else if (boundary_colors.size() == 1) {
        return *boundary_colors.begin();
    } else {
        return NONE; 
    }
}

// game.cpp

std::pair<float, float> Game::calculateFinalScore(float komi) const {
    float black_territory = 0;
    float white_territory = 0;
    
    std::set<int> visited_empty_points; 

    // 1. TÍNH ĐIỂM LÃNH THỔ (TERRITORY)
    for (int x = 0; x < 19; ++x) {
        for (int y = 0; y < 19; y++) {
            
            if (board->getPiece(x, y) == NONE && 
                visited_empty_points.find(x * 19 + y) == visited_empty_points.end()) {
                
                std::set<int> current_territory;
                PieceColor owner = getTerritoryOwner(x, y, current_territory); 

                if (owner == BLACK) {
                    black_territory += current_territory.size();
                } else if (owner == WHITE) {
                    white_territory += current_territory.size();
                }
                
                visited_empty_points.insert(current_territory.begin(), current_territory.end());
            }
        }
    }

    // 2. TÍNH TỔNG ĐIỂM CUỐI CÙNG (Territory Scoring: Lãnh thổ + Quân bắt)
    
    float black_final_score = black_territory + black_captures;
    float white_final_score = white_territory + white_captures + komi;

    // 3. IN KẾT QUẢ
    std::cout << "\n===================================" << std::endl;
    std::cout << "          FINAL SCORE REPORT         " << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << "Black Territory: " << black_territory << " + Captures: " << black_captures << std::endl;
    std::cout << "White Territory: " << white_territory << " + Captures: " << white_captures << " + Komi: " << komi << std::endl;
    std::cout << "Final Black Score: " << black_final_score << std::endl;
    std::cout << "Final White Score: " << white_final_score << std::endl;

    if (black_final_score > white_final_score) {
        std::cout << "Winner: BLACK (by " << black_final_score - white_final_score << " points)" << std::endl;
    } else if (white_final_score > black_final_score) {
        std::cout << "Winner: WHITE (by " << white_final_score - black_final_score << " points)" << std::endl;
    } else {
        std::cout << "Tied! (Draw)" << std::endl;
    }
    return {black_final_score, white_final_score};
}