#pragma once

#include <map>
#include <memory>
#include <string>

#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
namespace Engine
{
    class AssetMan
    {
    public:
        AssetMan();
        ~AssetMan();

        void AddTexture(int id, const std::string &filePath, bool wantRepeated = false);
        void AddFont(int id, const std::string &filePath);
        void AddSoundBuffer(int id, const std::string &filePath);
        const sf::SoundBuffer &GetSoundBuffer(int id) const;
        const sf::Texture &GetTexture(int id) const;
        const sf::Font &GetFont(int id) const;

    private:
        std::map<int, std::unique_ptr<sf::Texture>> m_textures;
        std::map<int, std::unique_ptr<sf::Font>> m_fonts;
        std::map<int, std::unique_ptr<sf::SoundBuffer>> m_soundBuffers;
    };
}