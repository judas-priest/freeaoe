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

#include "SdlRenderTarget.h"

#include "render/Camera.h"

#include "misc/fonts/Alegreya/Alegreya-Bold.ttf.h"
#include "misc/fonts/BerryRotunda/BerryRotunda.ttf.h"
#include "misc/fonts/charis/Charis-Regular.ttf.h"

#include <cstring>
#include <algorithm>

Drawable::Image::Ptr Drawable::Image::null = std::make_shared<SdlImage>();

// Static font data
SdlRenderTarget::FontInfo SdlRenderTarget::s_plainFont;
SdlRenderTarget::FontInfo SdlRenderTarget::s_uiFont;
SdlRenderTarget::FontInfo SdlRenderTarget::s_stylishFont;
bool SdlRenderTarget::s_fontsInitialized = false;

void SdlRenderTarget::initFonts()
{
    if (s_fontsInitialized) {
        return;
    }
    s_fontsInitialized = true;

    s_plainFont.data = resource_Charis_Regular_ttf_data;
    s_plainFont.dataSize = resource_Charis_Regular_ttf_size;

    s_uiFont.data = resource_Alegreya_Bold_ttf_data;
    s_uiFont.dataSize = resource_Alegreya_Bold_ttf_size;

    s_stylishFont.data = resource_BerryRotunda_ttf_data;
    s_stylishFont.dataSize = resource_BerryRotunda_ttf_size;
}

//------------------------------------------------------------------------------
// SDL blend mode conversion
//------------------------------------------------------------------------------
static SDL_BlendMode toSdlBlendMode(Drawable::BlendMode mode)
{
    switch (mode) {
    case Drawable::BlendMode::Add:      return SDL_BLENDMODE_ADD;
    case Drawable::BlendMode::Multiply: return SDL_BLENDMODE_MUL;
    case Drawable::BlendMode::None:     return SDL_BLENDMODE_NONE;
    case Drawable::BlendMode::Alpha:
    default:                            return SDL_BLENDMODE_BLEND;
    }
}

//------------------------------------------------------------------------------
// SDL key conversion
//------------------------------------------------------------------------------
static input::Key sdlKeyToInput(SDL_Keycode key)
{
    switch (key) {
    case SDLK_LEFT:      return input::Key::Left;
    case SDLK_RIGHT:     return input::Key::Right;
    case SDLK_UP:        return input::Key::Up;
    case SDLK_DOWN:      return input::Key::Down;
    case SDLK_ESCAPE:    return input::Key::Escape;
    case SDLK_RETURN:    return input::Key::Return;
    case SDLK_DELETE:    return input::Key::Delete;
    case SDLK_BACKSPACE: return input::Key::BackSpace;
    case SDLK_SPACE:     return input::Key::Space;
    case SDLK_TAB:       return input::Key::Tab;
    case SDLK_LSHIFT:    return input::Key::LShift;
    case SDLK_RSHIFT:    return input::Key::RShift;
    case SDLK_LCTRL:     return input::Key::LControl;
    case SDLK_RCTRL:     return input::Key::RControl;
    case SDLK_LALT:      return input::Key::LAlt;
    case SDLK_RALT:      return input::Key::RAlt;
    default:
        if (key >= SDLK_a && key <= SDLK_z)
            return input::Key(int(input::Key::A) + (key - SDLK_a));
        if (key >= SDLK_0 && key <= SDLK_9)
            return input::Key(int(input::Key::Num0) + (key - SDLK_0));
        if (key >= SDLK_F1 && key <= SDLK_F12)
            return input::Key(int(input::Key::F1) + (key - SDLK_F1));
        return input::Key::Unknown;
    }
}

//------------------------------------------------------------------------------
// Circle drawing helpers
//------------------------------------------------------------------------------
static void drawCircleOutline(SDL_Renderer *renderer, int cx, int cy, int radius)
{
    int x = radius, y = 0, err = 0;
    while (x >= y) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);
        if (err <= 0) { y++; err += 2 * y + 1; }
        if (err > 0)  { x--; err -= 2 * x + 1; }
    }
}

