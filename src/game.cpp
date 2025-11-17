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
    : board(new Board(*(other.board))), // ✅ Khởi tạo Boasrd MỚI
      turn(other.turn), 
      groups(other.groups)
{
    // Thân hàm BỎ TRỐNG
}
// game.cpp (SỬA LỖI)
Game& Game::operator=(const Game& other) {
    if (this != &other) { // Kiểm tra tự gán (self-assignment)
        
        // 1. Giải phóng bộ nhớ Board CŨ trước
        delete this->board;
        
        // 2. Deep Copy Board MỚI
        this->board = new Board(*(other.board));
        
        // 3. Sao chép các biến trạng thái cốt lõi
        this->turn = other.turn;
        this->groups = other.groups;
        
        // KHÔNG sao chép history và future để bảo toàn trạng thái undo/redo hiện tại
    }
    return *this;
}

Game::Game(Board* b) : board(b), turn(BLACK) {}


// Phương thức Logic Game (PRIVATE)
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
    std::cout << "Checking captures around (" << x << ", " << y << "):\n";
    if(board->getPiece(x-1,y) == oppositeColor(c)) {
        std::cout << x - 1 << ' ' << y << '\n';
        std::cout << findGroupIndex(x-1,y) << '\n';
        int groupIdx = findGroupIndex(x-1,y);
        if(groupIdx != -1){
            int tmp = calcLiberties(groupIdx);
            if(tmp == 0) removeGroup(x-1,y, total_captures);
        }
    }
    if(board->getPiece(x+1,y) == oppositeColor(c)) {
        std:: cout << x + 1 << ' ' << y << '\n';
        std:: cout << findGroupIndex(x+1,y) << '\n';
        int groupIdx = findGroupIndex(x+1,y);
        if(groupIdx != -1){
            int tmp = calcLiberties(groupIdx);
            if(tmp == 0) removeGroup(x+1,y, total_captures);
        }
    }
    if(board->getPiece(x,y-1) == oppositeColor(c)) {
        std:: cout << x << ' ' << y - 1 << '\n';       
        std:: cout << findGroupIndex(x,y-1) << '\n';
        int groupIdx = findGroupIndex(x,y-1);
        if (groupIdx != -1){
            int tmp = calcLiberties(groupIdx);
            if(tmp == 0) removeGroup(x,y-1, total_captures);
        }
    }
    if(board->getPiece(x,y+1) == oppositeColor(c)) {
        std:: cout << x << ' ' << y + 1 << '\n';
        std:: cout << findGroupIndex(x,y+1) << '\n';
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
        std::cout << "Removing piece at (" << tempx << ", " << tempy << ")\n";
        board->removePiece(tempx, tempy); 
    }
    groups.erase(groups.begin() + ind);
}

// Cập nhật chữ ký: nhận tham chiếu const
// Hàm này tính toán số lượng khí (ô trống kề cạnh) cho nhóm có ID đã cho.
int Game::calcLiberties(int id){ 
    if (id < 0 || id >= (int)groups.size()) {
        return 0; // Tránh lỗi nếu ID không hợp lệ
    }
    
    // Sử dụng set để lưu trữ các vị trí khí (dạng int x*19+y)
    // đảm bảo mỗi ô trống chỉ được tính một lần.
    std::set<int> unique_liberties;
    PieceGroup& current_group = groups[id];

    int tempx, tempy;
    for(int i = 0; i < current_group.getSize(); ++i) {
        int location = current_group.getLocation(i);
        tempx = location / 19;
        tempy = location % 19;
        // Kiểm tra 4 hướng (Đông, Tây, Nam, Bắc) của quân cờ này
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};

        for (int k = 0; k < 4; ++k) {
            int nx = tempx + dx[k];
            int ny = tempy + dy[k];
            std::cout << "Calculating liberties for piece at (" << nx << ", " << ny << ")\n";
            // Hàm valid(x,y) của bạn kiểm tra: 
            // 1. Trong biên (in bounds)
            // 2. Ô trống (board->getPiece(x,y) == NONE)
            if(valid(nx, ny)) {
                // Đây là một ô khí hợp lệ. Thêm nó vào set.
                // Nếu nó đã tồn tại trong set, nó sẽ không được thêm lần nữa.
                unique_liberties.insert(nx * 19 + ny);
            }
        }
    }
    // Tổng số khí của nhóm chính là kích thước của set.
    std::cout << "Total liberties for group " << id << ": " << unique_liberties.size() << std::endl;
    return unique_liberties.size();
}

