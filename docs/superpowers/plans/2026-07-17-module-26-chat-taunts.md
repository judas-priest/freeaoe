# Module 26: Chat & Taunts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add in-game chat with AoE2 taunts (1-42) that play sound effects.

**Architecture:** Chat input via Enter key opens a text field. Messages broadcast via existing `GameCommand::Chat` network command. Taunts detected by number prefix (e.g., typing "1" sends taunt 1 = "Yes"). Taunts play a sound for all players. Chat history displayed as overlay text with fade-out.

**Tech Stack:** C++20, SDL2 text input, existing network command system

---

## Background

- `GameCommand::Chat` type already exists (`net/GameCommand.h:33`)
- AoE2 has 42 taunts: "1" = "Yes", "2" = "No", "3" = "Food please", etc.
- Taunt sounds are in the sound DRS
- Lockstep networking already serializes commands — chat is just another command type
- SDL2 has `SDL_StartTextInput()` / `SDL_TEXTINPUT` event for text entry

## Key Files

- `src/net/GameCommand.h:33` — Chat command type
- `src/net/LockstepManager.h` — Command broadcasting
- `src/Engine.cpp` — Input handling, rendering
- `src/audio/AudioPlayer.h` — Sound playback

---

### Task 1: Chat message data structure

**Files:**
- Modify: `src/net/GameCommand.h`

- [ ] **Step 1: Add chat data to GameCommand**

In `GameCommand.h`, add chat-specific data. The command already has a `Chat` type. Add a string field:

```cpp
std::string chatMessage;
int chatPlayerId = -1;
```

Also add serialization for the chat message in the serialize/deserialize methods (for multiplayer).

- [ ] **Step 2: Build**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 3: Commit**

```bash
git add src/net/GameCommand.h
git commit -m "feat: add chat message data to GameCommand"
```

---

### Task 2: Chat input UI

**Files:**
- Create: `src/ui/ChatOverlay.h`
- Create: `src/ui/ChatOverlay.cpp`

- [ ] **Step 1: Create ChatOverlay class**

`src/ui/ChatOverlay.h`:

```cpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

struct SDL_Renderer;

class ChatOverlay
{
public:
    struct Message {
        std::string text;
        int playerId;
        uint32_t timestamp;
    };

    void startInput();
    void stopInput();
    bool isInputActive() const { return m_inputActive; }

    void handleTextInput(const char *text);
    void handleKeyPress(int key); // SDL_SCANCODE_RETURN, BACKSPACE, ESCAPE
    std::string consumeSubmittedMessage(); // Returns message and clears

    void addMessage(const std::string &text, int playerId, uint32_t time);
    void render(SDL_Renderer *renderer, int screenWidth, int screenHeight, uint32_t time);

private:
    bool m_inputActive = false;
    std::string m_inputBuffer;
    std::string m_submittedMessage;
    std::vector<Message> m_messages;

    static constexpr uint32_t MESSAGE_DISPLAY_TIME = 15000; // 15s
    static constexpr int MAX_MESSAGES = 10;
};
```

- [ ] **Step 2: Implement ChatOverlay**

`src/ui/ChatOverlay.cpp`:

```cpp
#include "ChatOverlay.h"
#include <SDL2/SDL.h>

void ChatOverlay::startInput()
{
    m_inputActive = true;
    m_inputBuffer.clear();
    SDL_StartTextInput();
}

void ChatOverlay::stopInput()
{
    m_inputActive = false;
    m_inputBuffer.clear();
    SDL_StopTextInput();
}

void ChatOverlay::handleTextInput(const char *text)
{
    if (m_inputActive) {
        m_inputBuffer += text;
    }
}

void ChatOverlay::handleKeyPress(int key)
{
    if (!m_inputActive) return;

    if (key == SDL_SCANCODE_RETURN) {
        if (!m_inputBuffer.empty()) {
            m_submittedMessage = m_inputBuffer;
        }
        stopInput();
    } else if (key == SDL_SCANCODE_ESCAPE) {
        stopInput();
    } else if (key == SDL_SCANCODE_BACKSPACE) {
        if (!m_inputBuffer.empty()) {
            m_inputBuffer.pop_back();
        }
    }
}

std::string ChatOverlay::consumeSubmittedMessage()
{
    std::string msg = std::move(m_submittedMessage);
    m_submittedMessage.clear();
    return msg;
}

void ChatOverlay::addMessage(const std::string &text, int playerId, uint32_t time)
{
    m_messages.push_back({text, playerId, time});
    if (m_messages.size() > MAX_MESSAGES) {
        m_messages.erase(m_messages.begin());
    }
}

void ChatOverlay::render(SDL_Renderer *renderer, int screenWidth, int screenHeight, uint32_t time)
{
    // Remove expired messages
    m_messages.erase(
        std::remove_if(m_messages.begin(), m_messages.end(),
            [time](const Message &m) { return time - m.timestamp > MESSAGE_DISPLAY_TIME; }),
        m_messages.end());

    // Render messages at bottom-left of screen
    // Use existing text rendering (SDL_RenderCopy with pre-rendered text textures)
    // Implementation depends on the existing text rendering API in freeaoe

    // If input active, render input box at bottom
    if (m_inputActive) {
        // Draw semi-transparent background bar
        SDL_Rect inputBg = {0, screenHeight - 30, screenWidth / 2, 30};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(renderer, &inputBg);
        // Render m_inputBuffer text at inputBg position
        // Use existing text rendering system
    }
}
```

