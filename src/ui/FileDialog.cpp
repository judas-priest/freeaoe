#include "FileDialog.h"

#include "core/Logger.h"
#include "core/Utility.h"
#include "global/Config.h"
#ifdef USE_SDL2
#include "render/SdlRenderTarget.h"
#else
#include "render/SfmlRenderTarget.h"
#ifndef USE_SDL2
#include <SFML/Graphics/RenderWindow.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Graphics/Color.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Graphics/Text.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Graphics/View.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Window/Event.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Window/Keyboard.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Window/Mouse.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Window/VideoMode.hpp>
#endif
#endif
#include "misc/images/parchment.jpg.h"

#include <stddef.h>
#include <algorithm>
#include <system_error>
#include <utility>

#ifndef USE_SDL2
static input::Event convertSfEventFD(const sf::Event &sfEvent) {
    input::Event ev{};
    switch(sfEvent.type) {
    case sf::Event::Closed: ev.type = input::Event::Closed; break;
    case sf::Event::MouseButtonPressed:
        ev.type = input::Event::MouseButtonPressed;
        ev.mouseButton.button = sfEvent.mouseButton.button == sf::Mouse::Right ? input::MouseButton::Right :
                                sfEvent.mouseButton.button == sf::Mouse::Middle ? input::MouseButton::Middle : input::MouseButton::Left;
        ev.mouseButton.x = sfEvent.mouseButton.x;
        ev.mouseButton.y = sfEvent.mouseButton.y;
        break;
    case sf::Event::MouseButtonReleased:
        ev.type = input::Event::MouseButtonReleased;
        ev.mouseButton.button = sfEvent.mouseButton.button == sf::Mouse::Right ? input::MouseButton::Right :
                                sfEvent.mouseButton.button == sf::Mouse::Middle ? input::MouseButton::Middle : input::MouseButton::Left;
        ev.mouseButton.x = sfEvent.mouseButton.x;
        ev.mouseButton.y = sfEvent.mouseButton.y;
        break;
    case sf::Event::MouseMoved:
        ev.type = input::Event::MouseMoved;
        ev.mouseMove.x = sfEvent.mouseMove.x;
        ev.mouseMove.y = sfEvent.mouseMove.y;
        break;
    case sf::Event::MouseWheelScrolled:
        ev.type = input::Event::MouseWheelScrolled;
        ev.mouseWheel.delta = sfEvent.mouseWheelScroll.delta;
        ev.mouseWheel.x = sfEvent.mouseWheelScroll.x;
        ev.mouseWheel.y = sfEvent.mouseWheelScroll.y;
        break;
    default: ev.type = input::Event::Closed; break;
    }
    return ev;
}
#endif // !USE_SDL2

FileDialog::FileDialog()
{
}

FileDialog::~FileDialog() { }