static void drawCircleFilled(SDL_Renderer *renderer, int cx, int cy, int radius)
{
    int x = radius, y = 0, err = 0;
    while (x >= y) {
        SDL_RenderDrawLine(renderer, cx - x, cy + y, cx + x, cy + y);
        SDL_RenderDrawLine(renderer, cx - y, cy + x, cx + y, cy + x);
        SDL_RenderDrawLine(renderer, cx - x, cy - y, cx + x, cy - y);
        SDL_RenderDrawLine(renderer, cx - y, cy - x, cx + y, cy - x);
        if (err <= 0) { y++; err += 2 * y + 1; }
        if (err > 0)  { x--; err -= 2 * x + 1; }
    }
}

//------------------------------------------------------------------------------
// Ellipse drawing helpers (for aspect ratio != 1)
//------------------------------------------------------------------------------
static void drawEllipseOutline(SDL_Renderer *renderer, int cx, int cy, int rx, int ry)
{
    // Midpoint ellipse algorithm
    int x = 0, y = ry;
    long long rx2 = (long long)rx * rx;
    long long ry2 = (long long)ry * ry;
    long long twoRx2 = 2 * rx2;
    long long twoRy2 = 2 * ry2;
    long long px = 0;
    long long py = twoRx2 * y;
    long long p;

    // Region 1
    p = ry2 - rx2 * ry + rx2 / 4;
    while (px < py) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        x++;
        px += twoRy2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= twoRx2;
            p += ry2 + px - py;
        }
    }

    // Region 2
    p = ry2 * (x * 2 + 1) * (x * 2 + 1) / 4 + rx2 * ((long long)(y - 1) * (y - 1) - ry2);
    while (y >= 0) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        y--;
        py -= twoRx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twoRy2;
            p += rx2 - py + px;
        }
    }
}

static void drawEllipseFilled(SDL_Renderer *renderer, int cx, int cy, int rx, int ry)
{
    for (int dy = -ry; dy <= ry; dy++) {
        // x^2/rx^2 + y^2/ry^2 = 1  =>  x = rx * sqrt(1 - y^2/ry^2)
        double ratio = 1.0 - (double)(dy * dy) / (double)(ry * ry);
        if (ratio < 0) ratio = 0;
        int dx = (int)(rx * std::sqrt(ratio));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

//------------------------------------------------------------------------------
// SdlImage
//------------------------------------------------------------------------------
bool SdlImage::isValid() const
{
    return sdlTexture != nullptr;
}

SdlImage::~SdlImage()
{
    if (sdlTexture) {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = nullptr;
    }
}

//------------------------------------------------------------------------------
// SdlText
//------------------------------------------------------------------------------
SdlText::SdlText()
{
}

SdlText::~SdlText()
{
    if (cachedTexture) {
        SDL_DestroyTexture(cachedTexture);
        cachedTexture = nullptr;
    }
    if (ownedFont) {
        TTF_CloseFont(ownedFont);
        ownedFont = nullptr;
    }
}

void SdlText::ensureFont()
{
    if (!fontData || fontDataSize == 0) {
        return;
    }

    int targetSize = static_cast<int>(pointSize);
    if (targetSize < 1) targetSize = 1;

    if (ownedFont) {
        // Check if we need to re-open at a different size
        // TTF_FontHeight isn't exactly point size, so we track it ourselves
        if (static_cast<int>(cachedPointSize) == targetSize) {
            font = ownedFont;
            return;
        }
        TTF_CloseFont(ownedFont);
        ownedFont = nullptr;
    }

    SDL_RWops *rw = SDL_RWFromConstMem(fontData, static_cast<int>(fontDataSize));
    if (!rw) {
        WARN << "Failed to create RWops for font data";
        return;
    }
    ownedFont = TTF_OpenFontRW(rw, 1, targetSize);
    if (!ownedFont) {
        WARN << "Failed to open font at size" << targetSize << ":" << TTF_GetError();
        return;
    }
    font = ownedFont;
    cachedPointSize = static_cast<float>(targetSize);
}

void SdlText::invalidateCache()
{
    if (cachedTexture) {
        SDL_DestroyTexture(cachedTexture);
        cachedTexture = nullptr;
    }
    cachedString.clear();
    cachedWidth = 0;
    cachedHeight = 0;
}

float SdlText::lineSpacing()
{
    ensureFont();
    if (!font) return pointSize;
    return static_cast<float>(TTF_FontLineSkip(font));
}

Size SdlText::size()
{
    ensureFont();
    if (!font) return Size(0, 0);

    const std::string &str = string.empty() ? std::string("M") : string;
    int w = 0, h = 0;
    TTF_SizeUTF8(font, str.c_str(), &w, &h);
    return Size(static_cast<float>(w), static_cast<float>(h));
}

//------------------------------------------------------------------------------
// SdlRenderTarget
//------------------------------------------------------------------------------
std::unique_ptr<Drawable::Window> SdlRenderTarget::createWindow(const Size size, const std::string &title)
{
    return std::make_unique<SdlWindow>(size, title);
}

SdlRenderTarget::SdlRenderTarget(const Size &size, SDL_Renderer *renderer) :
    m_renderer(renderer),
    m_size(size),
    m_isTextureTarget(true)
{
    initFonts();

    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        static_cast<int>(size.width),
        static_cast<int>(size.height)
    );
    if (m_texture) {
        SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);
    } else {
        WARN << "Failed to create texture target:" << SDL_GetError();
    }
}

