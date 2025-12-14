#include "MainMenu.hpp"
#include "ModeSelection.hpp"
#include "Settings.hpp"
#include <SFML/Window/Event.hpp>
#include <algorithm> // std::max

MainMenu::MainMenu(std::shared_ptr<Context> &context)
    : m_context{context}
    , m_gameTitle(m_context->m_assets->GetFont(MAIN_FONT), "Baduk", 64)
    , m_gameIcon(m_context->m_assets->GetTexture(GAME_ICON))          // <-- move ABOVE play text
    , m_playButtonText(m_context->m_assets->GetFont(MAIN_FONT), "Play", 40)
    , m_settingsButtonText(m_context->m_assets->GetFont(MAIN_FONT), "Settings", 40)
    , m_exitButtonText(m_context->m_assets->GetFont(MAIN_FONT), "Exit", 40)
    , m_isPlayButtonSelected(false)
    , m_isSettingsButtonSelected(false)
    , m_isExitButtonSelected(false)
    , m_isPlayButtonPressed(false)
    , m_isSettingsButtonPressed(false)
    , m_isExitButtonPressed(false)
{
    // sizes will be set in Init() for nicer responsive layout
}

MainMenu::~MainMenu() = default;

void MainMenu::Init()
{
    const auto winSize = m_context->m_window->getSize();
    const float cx = static_cast<float>(winSize.x) * 0.5f;

    // ---- Theme ----
    const sf::Color titleColor(245, 245, 245);
    const sf::Color headerFill(35, 35, 35, 200);
    const sf::Color headerOutline(235, 235, 235, 200);

    const sf::Color btnNormal(230, 230, 230);
    const sf::Color btnOutline(255, 255, 255, 140);
    const sf::Color textOnBtn(30, 30, 30);

    // ---- Sizes ----
    const float iconBoxSize = 150.f;  // square box
    const float iconPadding = 14.f;

    const float buttonW = 340.f;
    const float buttonH = 84.f;
    const float buttonGap = 18.f;

    // ---- Header positions (responsive) ----
    const float headerTopY   = winSize.y * 0.18f;
    const float iconCenterY  = headerTopY + iconBoxSize * 0.5f;

    // ---- Icon square ----
    m_titleBar.setSize({iconBoxSize, iconBoxSize});
    m_titleBar.setOrigin({iconBoxSize * 0.5f, iconBoxSize * 0.5f});
    m_titleBar.setPosition({cx, iconCenterY});
    m_titleBar.setFillColor(headerFill);
    m_titleBar.setOutlineThickness(2.f);
    m_titleBar.setOutlineColor(headerOutline);

    // ---- Icon scaling (fit inside square) ----
    {
        const sf::Texture& tex = m_gameIcon.getTexture();
        const auto texSize = tex.getSize();

        const float iconTarget = iconBoxSize - 2.f * iconPadding;
        const float scale = iconTarget / static_cast<float>(std::max(texSize.x, texSize.y));

        m_gameIcon.setScale({scale, scale});
        m_gameIcon.setOrigin({texSize.x * 0.5f, texSize.y * 0.5f});
        m_gameIcon.setPosition({cx, iconCenterY});
    }

    // ---- Title under icon ----
    {
        auto bounds = m_gameTitle.getLocalBounds();
        m_gameTitle.setOrigin(bounds.getCenter());
        m_gameTitle.setFillColor(titleColor);

        const float titleY = iconCenterY + iconBoxSize * 0.5f + 55.f;
        m_gameTitle.setPosition({cx, titleY});
    }

    // ---- Buttons layout under title ----
    const float buttonsTopY = m_gameTitle.getPosition().y + 115.f;

    auto setupButton = [&](sf::RectangleShape& box, sf::Text& text, float centerY)
    {
        box.setSize({buttonW, buttonH});
        box.setOrigin({buttonW * 0.5f, buttonH * 0.5f});
        box.setPosition({cx, centerY});
        box.setFillColor(btnNormal);
        box.setOutlineThickness(2.f);
        box.setOutlineColor(btnOutline);

        auto tb = text.getLocalBounds();
        text.setOrigin(tb.getCenter());
        text.setPosition(box.getPosition());
        text.setFillColor(textOnBtn);
    };

    setupButton(m_playButtonBox,     m_playButtonText,     buttonsTopY + 0.f * (buttonH + buttonGap));
    setupButton(m_settingsButtonBox, m_settingsButtonText, buttonsTopY + 1.f * (buttonH + buttonGap));
    setupButton(m_exitButtonBox,     m_exitButtonText,     buttonsTopY + 2.f * (buttonH + buttonGap));
}