bool FileDialog::setup(int width, int height)
{
#ifdef USE_SDL2
    m_window = std::make_unique<SdlWindow>(Size(width, height), "freeaoe");
#else
    m_window = std::make_unique<SfmlWindow>(Size(width, height), "freeaoe");
#endif
    {
        m_description = m_window->renderTarget->createText(Drawable::Text::UI);
        m_description->string = "Please select the directory containing your Age of Empires 2 installation.";
        m_description->pointSize = 20;
        m_description->position.x = width/2.f;
        m_description->position.y = 20;
        m_description->alignment = Drawable::Text::AlignHCenter;
        m_description->color = Drawable::Black;
    }

    const Size buttonSize(250, 50);

    m_fileList = std::make_unique<ListView>(ScreenRect(ScreenPos(width / 2.f - width * 3.f/8.f, 75), Size(width * 3.f/4.f, 550)), *m_window->renderTarget);
    m_fileList->setCurrentPath(std::filesystem::current_path().string());

    m_okButton = std::make_unique<Button>("OK", ScreenRect(ScreenPos(m_fileList->rect().x, 710), buttonSize), *m_window->renderTarget);

    m_cancelButton = std::make_unique<Button>("Cancel", ScreenRect(ScreenPos(m_fileList->rect().right() - buttonSize.width, 710), buttonSize), *m_window->renderTarget);
    m_cancelButton->enabled = true;

    m_openDownloadUrlButton = std::make_unique<Button>("Download trial version", ScreenRect(ScreenPos(m_fileList->rect().center().x - buttonSize.width/2, 710), buttonSize), *m_window->renderTarget);
    m_openDownloadUrlButton->enabled = true;

    m_backButton = std::make_unique<Button>("Back", ScreenRect(ScreenPos(m_fileList->rect().x - buttonSize.width/2, m_fileList->rect().y), Size(buttonSize.width / 2, buttonSize.height)), *m_window->renderTarget);
    m_backButton->enabled = true;

#if defined(__linux__)
    m_winePath = Config::winePath();
    if (!m_winePath.empty()) {
        if (std::filesystem::exists(m_winePath + "/drive_c")) {
            m_winePath += "/drive_c";
        }
        m_goToWineButton = std::make_unique<Button>("Go to Wine folder", ScreenRect(ScreenPos(m_fileList->rect().x, 650), buttonSize), *m_window->renderTarget);
        m_goToWineButton->enabled = true;
    }
#endif
    m_bgImage = m_window->renderTarget->loadImage(resource_parchment_jpg_data, resource_parchment_jpg_size);

    m_pathText = m_window->renderTarget->createText();
    m_pathText->pointSize = 15;
    m_pathText->position.x = m_fileList->rect().x + 5;
    m_pathText->position.y = 50;

    return true;
}

std::string FileDialog::getPath()
{
    std::string ret;

    while (m_window->isOpen()) {
        // Process events
#ifdef USE_SDL2
        input::Event event;
        while (m_window->pollEvent(event)) {
            if (event.type == input::Event::Closed) {
                m_window->close();
                continue;
            }

            if (m_openDownloadUrlButton->checkClick(event)) {
                util::openUrl("https://archive.org/details/AgeOfEmpiresIiTheConquerorsDemo", nullptr);
                continue;
            }

            if (m_cancelButton->checkClick(event)) {
                m_window->close();
            }

            if (m_okButton->checkClick(event)) {
                m_window->close();
                ret = m_fileList->currentText();
            }

            if (m_backButton->checkClick(event)) {
                m_fileList->setCurrentPath(m_fileList->pathHistory.back());
                m_fileList->pathHistory.pop_back();
            }

#if defined(__linux__)
            if (m_goToWineButton) {
                if (m_goToWineButton->checkClick(event)) {
                    m_fileList->setCurrentPath(std::filesystem::path(m_winePath));
                }
            }
#endif

            m_fileList->handleEvent(event);
        }
#else
        sf::Event sfEvent;
        if (!m_window->window->waitEvent(sfEvent)) {
            WARN << "failed to get event";
            break;
        }

        if (sfEvent.type == sf::Event::Closed) {
            m_window->close();
            continue;
        }

        input::Event event = convertSfEventFD(sfEvent);

        if (m_openDownloadUrlButton->checkClick(event)) {
            util::openUrl("https://archive.org/details/AgeOfEmpiresIiTheConquerorsDemo", nullptr);
            continue;
        }

        if (m_cancelButton->checkClick(event)) {
            m_window->close();
        }

        if (m_okButton->checkClick(event)) {
            m_window->close();
            ret = m_fileList->currentText();
        }

        if (m_backButton->checkClick(event)) {
            m_fileList->setCurrentPath(m_fileList->pathHistory.back());
            m_fileList->pathHistory.pop_back();
        }

#if defined(__linux__)
        if (m_goToWineButton) {
            if (m_goToWineButton->checkClick(event)) {
                m_fileList->setCurrentPath(std::filesystem::path(m_winePath));
            }
        }
#endif

        m_fileList->handleEvent(event);
#endif

        m_pathText->string = m_fileList->currentText();
        m_okButton->enabled = m_fileList->hasDataFolder;
        m_backButton->enabled = !m_fileList->pathHistory.empty();

        m_window->renderTarget->clear(Drawable::Color(32, 32, 32));
        m_window->renderTarget->draw(m_bgImage, {0, 0});
        m_window->renderTarget->draw(m_pathText);
        m_openDownloadUrlButton->render(m_window->renderTarget);
        m_cancelButton->render(m_window->renderTarget);
        m_okButton->render(m_window->renderTarget);
        m_backButton->render(m_window->renderTarget);
        m_fileList->render(m_window->renderTarget);
        m_window->renderTarget->draw(m_description);
        if (m_errorText) {
            m_window->renderTarget->draw(m_errorText);
        }

#if defined(__linux__)
        if (m_goToWineButton) {
            m_goToWineButton->render(m_window->renderTarget);
        }
#endif
        m_window->display();

#ifdef USE_SDL2
        SDL_Delay(16); // ~60fps cap for polling mode
#endif
    }

    return ret;
}

