/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2011  <copyright holder> <email>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "IRenderTarget.h"
#include "render/EventTypes.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <string>
#include <unordered_map>

struct SdlImage : public Drawable::Image
{
    SDL_Texture *sdlTexture = nullptr;
    SDL_Renderer *renderer = nullptr; // non-owning, for re-creation

    bool isValid() const override;
    ~SdlImage() override;
};

struct SdlText : public Drawable::Text
{
    SdlText();
    ~SdlText() override;

    TTF_Font *font = nullptr;           // non-owning
    const uint8_t *fontData = nullptr;  // raw font data for re-opening
    size_t fontDataSize = 0;
    TTF_Font *ownedFont = nullptr;      // owned font opened at current point size
    SDL_Texture *cachedTexture = nullptr;
    SDL_Renderer *renderer = nullptr;   // non-owning
    std::string cachedString;
    float cachedPointSize = 0;
    int cachedWidth = 0;
    int cachedHeight = 0;

    float lineSpacing() override;
    Size size() override;

    void ensureFont();
    void invalidateCache();
};

class SdlRenderTarget : public IRenderTarget
{
public:
    static std::unique_ptr<Drawable::Window> createWindow(const Size size, const std::string &title);

    SdlRenderTarget(const Size &size, SDL_Renderer *renderer);
    ~SdlRenderTarget() override;

    Size getSize() const override;
    void setSize(const Size size) const override;

    void draw(const ScreenRect &rect, const Drawable::Color &fillColor, const Drawable::Color &outlineColor = Drawable::Transparent, const float outlineSize = 1.) override;
    void draw(const std::shared_ptr<IRenderTarget> &renderTarget, const Drawable::BlendMode blendMode) override;

    void display() override;

    void draw(const Drawable::Rect &rect) override;
    void draw(const Drawable::Circle &circle) override;
    void draw(const std::shared_ptr<IRenderTarget> &renderTarget, const ScreenPos &pos = ScreenPos(0, 0)) override;

    Drawable::Image::Ptr createImage(const Size &size, const uint8_t *bytes) const override;
    Drawable::Image::Ptr loadImage(const uint8_t *data, const size_t dataSize) const override;
    void draw(const Drawable::Image::Ptr &image, const ScreenPos &position) override;
    void draw(const Drawable::Image::Ptr &image, const ScreenPos &position, const Drawable::BlendMode blendMode) override;

    std::shared_ptr<IRenderTarget> createTextureTarget(const Size &size) const override;

    Drawable::Text::Ptr createText(const Drawable::Text::Style style = Drawable::Text::Plain) const override;
    void draw(const Drawable::Text::Ptr &text) override;

    void clear(const Drawable::Color &color = Drawable::Color(0, 0, 0, 255)) override;

    SDL_Renderer *renderer() const { return m_renderer; }
    SDL_Texture *texture() const { return m_texture; }

private:
    friend struct SdlWindow;

    SDL_Renderer *m_renderer = nullptr; // non-owning
    SDL_Texture *m_texture = nullptr;   // owned, for off-screen targets
    mutable Size m_size;
    bool m_isTextureTarget = false;

    // Font data cache (static, shared across all targets)
    struct FontInfo {
        const uint8_t *data = nullptr;
        size_t dataSize = 0;
    };
    static FontInfo s_plainFont;
    static FontInfo s_uiFont;
    static FontInfo s_stylishFont;
    static bool s_fontsInitialized;
    static void initFonts();
};

struct SdlWindow : public Drawable::Window
{
    SdlWindow(const Size size, const std::string &title);
    ~SdlWindow() override;

    SDL_Window *sdlWindow = nullptr;
    SDL_Renderer *sdlRenderer = nullptr;
    bool m_open = true;

    void display() override;
    bool isOpen() const override;
    void close() override;

    bool pollEvent(input::Event &event);
};