bool Game::ended(int x, int y){
    if(x < 0 || y < 0) return true;
    return false;
}


// game.cpp

bool Game::placeStone(int x, int y) {
    // 1. KIỂM TRA LUẬT CẤP ĐỘ BOARD
    if(!valid(x, y)){
        std::cout << "Invalid move: spot already occupied or out of bounds." << endl;
        return false;
    }
    history.push_back(*this); // Lưu trạng thái hiện tại vào lịch sử trước khi thay đổi
    PieceColor current_color = turn;

    // --- BẮT ĐẦU NƯỚC ĐI TẠM THỜI ---
    
    // 2. SAO LƯU TRẠNG THÁI groups (QUAN TRỌNG: Để khôi phục nếu là Suicide)
    std::vector<PieceGroup> groups_backup = groups;

    // 3. ĐẶT QUÂN TẠM THỜI lên board
    board->setPiece(x, y, current_color); 

    // 4. GỘP NHÓM TẠM THỜI (thao tác này thay đổi vector groups)
    processGroups(x, y, current_color); 
    int placed_group_id = findGroupIndex(x, y); 

    // 5. KIỂM TRA VÀ THỰC HIỆN BẮT QUÂN ĐỐI PHƯƠNG
    // (Lưu ý: checkCaptures sẽ gọi removeGroup, làm sạch board và groups)
    int captures = checkCaptures(x, y, current_color);
    
    // 6. KIỂM TRA LUẬT TỰ SÁT (Suicide Rule)
    
    // Điều kiện TỰ SÁT:
    // a) Không bắt được quân nào (captures == 0)
    // VÀ
    // b) Nhóm quân vừa đặt (sau khi bắt quân, nếu có) bị hết khí (calcLiberties(id) == 0)
    cout << "number of captures : " << captures << '\n' << calcLiberties(placed_group_id) << '\n';

    if(current_color == BLACK) black_captures += captures;
    else white_captures += captures;
    if (captures == 0 && calcLiberties(placed_group_id) == 0) {
        
        // --- NƯỚC ĐI TỰ SÁT: THỰC HIỆN HOÀN TÁC (ROLLBACK) ---
        history.pop_back();
        cout << "Invalid move: Suicide (Group has 0 liberties and no captures)." << endl;
        if(current_color == BLACK) black_captures += captures;
        else white_captures += captures;
        // 1. Khôi phục groups về trạng thái ban đầu (trước khi processGroups)
        groups = groups_backup;         
        
        // 2. Xóa quân vừa đặt khỏi board (vì nó không nằm trong groups_backup)
        board->removePiece(x, y);       
        
        return false; // Nước đi bất hợp lệ, không chuyển lượt
    }

    // 7. KẾT THÚC NƯỚC ĐI HỢP LỆ (Nếu nước đi vượt qua kiểm tra)   
    future.clear();
    // Switch the turn
    turn = oppositeColor(turn);
    
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

// game.cpp

bool Game::undo() {
    if (history.empty()) {
        std::cout << "Cannot undo: No previous states." << std::endl;
        return false;
    }
    
    Game prevState = history.back(); 
    history.pop_back(); 
    
    // Lưu trạng thái HIỆN TẠI (trước Undo) vào future
    future.push_back(*this); // Sẽ gọi Copy Constructor đã sửa lỗi
    
    // Gán trạng thái MỚI VÀ ĐỘC LẬP (prevState) cho đối tượng hiện tại (*this)
    *this = prevState; // Sẽ gọi Copy Assignment Operator đã sửa lỗi
    
    std::cout << "Undo successful. Game state reverted to previous turn." << std::endl;
    return true;
}
bool Game::redo() {
    if (future.empty()) {
        std::cout << "Cannot redo: No future states." << std::endl; // Lỗi này sẽ không xuất hiện nữa
        return false;
    }
    
    Game nextState = future.back(); 
    future.pop_back(); 
    
    // Lưu trạng thái HIỆN TẠI (trước Redo) vào history
    history.push_back(*this); // Sẽ gọi Copy Constructor đã sửa lỗi
    
    // Gán trạng thái MỚI VÀ ĐỘC LẬP (nextState) cho đối tượng hiện tại (*this)
    *this = nextState; // Sẽ gọi Copy Assignment Operator đã sửa lỗi

    std::cout << "Redo successful. Game state moved forward." << std::endl;
    return true;
}

// game.cpp

// Trả về NONE, BLACK, WHITE, hoặc AMBIGUOUS (Tùy thuộc vào các màu biên)
PieceColor Game::getTerritoryOwner(int x, int y, std::set<int>& territory_set) const {
    
    // 1. Kiểm tra biên
    if (x < 0 || x >= 19 || y < 0 || y >= 19) return NONE;
    
    int pos = x * 19 + y;
    if (territory_set.count(pos)) return NONE; // Đã thăm trong lần gọi này

    PieceColor piece = board->getPiece(x, y);

    // 2. Nếu gặp quân cờ, trả về màu quân cờ đó (Đây là biên lãnh thổ)
    if (piece != NONE) return piece; 

    // 3. Nếu là ô trống, thêm vào lãnh thổ
    territory_set.insert(pos);
    
    // Khởi tạo các màu biên (BOUNDARY COLORS)
    std::set<PieceColor> boundary_colors;
    
    // 4. Duyệt 4 hướng (Đệ quy)
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    for (int i = 0; i < 4; ++i) {
        PieceColor neighbor_owner = getTerritoryOwner(x + dx[i], y + dy[i], territory_set);
        
        if (neighbor_owner != NONE) {
            boundary_colors.insert(neighbor_owner);
        }
    }
    
    // 5. Logic xác định chủ sở hữu lãnh thổ
    if (boundary_colors.empty()) {
        // Lãnh thổ chưa bị bao quanh (Border of the board or huge open area)
        return NONE; 
    } else if (boundary_colors.size() == 1) {
        // Lãnh thổ đã được xác định bởi MỘT màu duy nhất
        return *boundary_colors.begin();
    } else {
        // Có nhiều màu bao quanh (khu vực tranh chấp/ko rõ ràng)
        return NONE; 
    }
}

// game.cpp

void Game::calculateFinalScore(float komi) const {
    float black_territory = 0;
    float white_territory = 0;
    
    // Set để theo dõi các ô đã được kiểm tra (tránh tính toán lại)
    std::set<int> visited_empty_points; 

    // 1. TÍNH ĐIỂM LÃNH THỔ (TERRITORY)
    for (int x = 0; x < 19; ++x) {
        for (int y = 0; y < 19; y++) {
            
            // Nếu ô là trống VÀ chưa được kiểm tra
            if (board->getPiece(x, y) == NONE && 
                visited_empty_points.find(x * 19 + y) == visited_empty_points.end()) {
                
                std::set<int> current_territory;
                // Gọi hàm DFS/Flood Fill để xác định chủ sở hữu
                PieceColor owner = getTerritoryOwner(x, y, current_territory); 

                if (owner == BLACK) {
                    black_territory += current_territory.size();
                } else if (owner == WHITE) {
                    white_territory += current_territory.size();
                }
                
                // Đánh dấu tất cả các ô đã thăm
                visited_empty_points.insert(current_territory.begin(), current_territory.end());
            }
        }
    }

    // 2. TÍNH TỔNG ĐIỂM CUỐI CÙNG (Territory Scoring: Lãnh thổ + Quân bắt)
    
    // LƯU Ý QUAN TRỌNG: Bạn phải đảm bảo black_captures và white_captures 
    // đã được cập nhật chính xác trong hàm removeGroup.
    // Nếu chưa có, các biến này sẽ là 0.
    
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
}