void FileDialog::setErrorString(const std::string &error) noexcept
{
    m_errorText = m_window->renderTarget->createText(Drawable::Text::UI);
    m_errorText->string = "Failed to load game data: " + error;
    m_errorText->pointSize = 17;
    m_errorText->alignment = Drawable::Text::AlignHCenter;
    m_errorText->position.x = m_window->renderTarget->getSize().width/2;
    m_errorText->color = Drawable::Color(100, 32, 32);
}

Button::Button(const std::string &text, const ScreenRect &rect, const IRenderTarget &renderTarget) :
    m_rect(rect)
{
    m_text = renderTarget.createText();
    m_text->string = text;
    m_text->pointSize = 24;
    const int textWidth = m_text->size().width;
    const int textHeight = m_text->size().height;
    m_text->position.x = m_rect.x + (m_rect.width / 2. - textWidth / 2.);
    m_text->position.y = m_rect.y + (m_rect.height / 2. - textHeight);

    m_background.borderSize = 3;
    m_background.rect = m_rect;
}

bool Button::checkClick(const input::Event &event)
{
    if (!enabled) {
        m_pressed = false;
        return false;
    }

    if (event.type == input::Event::MouseMoved) {
        ScreenPos mousePos(event.mouseMove.x, event.mouseMove.y);

        if (!m_rect.contains(mousePos)) {
            m_pressed = false;
        }

        return false;
    }

    if (event.type != input::Event::MouseButtonPressed && event.type != input::Event::MouseButtonReleased) {
        return false;
    }

    ScreenPos mousePos(event.mouseButton.x, event.mouseButton.y);

    if (!m_rect.contains(mousePos)) {
        m_pressed = false;

        return false;
    }

    if (event.type == input::Event::MouseButtonPressed) {
        m_pressed = true;
        return false;
    }

    return m_pressed;
}

void Button::render(const std::unique_ptr<IRenderTarget> &window)
{
    if (!enabled) {
        m_background.borderColor = Drawable::Color(64, 64, 64);
        m_background.fillColor = Drawable::Color(64, 64, 64, 128);
        m_background.filled = true;
        m_text->outlineColor = Drawable::Transparent;
        m_text->color = Drawable::Color(64, 64, 64);
    } else if (m_pressed) {
        m_background.borderColor = Drawable::Black;
        m_background.fillColor = {0, 0, 0, 200};
        m_background.filled = true;
        m_text->outlineColor = Drawable::Transparent;
        m_text->color = Drawable::White;
    } else {
        m_background.borderColor = Drawable::Color(0, 0, 0, 64);
        m_background.fillColor = {0, 0, 0, 128};
        m_background.filled = true;
        m_text->outlineColor = Drawable::Black;
        m_text->color = {0xda, 0xd1, 0xb2};
    }
    window->draw(m_background);
    window->draw(m_text);
}