- [ ] **Step 3: Build**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/ui/ChatOverlay.h src/ui/ChatOverlay.cpp
git commit -m "feat: ChatOverlay UI for in-game chat input and display"
```

---

### Task 3: Taunt detection and sound playback

**Files:**
- Modify: `src/ui/ChatOverlay.h`
- Modify: `src/ui/ChatOverlay.cpp`

- [ ] **Step 1: Add taunt map**

In `ChatOverlay.cpp`, add a static taunt table:

```cpp
static const std::unordered_map<int, std::string> TAUNTS = {
    {1, "Yes"}, {2, "No"}, {3, "Food please"}, {4, "Wood please"},
    {5, "Gold please"}, {6, "Stone please"}, {7, "Ahh!"},
    {8, "All hail, king of the losers!"}, {9, "Ooh!"},
    {10, "I'll beat you back to Age of Empires"},
    {11, "(Herb Laugh)"}, {12, "Being rushed"},
    {13, "Sure, blame it on your ISP"}, {14, "Start the game already!"},
    {15, "Don't point that thing at me!"},
    {16, "Enemy sighted!"}, {17, "It is good to be the king"},
    {18, "Monk! I need a monk!"}, {19, "Long time, no siege"},
    {20, "My granny could scrap better than that"},
    {21, "Nice town, I'll take it"}, {22, "Quit touching me!"},
    {23, "Raiding party!"}, {24, "Dadgum"},
    {25, "Eh, smite me"}, {26, "The wonder, the wonder, the... no"},
    {27, "You played two hours to die like this?"},
    {28, "Yeah, well, you should see the other guy"},
    {29, "Roggan"}, {30, "Wololo"},
    {31, "Attack an enemy now"}, {32, "Cease creating extra villagers"},
    {33, "Create extra villagers"}, {34, "Build a navy"},
    {35, "Stop building a navy"}, {36, "Wait for my signal to attack"},
    {37, "Build a wonder"}, {38, "Give me your extra resources"},
    {39, "(Ally sound)"}, {40, "(Enemy sound)"},
    {41, "(Neutral sound)"}, {42, "What age are you in?"}
};
```

- [ ] **Step 2: Detect taunt in submitted message**

Add a method to check if a message is a taunt:

```cpp
int ChatOverlay::detectTaunt(const std::string &message)
{
    // Try to parse as integer
    try {
        int num = std::stoi(message);
        if (num >= 1 && num <= 42) return num;
    } catch (...) {}
    return 0;
}
```

When a message is submitted and it's a taunt, replace the message text with the taunt string and mark it for sound playback.

- [ ] **Step 3: Build and test**

```bash
cd build && make -j$(nproc)
```

Press Enter, type "30", press Enter. Should display "Wololo" in chat.

- [ ] **Step 4: Commit**

```bash
git add src/ui/ChatOverlay.h src/ui/ChatOverlay.cpp
git commit -m "feat: detect AoE2 taunts (1-42) in chat messages"
```

---

### Task 4: Wire chat to Engine and network

**Files:**
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`

- [ ] **Step 1: Add ChatOverlay to Engine**

```cpp
#include "ui/ChatOverlay.h"
std::unique_ptr<ChatOverlay> m_chatOverlay;
```

Initialize in Engine constructor. In key handler, Enter key opens chat:

```cpp
case SDL_SCANCODE_RETURN:
    if (!m_chatOverlay->isInputActive()) {
        m_chatOverlay->startInput();
    }
    break;
```

Route `SDL_TEXTINPUT` events to chat overlay. Route keyboard events when chat is active.

- [ ] **Step 2: Send chat as GameCommand**

In Engine update loop:

```cpp
std::string msg = m_chatOverlay->consumeSubmittedMessage();
if (!msg.empty()) {
    GameCommand cmd;
    cmd.type = GameCommand::Chat;
    cmd.chatMessage = msg;
    cmd.chatPlayerId = m_humanPlayer->playerId;
    m_lockstep->sendCommand(cmd);
}
```

- [ ] **Step 3: Receive and display chat**

In the command execution handler, when a Chat command is received:

```cpp
case GameCommand::Chat:
    m_chatOverlay->addMessage(cmd.chatMessage, cmd.chatPlayerId, currentTime);
    int taunt = ChatOverlay::detectTaunt(cmd.chatMessage);
    if (taunt > 0) {
        m_audioPlayer->playSound(tauntSoundId(taunt));
    }
    break;
```

- [ ] **Step 4: Render chat overlay**

In the render loop, after UI:

```cpp
m_chatOverlay->render(renderer, screenWidth, screenHeight, currentTime);
```

- [ ] **Step 5: Build and test**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 6: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "feat: in-game chat with taunts over network"
```