SdlRenderTarget::~SdlRenderTarget()
{
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
}

Size SdlRenderTarget::getSize() const
{
    return m_size;
}

void SdlRenderTarget::setSize(const Size size) const
{
    m_size = size;
    m_camera->setViewportSize(size);
}

void SdlRenderTarget::draw(const ScreenRect &rect, const Drawable::Color &fillColor, const Drawable::Color &outlineColor, const float outlineSize)
{
    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    SDL_Rect sdlRect;
    sdlRect.x = static_cast<int>(rect.x);
    sdlRect.y = static_cast<int>(rect.y);
    sdlRect.w = static_cast<int>(rect.width);
    sdlRect.h = static_cast<int>(rect.height);

    // Fill
    SDL_SetRenderDrawColor(m_renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
    SDL_RenderFillRect(m_renderer, &sdlRect);

    // Outline
    if (outlineSize > 0 && outlineColor.a > 0) {
        SDL_SetRenderDrawColor(m_renderer, outlineColor.r, outlineColor.g, outlineColor.b, outlineColor.a);
        SDL_RenderDrawRect(m_renderer, &sdlRect);
    }

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

void SdlRenderTarget::draw(const Drawable::Rect &rect)
{
    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    SDL_Rect sdlRect;
    sdlRect.x = static_cast<int>(rect.rect.x);
    sdlRect.y = static_cast<int>(rect.rect.y);
    sdlRect.w = static_cast<int>(rect.rect.width);
    sdlRect.h = static_cast<int>(rect.rect.height);

    if (rect.filled) {
        SDL_SetRenderDrawColor(m_renderer, rect.fillColor.r, rect.fillColor.g, rect.fillColor.b, rect.fillColor.a);
        SDL_RenderFillRect(m_renderer, &sdlRect);
    }

    if (rect.borderSize > 0) {
        SDL_SetRenderDrawColor(m_renderer, rect.borderColor.r, rect.borderColor.g, rect.borderColor.b, rect.borderColor.a);
        SDL_RenderDrawRect(m_renderer, &sdlRect);
    }

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

void SdlRenderTarget::draw(const Drawable::Circle &circle)
{
    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    int cx = static_cast<int>(circle.center.x);
    int cy = static_cast<int>(circle.center.y);
    int radius = static_cast<int>(circle.radius);

    if (circle.aspectRatio != 1.f) {
        int rx = radius;
        int ry = static_cast<int>(radius * circle.aspectRatio);

        if (circle.filled) {
            SDL_SetRenderDrawColor(m_renderer, circle.fillColor.r, circle.fillColor.g, circle.fillColor.b, circle.fillColor.a);
            drawEllipseFilled(m_renderer, cx, cy, rx, ry);
        }
        if (circle.borderSize > 0) {
            SDL_SetRenderDrawColor(m_renderer, circle.borderColor.r, circle.borderColor.g, circle.borderColor.b, circle.borderColor.a);
            drawEllipseOutline(m_renderer, cx, cy, rx, ry);
        }
    } else {
        if (circle.filled) {
            SDL_SetRenderDrawColor(m_renderer, circle.fillColor.r, circle.fillColor.g, circle.fillColor.b, circle.fillColor.a);
            drawCircleFilled(m_renderer, cx, cy, radius);
        }
        if (circle.borderSize > 0) {
            SDL_SetRenderDrawColor(m_renderer, circle.borderColor.r, circle.borderColor.g, circle.borderColor.b, circle.borderColor.a);
            drawCircleOutline(m_renderer, cx, cy, radius);
        }
    }

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

void SdlRenderTarget::draw(const Drawable::Image::Ptr &image, const ScreenPos &position)
{
    if (!image) {
        WARN << "can't render null image";
        return;
    }

    const std::shared_ptr<const SdlImage> sdlImage = std::static_pointer_cast<const SdlImage>(image);
    if (!sdlImage->sdlTexture) {
        return;
    }

    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    SDL_Rect dst;
    dst.x = static_cast<int>(position.x);
    dst.y = static_cast<int>(position.y);
    dst.w = static_cast<int>(image->size.width * image->scaleX);
    dst.h = static_cast<int>(image->size.height * image->scaleY);

    SDL_RenderCopy(m_renderer, sdlImage->sdlTexture, nullptr, &dst);

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

void SdlRenderTarget::draw(const Drawable::Image::Ptr &image, const ScreenPos &position, const Drawable::BlendMode blendMode)
{
    if (!image) {
        WARN << "can't render null image";
        return;
    }

    const std::shared_ptr<const SdlImage> sdlImage = std::static_pointer_cast<const SdlImage>(image);
    if (!sdlImage->sdlTexture) {
        return;
    }

    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    // Temporarily change blend mode
    SDL_BlendMode prevBlend;
    SDL_GetTextureBlendMode(sdlImage->sdlTexture, &prevBlend);
    SDL_SetTextureBlendMode(sdlImage->sdlTexture, toSdlBlendMode(blendMode));

    SDL_Rect dst;
    dst.x = static_cast<int>(position.x);
    dst.y = static_cast<int>(position.y);
    dst.w = static_cast<int>(image->size.width * image->scaleX);
    dst.h = static_cast<int>(image->size.height * image->scaleY);

    SDL_RenderCopy(m_renderer, sdlImage->sdlTexture, nullptr, &dst);

    // Restore previous blend mode
    SDL_SetTextureBlendMode(sdlImage->sdlTexture, prevBlend);

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

void SdlRenderTarget::draw(const std::shared_ptr<IRenderTarget> &renderTarget, const ScreenPos &pos)
{
    if (!renderTarget) {
        WARN << "can't render null render target";
        return;
    }

    const std::shared_ptr<const SdlRenderTarget> sdlTarget = std::static_pointer_cast<const SdlRenderTarget>(renderTarget);
    if (!sdlTarget->m_texture) {
        return;
    }

    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    SDL_Rect dst;
    dst.x = static_cast<int>(pos.x);
    dst.y = static_cast<int>(pos.y);
    dst.w = static_cast<int>(sdlTarget->m_size.width);
    dst.h = static_cast<int>(sdlTarget->m_size.height);

    SDL_RenderCopy(m_renderer, sdlTarget->m_texture, nullptr, &dst);

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

void SdlRenderTarget::draw(const std::shared_ptr<IRenderTarget> &renderTarget, const Drawable::BlendMode blendMode)
{
    if (!renderTarget) {
        WARN << "can't render null render target";
        return;
    }

    const std::shared_ptr<const SdlRenderTarget> sdlTarget = std::static_pointer_cast<const SdlRenderTarget>(renderTarget);
    if (!sdlTarget->m_texture) {
        return;
    }

    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    SDL_BlendMode prevBlend;
    SDL_GetTextureBlendMode(sdlTarget->m_texture, &prevBlend);
    SDL_SetTextureBlendMode(sdlTarget->m_texture, toSdlBlendMode(blendMode));

    SDL_RenderCopy(m_renderer, sdlTarget->m_texture, nullptr, nullptr);

    SDL_SetTextureBlendMode(sdlTarget->m_texture, prevBlend);

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

void SdlRenderTarget::display()
{
    // No-op for texture targets; SdlWindow::display() calls SDL_RenderPresent
}

Drawable::Image::Ptr SdlRenderTarget::createImage(const Size &size, const uint8_t *bytes) const
{
    std::shared_ptr<SdlImage> ret = std::make_shared<SdlImage>();
    ret->renderer = m_renderer;
    ret->size = size;

    int w = static_cast<int>(size.width);
    int h = static_cast<int>(size.height);

    if (w <= 0 || h <= 0) {
        w = 10;
        h = 10;
    }

    ret->sdlTexture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        w, h
    );

    if (!ret->sdlTexture) {
        WARN << "Failed to create texture:" << SDL_GetError();
        return ret;
    }

    SDL_SetTextureBlendMode(ret->sdlTexture, SDL_BLENDMODE_BLEND);

    if (bytes) {
        SDL_UpdateTexture(ret->sdlTexture, nullptr, bytes, w * 4);
    } else {
        // Fill with transparent
        std::vector<uint8_t> transparent(w * h * 4, 0);
        SDL_UpdateTexture(ret->sdlTexture, nullptr, transparent.data(), w * 4);
    }

    return ret;
}

Drawable::Image::Ptr SdlRenderTarget::loadImage(const uint8_t *data, const size_t dataSize) const
{
    std::shared_ptr<SdlImage> ret = std::make_shared<SdlImage>();
    ret->renderer = m_renderer;

    SDL_RWops *rw = SDL_RWFromConstMem(data, static_cast<int>(dataSize));
    if (!rw) {
        WARN << "Failed to create RWops for image data";
        return ret;
    }

    SDL_Surface *surface = SDL_LoadBMP_RW(rw, 0);

    if (!surface) {
        // Try loading as raw RGBA if BMP fails - but SDL2 base only supports BMP.
        // For other formats (PNG, JPG), SDL_image would be needed.
        // Fall back: treat as raw data (won't work for most formats)
        SDL_RWclose(rw);
        WARN << "Failed to load image from memory:" << SDL_GetError();
        return ret;
    }

    SDL_RWclose(rw);

    // Convert to RGBA32 for consistency
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);

    if (!converted) {
        WARN << "Failed to convert surface format:" << SDL_GetError();
        return ret;
    }

    ret->sdlTexture = SDL_CreateTextureFromSurface(m_renderer, converted);
    ret->size = Size(static_cast<float>(converted->w), static_cast<float>(converted->h));

    SDL_FreeSurface(converted);

    if (ret->sdlTexture) {
        SDL_SetTextureBlendMode(ret->sdlTexture, SDL_BLENDMODE_BLEND);
    } else {
        WARN << "Failed to create texture from surface:" << SDL_GetError();
    }

    return ret;
}

std::shared_ptr<IRenderTarget> SdlRenderTarget::createTextureTarget(const Size &size) const
{
    return std::make_shared<SdlRenderTarget>(size, m_renderer);
}

Drawable::Text::Ptr SdlRenderTarget::createText(const Drawable::Text::Style style) const
{
    initFonts();

    std::shared_ptr<SdlText> ret = std::make_shared<SdlText>();
    ret->renderer = m_renderer;

    switch (style) {
    case Drawable::Text::UI:
        ret->fontData = s_uiFont.data;
        ret->fontDataSize = s_uiFont.dataSize;
        break;
    case Drawable::Text::Stylish:
        ret->fontData = s_stylishFont.data;
        ret->fontDataSize = s_stylishFont.dataSize;
        break;
    case Drawable::Text::Plain:
    default:
        ret->fontData = s_plainFont.data;
        ret->fontDataSize = s_plainFont.dataSize;
        break;
    }

    return ret;
}

void SdlRenderTarget::draw(const Drawable::Text::Ptr &text)
{
    if (!text) {
        WARN << "can't render null text";
        return;
    }

    std::shared_ptr<SdlText> sdlText = std::static_pointer_cast<SdlText>(text);
    sdlText->ensureFont();

    if (!sdlText->font) {
        return;
    }

    // Check if we need to re-render the cached texture
    bool needsRender = (sdlText->cachedTexture == nullptr) ||
                       (sdlText->cachedString != text->string) ||
                       (static_cast<int>(sdlText->cachedPointSize) != static_cast<int>(text->pointSize));

    if (needsRender) {
        sdlText->invalidateCache();

        if (text->string.empty()) {
            return;
        }

        SDL_Color sdlColor = { text->color.r, text->color.g, text->color.b, text->color.a };
        SDL_Surface *surface = nullptr;

        if (text->outlineColor.a > 0) {
            // Render with outline: render outline first, then text on top
            TTF_SetFontOutline(sdlText->font, 2);
            SDL_Color outlineCol = { text->outlineColor.r, text->outlineColor.g, text->outlineColor.b, text->outlineColor.a };
            SDL_Surface *outlineSurface = TTF_RenderUTF8_Blended(sdlText->font, text->string.c_str(), outlineCol);
            TTF_SetFontOutline(sdlText->font, 0);

            SDL_Surface *textSurface = TTF_RenderUTF8_Blended(sdlText->font, text->string.c_str(), sdlColor);

            if (outlineSurface && textSurface) {
                // Blit text onto outline surface
                SDL_Rect dstRect = { 2, 2, textSurface->w, textSurface->h };
                SDL_BlitSurface(textSurface, nullptr, outlineSurface, &dstRect);
                surface = outlineSurface;
                SDL_FreeSurface(textSurface);
            } else {
                if (outlineSurface) SDL_FreeSurface(outlineSurface);
                if (textSurface) SDL_FreeSurface(textSurface);
                surface = TTF_RenderUTF8_Blended(sdlText->font, text->string.c_str(), sdlColor);
            }
        } else {
            surface = TTF_RenderUTF8_Blended(sdlText->font, text->string.c_str(), sdlColor);
        }

        if (!surface) {
            WARN << "Failed to render text:" << TTF_GetError();
            return;
        }

        if (sdlText->cachedTexture) {
            SDL_DestroyTexture(sdlText->cachedTexture);
        }
        sdlText->cachedTexture = SDL_CreateTextureFromSurface(m_renderer, surface);
        sdlText->cachedWidth = surface->w;
        sdlText->cachedHeight = surface->h;
        sdlText->cachedString = text->string;
        sdlText->cachedPointSize = text->pointSize;
        SDL_FreeSurface(surface);

        if (!sdlText->cachedTexture) {
            WARN << "Failed to create text texture:" << SDL_GetError();
            return;
        }
    }

    if (!sdlText->cachedTexture) {
        return;
    }

    // Calculate position based on alignment
    ScreenPos position = text->position;

    if (text->alignment & Drawable::Text::AlignRight) {
        position.x = text->position.x - sdlText->cachedWidth;
    } else if (text->alignment & Drawable::Text::AlignHCenter) {
        position.x = text->position.x - sdlText->cachedWidth / 2.f;
    }

    if (text->alignment & Drawable::Text::AlignBottom) {
        position.y = text->position.y - sdlText->cachedHeight;
    } else if (text->alignment & Drawable::Text::AlignVCenter) {
        position.y = text->position.y - sdlText->cachedHeight / 2.f;
    }

    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    SDL_Rect dst;
    dst.x = static_cast<int>(position.x);
    dst.y = static_cast<int>(position.y);
    dst.w = sdlText->cachedWidth;
    dst.h = sdlText->cachedHeight;

    SDL_RenderCopy(m_renderer, sdlText->cachedTexture, nullptr, &dst);

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

void SdlRenderTarget::clear(const Drawable::Color &color)
{
    SDL_Texture *prevTarget = SDL_GetRenderTarget(m_renderer);
    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, m_texture);
    }

    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(m_renderer);

    if (m_isTextureTarget && m_texture) {
        SDL_SetRenderTarget(m_renderer, prevTarget);
    }
}

//------------------------------------------------------------------------------
// SdlWindow
//------------------------------------------------------------------------------
SdlWindow::SdlWindow(const Size size, const std::string &title)
{
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            WARN << "Failed to initialize SDL:" << SDL_GetError();
            return;
        }
    }

    if (!TTF_WasInit()) {
        if (TTF_Init() != 0) {
            WARN << "Failed to initialize SDL_ttf:" << TTF_GetError();
            return;
        }
    }

    Uint32 flags = SDL_WINDOW_SHOWN;
#ifdef ANDROID
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
    flags |= SDL_WINDOW_RESIZABLE;
#endif
    sdlWindow = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        static_cast<int>(size.width ? size.width : 1280),
        static_cast<int>(size.height ? size.height : 1024),
        flags
    );

    if (!sdlWindow) {
        WARN << "Failed to create SDL window:" << SDL_GetError();
        return;
    }

    sdlRenderer = SDL_CreateRenderer(
        sdlWindow, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!sdlRenderer) {
        WARN << "Failed to create SDL renderer:" << SDL_GetError();
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = nullptr;
        return;
    }

    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    // Create a render target that draws directly to the window (no off-screen texture)
    auto *target = new SdlRenderTarget(size, sdlRenderer);
    // For the window render target, we don't want an off-screen texture -
    // we draw directly to the renderer's default target.
    if (target->m_texture) {
        SDL_DestroyTexture(target->m_texture);
        target->m_texture = nullptr;
    }
    target->m_isTextureTarget = false;
    renderTarget.reset(target);
}

SdlWindow::~SdlWindow()
{
    // Release render target before destroying renderer/window
    renderTarget.reset();

    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = nullptr;
    }
    if (sdlWindow) {
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = nullptr;
    }
}

void SdlWindow::display()
{
    if (sdlRenderer) {
        SDL_RenderPresent(sdlRenderer);
    }
}

bool SdlWindow::isOpen() const
{
    return m_open && sdlWindow != nullptr;
}

void SdlWindow::close()
{
    m_open = false;
}

bool SdlWindow::pollEvent(input::Event &event)
{
    SDL_Event sdlEvent;
    if (!SDL_PollEvent(&sdlEvent)) {
        return false;
    }

    switch (sdlEvent.type) {
    case SDL_QUIT:
        event.type = input::Event::Closed;
        return true;

    case SDL_KEYDOWN:
        event.type = input::Event::KeyPressed;
        event.key.code = sdlKeyToInput(sdlEvent.key.keysym.sym);
        event.key.shift = (sdlEvent.key.keysym.mod & KMOD_SHIFT) != 0;
        event.key.control = (sdlEvent.key.keysym.mod & KMOD_CTRL) != 0;
        event.key.alt = (sdlEvent.key.keysym.mod & KMOD_ALT) != 0;
        return true;

    case SDL_KEYUP:
        event.type = input::Event::KeyReleased;
        event.key.code = sdlKeyToInput(sdlEvent.key.keysym.sym);
        event.key.shift = (sdlEvent.key.keysym.mod & KMOD_SHIFT) != 0;
        event.key.control = (sdlEvent.key.keysym.mod & KMOD_CTRL) != 0;
        event.key.alt = (sdlEvent.key.keysym.mod & KMOD_ALT) != 0;
        return true;

    case SDL_MOUSEBUTTONDOWN:
        event.type = input::Event::MouseButtonPressed;
        event.mouseButton.x = sdlEvent.button.x;
        event.mouseButton.y = sdlEvent.button.y;
        event.mouseButton.clicks = sdlEvent.button.clicks;
        switch (sdlEvent.button.button) {
        case SDL_BUTTON_LEFT:   event.mouseButton.button = input::MouseButton::Left; break;
        case SDL_BUTTON_RIGHT:  event.mouseButton.button = input::MouseButton::Right; break;
        case SDL_BUTTON_MIDDLE: event.mouseButton.button = input::MouseButton::Middle; break;
        default: event.mouseButton.button = input::MouseButton::Left; break;
        }
        return true;

    case SDL_MOUSEBUTTONUP:
        event.type = input::Event::MouseButtonReleased;
        event.mouseButton.x = sdlEvent.button.x;
        event.mouseButton.y = sdlEvent.button.y;
        event.mouseButton.clicks = sdlEvent.button.clicks;
        switch (sdlEvent.button.button) {
        case SDL_BUTTON_LEFT:   event.mouseButton.button = input::MouseButton::Left; break;
        case SDL_BUTTON_RIGHT:  event.mouseButton.button = input::MouseButton::Right; break;
        case SDL_BUTTON_MIDDLE: event.mouseButton.button = input::MouseButton::Middle; break;
        default: event.mouseButton.button = input::MouseButton::Left; break;
        }
        return true;

    case SDL_MOUSEMOTION:
        event.type = input::Event::MouseMoved;
        event.mouseMove.x = sdlEvent.motion.x;
        event.mouseMove.y = sdlEvent.motion.y;
        return true;

    case SDL_MOUSEWHEEL:
        event.type = input::Event::MouseWheelScrolled;
        event.mouseWheel.delta = static_cast<float>(sdlEvent.wheel.y);
        {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            event.mouseWheel.x = mx;
            event.mouseWheel.y = my;
        }
        return true;

    case SDL_FINGERDOWN: {
        event.type = input::Event::TouchBegan;
        int ww, wh;
        SDL_GetWindowSize(sdlWindow, &ww, &wh);
        event.touch.finger = static_cast<int>(sdlEvent.tfinger.fingerId);
        int px = static_cast<int>(sdlEvent.tfinger.x * ww);
        int py = static_cast<int>(sdlEvent.tfinger.y * wh);
        int lw, lh;
        SDL_RenderGetLogicalSize(sdlRenderer, &lw, &lh);
        event.touch.x = (lw > 0) ? px * lw / ww : px;
        event.touch.y = (lh > 0) ? py * lh / wh : py;
        return true;
    }

    case SDL_FINGERUP: {
        event.type = input::Event::TouchEnded;
        int ww, wh;
        SDL_GetWindowSize(sdlWindow, &ww, &wh);
        event.touch.finger = static_cast<int>(sdlEvent.tfinger.fingerId);
        int px = static_cast<int>(sdlEvent.tfinger.x * ww);
        int py = static_cast<int>(sdlEvent.tfinger.y * wh);
        int lw, lh;
        SDL_RenderGetLogicalSize(sdlRenderer, &lw, &lh);
        event.touch.x = (lw > 0) ? px * lw / ww : px;
        event.touch.y = (lh > 0) ? py * lh / wh : py;
        return true;
    }

    case SDL_FINGERMOTION: {
        event.type = input::Event::TouchMoved;
        int ww, wh;
        SDL_GetWindowSize(sdlWindow, &ww, &wh);
        event.touch.finger = static_cast<int>(sdlEvent.tfinger.fingerId);
        int px = static_cast<int>(sdlEvent.tfinger.x * ww);
        int py = static_cast<int>(sdlEvent.tfinger.y * wh);
        int lw, lh;
        SDL_RenderGetLogicalSize(sdlRenderer, &lw, &lh);
        event.touch.x = (lw > 0) ? px * lw / ww : px;
        event.touch.y = (lh > 0) ? py * lh / wh : py;
        return true;
    }

    case SDL_MULTIGESTURE: {
        if (sdlEvent.mgesture.numFingers == 2) {
            event.type = input::Event::PinchZoom;
            event.pinch.dDist = sdlEvent.mgesture.dDist;
            return true;
        }
        return false;
    }

    case SDL_TEXTINPUT: {
        event.type = input::Event::TextEntered;
        // Decode first UTF-8 character to a Unicode code point
        const uint8_t *bytes = reinterpret_cast<const uint8_t*>(sdlEvent.text.text);
        uint32_t codepoint = 0;
        if (bytes[0] < 0x80) {
            codepoint = bytes[0];
        } else if ((bytes[0] & 0xE0) == 0xC0) {
            codepoint = (bytes[0] & 0x1F) << 6;
            codepoint |= (bytes[1] & 0x3F);
        } else if ((bytes[0] & 0xF0) == 0xE0) {
            codepoint = (bytes[0] & 0x0F) << 12;
            codepoint |= (bytes[1] & 0x3F) << 6;
            codepoint |= (bytes[2] & 0x3F);
        } else if ((bytes[0] & 0xF8) == 0xF0) {
            codepoint = (bytes[0] & 0x07) << 18;
            codepoint |= (bytes[1] & 0x3F) << 12;
            codepoint |= (bytes[2] & 0x3F) << 6;
            codepoint |= (bytes[3] & 0x3F);
        }
        event.text.unicode = codepoint;
        return true;
    }

    default:
        break;
    }

    return false;
}