ListView::ListView(const ScreenRect rect, const IRenderTarget &window) :
    m_rect(rect)
{
    m_itemHeight = rect.height / numVisible;

    for (int i=0; i<numVisible; i++) {
        Drawable::Text::Ptr text = window.createText();
        text->pointSize = 18;
        text->outlineColor = Drawable::Transparent;
        const int textHeight = text->size().height;
        text->position.x = rect.x + 10;
        text->position.y = rect.y + i * m_itemHeight + textHeight/3.f;

        m_texts.push_back(std::move(text));
    }

    m_background.borderSize = 3;
    m_background.rect = m_rect;
    m_background.filled = true;
    m_background.fillColor = {0, 0, 0, 200};
    m_background.borderColor = Drawable::Black;

    m_selectedOutline.borderSize = 1;
    m_selectedOutline.rect.setTopLeft(m_rect.topLeft());
    m_selectedOutline.fillColor = {0xda, 0xd1, 0xb2};
    m_selectedOutline.filled = true;
    m_selectedOutline.borderColor = Drawable::Transparent;

    m_scrollBar.borderSize = 0;
    m_scrollBar.rect  = ScreenRect(ScreenPos(m_rect.topRight() + ScreenPos(-20, 5)),
                                   Size(0, 10));
    m_scrollBar.fillColor = Drawable::Color(128, 128, 128, 200);
    m_scrollBar.filled = true;
    m_scrollBar.borderColor = {0, 0, 0, 32};

    updateScrollbar();
}

void ListView::handleEvent(const input::Event &event)
{
    if (event.type == input::Event::MouseMoved) {
        ScreenPos mousePos(event.mouseMove.x, event.mouseMove.y);

        if (m_pressed) {
            moveScrollbar(mousePos.y);
        }

        return;
    }

    if (event.type == input::Event::MouseButtonReleased) {
        m_scrollBar.fillColor = Drawable::Color(255, 255, 255, 200);
        m_pressed = false;
        return;
    }

    if (event.type == input::Event::MouseWheelScrolled) {
        if (event.mouseWheel.delta < 0) {
            setOffset(m_offset + 1);
        } else {
            setOffset(m_offset - 1);
        }
    }

    if (event.type != input::Event::MouseButtonPressed) {
        return;
    }

    ScreenPos mousePos(event.mouseButton.x, event.mouseButton.y);
    if (!m_rect.contains(mousePos)) {
        return;
    }

    // scrollbar hit
    if (mousePos.x > m_rect.x + m_rect.width - 20) {
        m_scrollBar.fillColor = Drawable::White;
        moveScrollbar(mousePos.y);
        m_pressed = true;
        return;
    }

    int index = (event.mouseButton.y - m_rect.y) / m_itemHeight + m_offset;

    if (index >= m_list.size()) {
        return;
    }

    // Didn't click the same item
    if (index != m_currentItem) {
        m_currentItem = index;
        return;
    }

    if (std::filesystem::is_directory(m_list[m_currentItem])) {
        pathHistory.push_back(m_currentPath.string());
        setCurrentPath(std::filesystem::canonical(m_list[m_currentItem]).string());
    }
}

void ListView::render(const std::unique_ptr<IRenderTarget> &window)
{
    window->draw(m_background);

    for (size_t i=0; i<m_texts.size(); i++) {
        if (i+m_offset >= m_list.size()) {
            continue;
        }

        if (std::filesystem::is_directory(m_list[i+m_offset])) {
            m_texts[i]->color = Drawable::White;
        } else {
            m_texts[i]->color = Drawable::Color(128, 128, 128);
        }
    }

    if (m_currentItem - m_offset >= 0 && m_currentItem - m_offset < numVisible) {
        m_selectedOutline.rect.setSize(Size(m_rect.width, m_itemHeight));
        m_selectedOutline.rect.setTopLeft(ScreenPos(m_rect.x, m_rect.y + (m_currentItem - m_offset) * m_itemHeight));
        window->draw(m_selectedOutline);

        m_texts[m_currentItem - m_offset]->color = Drawable::Black;
    }

    for (Drawable::Text::Ptr &text : m_texts) {
        window->draw(text);
    }

    window->draw(m_scrollBar);
}

