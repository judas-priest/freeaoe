#pragma once

#include <memory>

#include "core/Types.h"
#include "render/IRenderTarget.h"

namespace genie {
class SlpFile;

typedef std::shared_ptr<SlpFile> SlpFilePtr;
}

class IRenderTarget;

struct IconButton
{
    enum Type {
        Invalid = -1,
        GameMenu = 0,
        Diplo = 1,
        Chat = 2,
        TechTree = 3,
        Settings = 4,
        ButtonsCount
    };

    IconButton(std::shared_ptr<IRenderTarget> renderTarget);

    Type type() const { return m_type; }
    bool setType(const Type type);
    bool setPosition(const ScreenPos &position);
    bool onMousePressed(const ScreenPos &mousePosition);
    bool onMouseReleased(const ScreenPos &mousePosition);
    void render() const;

    const ScreenRect &rect() const { return m_rect; }

private:
    bool setPressed(const bool isPressed);

    ScreenRect m_rect;

    Drawable::Image::Ptr texture;
    Drawable::Image::Ptr pressedTexture;

    bool m_pressed = false;
    std::shared_ptr<IRenderTarget> m_renderTarget;
    ScreenPos m_position;
    genie::SlpFilePtr m_buttonsFile;
    Type m_type = Invalid;
};


