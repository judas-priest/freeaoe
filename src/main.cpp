/*
    Main entry point.
    Copyright (C) 2017-2020 Martin Sandsmark <martin.sandsmark@kde.org>

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

#ifdef USE_SDL2
#include <SDL2/SDL.h>
#endif
#ifdef ANDROID
#include <android/log.h>
#include <unistd.h>
#include <thread>
#endif

#include <genie/script/ScnFile.h>
#include "ui/ScenarioBrowser.h"
#include <filesystem>
#include <memory>
#include <string>

#include "Engine.h"
#include "audio/AudioPlayer.h"
#include "core/Logger.h"
#include "core/Utility.h"
#include "debug/SampleGameFactory.h"
#include "global/Config.h"
#include "resource/AssetManager.h"
#include "resource/DataManager.h"
#include "resource/LanguageManager.h"
#include "ui/FileDialog.h"
#ifndef USE_SDL2
#include "ui/HistoryScreen.h"
#include "ui/HomeScreen.h"
#endif
#include "editor/Editor.h"
#include "debug/SampleGameFactory.h"
#include <genie/util/Utility.h>
#include <genie/util/Logger.h>

#ifdef _WIN32
#include <windows.h>
static HANDLE s_stdoutHandle;
static DWORD s_outModeInit;
static void fixWindowsConsole()
{
    DWORD outMode = 0;
    s_stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if(s_stdoutHandle == INVALID_HANDLE_VALUE) {
        puts("failed to get stdout handle");
    }

    if(!GetConsoleMode(s_stdoutHandle, &outMode)) {
        puts("failed to get windows console mode");
    }

    s_outModeInit = outMode;

    // Enable ANSI escape codes
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if(!SetConsoleMode(s_stdoutHandle, outMode)) {
        puts("Failed to set console mode");
    }
}
static void restoreWindowsConsole()
{
    if(!SetConsoleMode(s_stdoutHandle, s_outModeInit)) {
        puts("Failed to restore windows console mode");
    }
}
#endif

static void initData()
{
    if (!Config::Inst().isOptionSet(Config::GamePath)) {
        throw std::runtime_error("No data path set");
    }

    const std::string dataPath = Config::Inst().getValue(Config::GamePath);
    if (!std::filesystem::exists(dataPath)) {
        throw std::runtime_error("Data path does not exist");
    }

    if (!LanguageManager::Inst()->initialize()) {
        throw std::runtime_error("Failed to load language.dll");
    }

    if (!DataManager::Inst().initialize()) {
        throw std::runtime_error("Failed to load game data");
    }
    DBG << "Loaded game data";

    // reinits if already inited
    AssetManager::create(DataManager::Inst().isHd());

    if (!AssetManager::Inst()->initialize(DataManager::Inst().gameVersion())) {
        throw std::runtime_error("Failed to load game assets");
    }
}

#ifndef USE_SDL2
static bool showHomeScreen(genie::ScnFilePtr *scenarioFile)
{
    Config &config = Config::Inst();

    // TODO: clean up this mess...
    while (true) {
        HomeScreen home;
        if (!home.init()) {
            return false;
        }

        const HomeScreen::Button::Type button = home.getSelection();
        if (button == HomeScreen::Button::Exit) {
            return false;
        }

        if (button == HomeScreen::Button::History) {
            HistoryScreen history;
            if (!history.init(AssetManager::Inst()->historyFilesPath())) {
                continue;
            }

            history.display();

            continue;
        }

        if (button == HomeScreen::Button::MapEditor) {
            Editor editor;
            if (editor.init()) {
                editor.run();
            } else {
                WARN << "Failed to load editor";
            }

            continue;
        }

        if (button == HomeScreen::Button::Tutorial) {
            genie::CpxFile cpxFile;
            cpxFile.setFileName(genie::util::resolvePathCaseInsensitive("cam8.cpn", AssetManager::Inst()->campaignsPath()));

            try {
                cpxFile.load();

                *scenarioFile = cpxFile.getScnFile(0);
            } catch (const std::exception &error) {
                WARN << "Failed to load " << cpxFile.getFileName() << ":" << error.what();
                continue;
            }

            break;
        }

        if (button == HomeScreen::Button::Singleplayer) {
            if (home.getGameType() != HomeScreen::GameTypeChoice::Campaign) {
                break;
            }


            std::string campaignFile;
            if (DataManager::Inst().isHd()){
                campaignFile = config.getValue(Config::GamePath) + "/resources/_common/drs/retail-campaigns/dlc0/conquerors/xcam3.cpn";
            } else {
                campaignFile = config.getValue(Config::GamePath) + "/Campaign/xcam3.cpx";
            }

            genie::CpxFile cpxFile;
            cpxFile.setFileName(genie::util::resolvePathCaseInsensitive(campaignFile));

            try {
                cpxFile.load();

                *scenarioFile = cpxFile.getScnFile(0);
                break;
            } catch (const std::exception &error) {
                WARN << "Failed to load " << cpxFile.getFileName() << ":" << error.what();
            }

            break;
        }

        if (!config.isOptionSet(Config::ScenarioFile)) {
            break;
        }

        try {
            *scenarioFile = std::make_shared<genie::ScnFile>();
            (*scenarioFile)->load(config.getValue(Config::ScenarioFile));
        } catch (const std::exception &error) {
            WARN << "Failed to load" << config.getValue(Config::ScenarioFile) << ":" << error.what();
            continue;
        }
        break;
    }

    return true;
}
#endif // !USE_SDL2

static bool requestFilePath(const std::string &errorMessage)
{
    Config &config = Config::Inst();

    FileDialog filediag;
    if (!filediag.setup(1024, 768)) {
        WARN << "failed to open file dialog!";
        return false;
    }
    filediag.setErrorString(errorMessage);

    const std::string selectedPath = filediag.getPath();
    if (selectedPath.empty()) {
        WARN << "user aborted";
        return false;
    }

    config.setValue(Config::GamePath, selectedPath);

    return true;
}

int main(int argc, char **argv)
#ifdef NDEBUG
try
#endif
{
#ifdef _WIN32
    fixWindowsConsole();
#endif
#ifdef ANDROID
    // Redirect stdout/stderr to Android logcat
    // SDL2 on Android does this automatically via SDL_AndroidLogMessage,
    // but only if SDL is initialized. Force it via __android_log_write in a thread.
    {
        int pfd[2];
        pipe(pfd);
        dup2(pfd[1], STDOUT_FILENO);
        dup2(pfd[1], STDERR_FILENO);
        std::thread([fd = pfd[0]]() {
            char buf[512];
            while (true) {
                ssize_t n = read(fd, buf, sizeof(buf) - 1);
                if (n <= 0) break;
                buf[n] = '\0';
                __android_log_write(ANDROID_LOG_INFO, "FreeAoE", buf);
            }
        }).detach();
    }
    __android_log_print(ANDROID_LOG_INFO, "FreeAoE", "=== freeaoe starting ===");
    LogPrinter::enableAllDebug = true;
    genie::Logger::setLogLevel(genie::Logger::L_INFO);
#endif
    for (int i=1; i<argc; i++) {
        if (std::string(argv[i]) == "--debug") {
            LogPrinter::enableAllDebug = true;
            genie::Logger::setLogLevel(genie::Logger::L_INFO);
            break;
        }
    }
    if (!LogPrinter::enableAllDebug) {
        genie::Logger::setLogLevel(genie::Logger::L_WARNING);
    }
    DBG << "executable path" << util::executablePath() << "folder:" << util::executableDirectory();

    Config &config = Config::Inst();

    if (!config.parseOptions(argc, argv)) {
        return 1;
    }

#ifdef ANDROID
    if (!config.isOptionSet(Config::GamePath)) {
        const char *extPath = SDL_AndroidGetExternalStoragePath();
        if (extPath) {
            std::string gamePath = std::string(extPath) + "/aoe2data";
            config.setValue(Config::GamePath, gamePath);
        }
    }
    // Force Russian language on Android
    config.setValue(Config::Language, "ru");
    __android_log_print(4, "FreeAoE", "gamePath=%s lang=%s", config.getValue(Config::GamePath).c_str(), config.getValue(Config::Language).c_str());

    // Debug: check if HD language files are accessible
    {
        std::string gp = config.getValue(Config::GamePath);
        std::string hdFile = gp + "/resources/_common/strings/key-value/non-localized-key-value-strings-utf8.txt";
        std::string ruFile = gp + "/resources/ru/strings/key-value/key-value-strings-utf8.txt";
        bool hdExists = std::filesystem::exists(hdFile);
        bool ruExists = std::filesystem::exists(ruFile);
        __android_log_print(4, "FreeAoE", "LANG gamePath=[%s] hdFile=%d ruFile=%d lang=%s",
            gp.c_str(), hdExists, ruExists, config.getValue(Config::Language).c_str());

        // If HD file not found at current path, try app-specific storage
        if (!hdExists) {
            const char *extPath = SDL_AndroidGetExternalStoragePath();
            if (extPath) {
                std::string altPath = std::string(extPath) + "/aoe2data";
                std::string altHd = altPath + "/resources/_common/strings/key-value/non-localized-key-value-strings-utf8.txt";
                if (std::filesystem::exists(altHd)) {
                    __android_log_print(4, "FreeAoE", "LANG switching gamePath to [%s]", altPath.c_str());
                    config.setValue(Config::GamePath, altPath);
                } else {
                    __android_log_print(4, "FreeAoE", "LANG alt path also missing: [%s]", altHd.c_str());
                }
            }
        }
    }
#endif

    while (true) {
        try {
            initData();
            break;
        } catch(const std::exception &e) {
            WARN << "failed to load:" << e.what() << strerror(errno);

            std::string errorMessage = e.what();
            if (errorMessage.empty() && errno != 0) {
                errorMessage = strerror(errno);
            }

            if (!requestFilePath(errorMessage)) {
                return 1;
            }
        }
    }

    AudioPlayer::instance();

    genie::ScnFilePtr scenarioFile;

#ifndef USE_SDL2
    if (!config.isOptionSet(Config::GameSample) && !config.isOptionSet(Config::SinglePlayer)) {
        if (!showHomeScreen(&scenarioFile)) {
            return 0;
        }

    }
#endif

#ifdef USE_SDL2
    // Load scenario file if specified
    if (config.isOptionSet(Config::ScenarioFile)) {
        try {
            scenarioFile = std::make_shared<genie::ScnFile>();
            scenarioFile->load(config.getValue(Config::ScenarioFile));
        } catch (const std::exception &error) {
            WARN << "Failed to load scenario" << config.getValue(Config::ScenarioFile) << ":" << error.what();
        }
    }

    // Show scenario browser if no scenario specified
    bool isRandomMap = false;
    int rmType = 0, rmSize = 144, rmPlayers = 2, rmStartingAge = 0;
    int rmCivIds[8] = {}, rmTeams[8] = {};
    std::string campaignPath;
    int campaignScenarioIndex = -1, campaignScenarioCount = 0;
    if (!scenarioFile && !config.isOptionSet(Config::SinglePlayer) && !config.isOptionSet(Config::GameSample)) {
        std::string camPath = AssetManager::Inst()->campaignsPath();
        auto browserResult = ScenarioBrowser::show(camPath);
        scenarioFile = browserResult.scenario;
        campaignPath = browserResult.campaignPath;
        campaignScenarioIndex = browserResult.scenarioIndex;
        campaignScenarioCount = browserResult.scenarioCount;
        if (browserResult.isRandomMap) {
            isRandomMap = true;
            rmType = browserResult.randomMapType;
            rmSize = browserResult.randomMapSize;
            rmPlayers = browserResult.randomPlayerCount;
            rmStartingAge = browserResult.randomStartingAge;
            for (int i = 0; i < 8; i++) {
                rmCivIds[i] = browserResult.randomCivIds[i];
                rmTeams[i] = browserResult.randomTeams[i];
            }
        }
    }

    // Fall back to single-player test map if nothing selected
    if (!scenarioFile && !isRandomMap && !config.isOptionSet(Config::SinglePlayer) && !config.isOptionSet(Config::GameSample)) {
        config.setValue(Config::SinglePlayer, "1");
    }
#endif

    Engine engine;
    if (isRandomMap) {
        engine.setSkipDemoGame(true);
    }
    if (!engine.setup(scenarioFile)) {
        return 1;
    }
    if (isRandomMap) {
        engine.setupRandomMap(rmType, rmSize, rmPlayers, rmStartingAge, rmCivIds, rmTeams);
    }
    // Store campaign info for progression
    if (!isRandomMap && !campaignPath.empty()) {
        engine.setCampaignInfo(campaignPath, campaignScenarioIndex, campaignScenarioCount);
    }

    // Multiplayer: --host=PORT or --join=host:port
    //
    // Two-instance local test:
    //   Terminal 1: ./freeaoe --host=12345
    //   Terminal 2: ./freeaoe --join=localhost:12345
    //
    if (config.isOptionSet(Config::HostGame)) {
        uint16_t port = 12345;
        std::string portStr = config.getValue(Config::HostGame);
        if (!portStr.empty()) {
            port = static_cast<uint16_t>(std::stoi(portStr));
        }
        engine.setupMultiplayerHost(port);
    } else if (config.isOptionSet(Config::JoinGame)) {
        std::string joinStr = config.getValue(Config::JoinGame);
        std::string host = "localhost";
        uint16_t port = 12345;
        size_t colonPos = joinStr.rfind(':');
        if (colonPos != std::string::npos) {
            host = joinStr.substr(0, colonPos);
            port = static_cast<uint16_t>(std::stoi(joinStr.substr(colonPos + 1)));
        } else {
            host = joinStr;
        }
        engine.setupMultiplayerClient(host, port);
    }

    engine.start();
#ifdef _WIN32
    restoreWindowsConsole();
#endif

    return 0;
}
#ifdef NDEBUG
 catch (const std::exception &e) {
    std::cerr << "uncatched exception '" << e.what() << "', terminating uncleanly" << std::endl;
    return 1;
}
#endif