void ListView::setCurrentPath(std::string pathString)
{
    if (pathString.empty()) {
        pathString = "/";
    }

    std::filesystem::path path(pathString);

    m_list.clear();

    hasDataFolder = false;
    try {

        // avoid crashing
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
            throw std::filesystem::filesystem_error("fuckings to boost which I blame for this shitty API", std::error_code());
        }

        std::filesystem::directory_iterator boostIsShit(path);
        for (const std::filesystem::directory_entry &entry : boostIsShit) try {
            const std::string entryName = entry.path().filename().string();
            if ((entryName == "Data" || entryName == "resources") && entry.is_directory()) {
                hasDataFolder = true;
            }

            m_list.push_back(entry.path().string());

            // MSVC and/or Wine has a troubled relationship with UTF-8 path names
        } catch (const std::exception &e) { WARN << "error adding dir entry, probably windows shit" << e.what(); }
    } catch (const std::filesystem::filesystem_error &err) {
        WARN << "Err" << err.what();
        if (m_currentPath.empty()) {
            WARN << "not even a current path, blame boost";
            return;
        }

        pathString = m_currentPath.string();
    }
    path = std::filesystem::path(pathString);

    if (!path.parent_path().empty() && pathString != path.parent_path()) {
        std::filesystem::path dotdot = pathString;
        dotdot += "/..";
        m_list.push_back(dotdot.string());
    }

    std::sort(m_list.begin(), m_list.end(), [](const std::filesystem::path &a, const std::filesystem::path &b){
        const int aIsNotDir = !std::filesystem::is_directory(a);
        const int bIsNotDir = !std::filesystem::is_directory(b);
        if (aIsNotDir != bIsNotDir) {
            return aIsNotDir < bIsNotDir;
        }

        const std::string aName = util::toLowercase(a.filename().string());
        const std::string bName = util::toLowercase(b.filename().string());
        const bool aDot = aName[0] == '.' && (aName.size() < 2 || aName[1] != '.');
        const bool bDot = bName[0] == '.' && (bName.size() < 2 || bName[1] != '.');
        if (aDot != bDot) {
            return aDot < bDot;
        }

        // clang-tidy for some reason believes the < should be a nullptr
        return a.filename() < b.filename(); // NOLINT
    });

    m_currentItem = 0;
    m_offset = 0;

    for (size_t i=0; i<m_texts.size(); i++) {
        if (i < m_list.size()) {
            std::string filename = std::filesystem::path(m_list[i]).filename().string();
            if (filename.size() > 40) {
                filename.resize(40);
                filename += "...";
            }
            if (std::filesystem::is_directory(m_list[i])) {
                m_texts[i]->string = "[" + filename + "]";
            } else{
                m_texts[i]->string = filename;
            }
        } else {
            m_texts[i]->string.clear();
        }
    }

    updateScrollbar();

    m_currentPath = pathString;
}

void ListView::setOffset(int offset)
{
    if (offset > int(m_list.size()) - numVisible) {
        offset = int(m_list.size()) - numVisible;
    }

    if (offset < 0) {
        offset = 0;
    }

    if (offset == m_offset) {
        return;
    }

    m_offset = offset;

    for (size_t i=0; i<m_texts.size(); i++) {
        if (i+m_offset < m_list.size()) {
            std::string filename = std::filesystem::path(m_list[i+m_offset]).filename().string();
            if (filename.size() > 40) {
                filename.resize(40);
                filename += "...";
            }
            if (std::filesystem::is_directory(m_list[i+m_offset])) {
                m_texts[i]->string = "[" + filename + "]";
            } else{
                m_texts[i]->string = filename;
            }
        } else {
            m_texts[i]->string.clear();
        }
    }

    updateScrollbar();
}

std::string ListView::currentText() const
{
    return std::filesystem::canonical(m_currentPath).string();
}

void ListView::updateScrollbar()
{
    if (m_list.empty()) {
        m_scrollBar.rect.setSize(Size(0, 0));
        return;
    }

    float scrollbarSize =  m_rect.height * (float(numVisible) / m_list.size());
    float width = 20;
    if (scrollbarSize >= m_rect.height) {
        m_scrollBar.rect.setSize(Size(0, 0));
        return;
    }

    scrollbarSize = std::max(scrollbarSize, 10.f);
    m_scrollBar.rect.setSize(Size(width, scrollbarSize));

    int scrollbarPos = m_rect.height * float(m_offset) / m_list.size();
    m_scrollBar.rect.setTopLeft(m_rect.topRight() + ScreenPos(-20, scrollbarPos));
}

void ListView::moveScrollbar(int mouseY)
{
    float ratio = (mouseY - m_scrollBar.rect.width/2 - m_rect.y) / m_rect.height;
    setOffset(m_list.size() * ratio);
}
