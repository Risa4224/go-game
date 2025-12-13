#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <string>
#include <SFML/System/Clock.hpp>
#include <atomic>
#include <mutex>
#include <thread>

#include "State.hpp"
#include "GameApp.h"
#include "game.h"
#include "AI.h"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

class MainBoard : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    sf::RectangleShape m_boardBackground;
    std::vector<sf::Vertex> m_gridLines;
    float m_boardPixelSize;
    int m_boardSize;
    float m_cellSize;
    sf::Vector2f m_boardTopLeft;

    std::vector<sf::CircleShape> m_stones;

    // UI Buttons
    sf::RectangleShape m_undoButtonBox;
    sf::RectangleShape m_redoButtonBox;
    sf::RectangleShape m_passButtonBox;
    sf::RectangleShape m_pauseButtonBox;
    sf::RectangleShape m_saveButtonBox;
    sf::RectangleShape m_loadButtonBox;
    
    // Hover states
    bool m_undoHovered;
    bool m_redoHovered;
    bool m_passHovered;
    bool m_pauseHovered;
    bool m_saveHovered;
    bool m_loadHovered;

    // Textures for Themes
    sf::Texture m_boardTextureClassic;
    sf::Texture m_boardTextureDark;
    bool m_hasClassicTexture = false;
    bool m_hasDarkTexture = false;

    // Stone theme caching (so we can refresh visuals when the user changes settings)
    StoneTheme m_lastStoneTheme = StoneTheme::Classic;

    // Stone textures (SFML 3: shapes store a pointer to the texture, so textures must outlive the shapes)
    sf::Texture m_stoneTexClassicBlack;
    sf::Texture m_stoneTexClassicWhite;
    sf::Texture m_stoneTexSlateShellBlack;
    sf::Texture m_stoneTexSlateShellWhite;
    sf::Texture m_stoneTexGlassBlack;
    sf::Texture m_stoneTexGlassWhite;

    bool m_hasStoneTexClassicBlack = false;
    bool m_hasStoneTexClassicWhite = false;
    bool m_hasStoneTexSlateShellBlack = false;
    bool m_hasStoneTexSlateShellWhite = false;
    bool m_hasStoneTexGlassBlack = false;
    bool m_hasStoneTexGlassWhite = false;

    // Notification Logic
    std::string m_notificationText;
    bool m_showNotification = false;
    sf::Clock m_notificationClock;
    float m_notificationDuration = 3.f; 

    // Menu Overlay Text Helpers (for Save/Load menu)
    std::optional<sf::Text> m_menuTitleText;
    std::optional<sf::Text> m_menuDeleteText;
    std::optional<sf::Text> m_menuCancelText;
    std::optional<sf::Text> m_menuActionText;
    std::optional<sf::Text> m_fileListText;

    std::unique_ptr<Game> m_game;

    // --- Async AI move (prevents UI freeze) ---
    std::thread m_aiThread;
    mutable std::mutex m_aiMutex;
    std::atomic<bool> m_aiThinking{false};
    std::atomic<bool> m_aiResultReady{false};
    AIMove m_aiResult;
    sf::Clock m_aiDelayClock;
    float m_aiDelaySeconds{3.f};


    void setNotification(const std::string &msg);
    void buildGrid();
    void loadStoneTexturesFromFiles();
    const sf::Texture* getStoneTexture(StoneTheme theme, PieceColor c) const;
    void rebuildStonesFromGame();
    void applyStoneVisual(sf::CircleShape& stone, PieceColor c) const;
    void handleLeftClick(const sf::Vector2i &pixelPos);
    void resetGame();

    // AI Helpers
    bool isAIMode() const;
    PieceColor humanColor() const;
    PieceColor aiColor() const;
    void maybeRunAITurn(); // queues AI in background
    void queueAITurn(float delaySeconds);
    void pollAITurn();
    void applyAIMove(const AIMove& mv);
    void cancelAIWorker();
    bool aiBusy() const;
    void handleGameOver();
    
    // Sounds
    sf::Sound m_placeSound;
    sf::Sound m_passSound;
    sf::Sound m_invalidSound;
    sf::Sound m_winSound;

    std::vector<std::string> m_moveHistory;
    std::vector<std::string> m_moveRedo;
public:
    MainBoard(std::shared_ptr<Context> &context);
    ~MainBoard() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};