void MainMenu::ProcessInput()
{
    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
        }
        else if (const auto *mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos{
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y)};

            const auto playBounds     = m_playButtonBox.getGlobalBounds();
            const auto settingsBounds = m_settingsButtonBox.getGlobalBounds();
            const auto exitBounds     = m_exitButtonBox.getGlobalBounds();

            m_isPlayButtonSelected = playBounds.contains(mousePos);
            m_isSettingsButtonSelected = (!m_isPlayButtonSelected && settingsBounds.contains(mousePos));
            m_isExitButtonSelected = (!m_isPlayButtonSelected && !m_isSettingsButtonSelected && exitBounds.contains(mousePos));
        }
        else if (const auto *mouseButton = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseButton->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePos{
                    static_cast<float>(mouseButton->position.x),
                    static_cast<float>(mouseButton->position.y)};

                const auto playBounds     = m_playButtonBox.getGlobalBounds();
                const auto settingsBounds = m_settingsButtonBox.getGlobalBounds();
                const auto exitBounds     = m_exitButtonBox.getGlobalBounds();

                m_isPlayButtonPressed = playBounds.contains(mousePos);
                m_isSettingsButtonPressed = (!m_isPlayButtonPressed && settingsBounds.contains(mousePos));
                m_isExitButtonPressed = (!m_isPlayButtonPressed && !m_isSettingsButtonPressed && exitBounds.contains(mousePos));
            }
        }
    }
}

void MainMenu::Update(sf::Time)
{
    const sf::Color btnNormal(230, 230, 230);
    const sf::Color btnHover(245, 245, 245);
    const sf::Color btnOutline(255, 255, 255, 140);
    const sf::Color btnOutlineHover(255, 255, 255, 220);

    const sf::Color textNormal(30, 30, 30);
    const sf::Color textHover(10, 10, 10);

    auto applyHover = [&](bool hovered, sf::RectangleShape& box, sf::Text& text)
    {
        if (hovered)
        {
            box.setFillColor(btnHover);
            box.setOutlineColor(btnOutlineHover);
            text.setFillColor(textHover);
        }
        else
        {
            box.setFillColor(btnNormal);
            box.setOutlineColor(btnOutline);
            text.setFillColor(textNormal);
        }
    };

    applyHover(m_isPlayButtonSelected,     m_playButtonBox,     m_playButtonText);
    applyHover(m_isSettingsButtonSelected, m_settingsButtonBox, m_settingsButtonText);
    applyHover(m_isExitButtonSelected,     m_exitButtonBox,     m_exitButtonText);

    if (m_isPlayButtonPressed)
    {
        m_context->m_states->Add(std::make_unique<ModeSelection>(m_context), false);
        m_isPlayButtonPressed = false;
    }

    if (m_isSettingsButtonPressed)
    {
        m_context->m_states->Add(std::make_unique<SettingsState>(m_context), false);
        m_isSettingsButtonPressed = false;
    }

    if (m_isExitButtonPressed)
    {
        m_context->m_window->close();
        m_isExitButtonPressed = false;
    }
}

void MainMenu::Draw()
{
    m_context->m_window->draw(m_titleBar);
    m_context->m_window->draw(m_gameIcon);
    m_context->m_window->draw(m_gameTitle);

    m_context->m_window->draw(m_playButtonBox);
    m_context->m_window->draw(m_settingsButtonBox);
    m_context->m_window->draw(m_exitButtonBox);

    m_context->m_window->draw(m_playButtonText);
    m_context->m_window->draw(m_settingsButtonText);
    m_context->m_window->draw(m_exitButtonText);
}
