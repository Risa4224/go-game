#include "AssetMan.hpp"
#include <iostream>
#include <SFML/Audio/SoundBuffer.hpp> 
Engine::AssetMan::AssetMan()
{
}

Engine::AssetMan::~AssetMan()
{
}

void Engine::AssetMan::AddTexture(int id, const std::string &filePath, bool wantRepeated)
{
    auto texture = std::make_unique<sf::Texture>();

    if (texture->loadFromFile(filePath))
    {
        texture->setRepeated(wantRepeated);
        m_textures[id] = std::move(texture);
    }
}

void Engine::AssetMan::AddFont(int id, const std::string &filePath)
{
    auto font = std::make_unique<sf::Font>();

    if (font->openFromFile(filePath))
    {
        m_fonts[id] = std::move(font);
    }
}

const sf::Texture &Engine::AssetMan::GetTexture(int id) const
{
    return *(m_textures.at(id).get());
}

const sf::Font &Engine::AssetMan::GetFont(int id) const
{
    return *(m_fonts.at(id).get());
}   

void Engine::AssetMan::AddSoundBuffer(int id, const std::string &filePath)
{
    auto buffer = std::make_unique<sf::SoundBuffer>();

    if (buffer->loadFromFile(filePath))
    {
        m_soundBuffers[id] = std::move(buffer);
    }
    else
    {
        std::cerr << "[AssetMan] Failed to load sound: " << filePath << '\n';
    }
}

const sf::SoundBuffer &Engine::AssetMan::GetSoundBuffer(int id) const
{
    return *(m_soundBuffers.at(id).get());
}