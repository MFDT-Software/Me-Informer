#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <windows.h>
#include <optional>
#include <string>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cstdint>
#define SFML_STATIC
#include <SFML/Graphics.hpp>
#define DISCORDPP_IMPLEMENTATION
#include <discordpp.h>
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:WinMainCRTStartup")
// Constants
constexpr uint64_t DISCORD_APPLICATION_ID = your_app_id + ULL;
constexpr const char* DISCORD_LOGO_KEY = "your_logo_key";
constexpr const char* DATA_FILENAME = "your_filename";
constexpr const char* XOR_KEY = "your_key";
// App States
enum class AppState {
    SelectLanguage,
    InputNickname,
    Main
};
// User Settings Structure
struct UserSettings {
    std::string lang = "Ru"; // "Ru" or "En"
    std::string nickname = "";
    std::vector<std::string> history; // Max 10 items
};
// Encryption helper (XOR)
std::string xorCipher(const std::string& input) {
    std::string output = input;
    size_t keyLen = strlen(XOR_KEY);
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ XOR_KEY[i % keyLen];
    }
    return output;
}
// Save encrypted settings
bool saveSettings(const UserSettings& settings) {
    SetFileAttributesA(DATA_FILENAME, FILE_ATTRIBUTE_NORMAL);
    std::ostringstream ss;
    ss << settings.lang << "\n";
    ss << settings.nickname << "\n";
    ss << settings.history.size() << "\n";
    for (const auto& item : settings.history) {
        ss << item << "\n";
    }
    std::string encrypted = xorCipher(ss.str());
    std::ofstream file(DATA_FILENAME, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(encrypted.data(), encrypted.size());
    file.close();
    SetFileAttributesA(DATA_FILENAME, FILE_ATTRIBUTE_HIDDEN);
    return true;
}
// Load encrypted settings from file
bool loadSettings(UserSettings& settings) {
    std::ifstream file(DATA_FILENAME, std::ios::binary);
    if (!file.is_open()) return false;
    std::string encrypted((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    if (encrypted.empty()) return false;
    std::string decrypted = xorCipher(encrypted);
    std::istringstream ss(decrypted);
    if (!(ss >> settings.lang)) return false;
    ss.ignore();
    if (!std::getline(ss, settings.nickname)) return false;
    size_t historyCount = 0;
    if (ss >> historyCount) {
        ss.ignore();
        settings.history.clear();
        for (size_t i = 0; i < historyCount; ++i) {
            std::string line;
            if (std::getline(ss, line)) {
                settings.history.push_back(line);
            }
        }
    }
    return true;
}
// Wide-character Localization strings
struct LocalizedStrings {
    std::wstring selectLangTitle;
    std::wstring nickTitle;
    std::wstring nickHint;
    std::wstring inputHint;
    std::wstring statusUpdated;
    std::wstring historyHint;
    std::wstring langBtn;
    std::wstring exitBtn;
};
LocalizedStrings getStrings(const std::string& lang) {
    LocalizedStrings s;
    if (lang == "Ru") {
        s.selectLangTitle = L"Выберите язык / Select Language";
        s.nickTitle = L"Добро пожаловать!";
        s.nickHint = L"Введите ваш никнейм и нажмите Enter:";
        s.inputHint = L"Введите статус для Discord и нажмите Enter:";
        s.statusUpdated = L"Статус обновлен: ";
        s.historyHint = L"▲/▼ — История | ◄/► — Каретка";
        s.langBtn = L"RU";
        s.exitBtn = L"ВЫХОД";
    }
    else {
        s.selectLangTitle = L"Select Language / Выберите язык";
        s.nickTitle = L"Welcome!";
        s.nickHint = L"Enter your nickname and press Enter:";
        s.inputHint = L"Enter Discord status and press Enter:";
        s.statusUpdated = L"Status updated: ";
        s.historyHint = L"▲/▼ — History | ◄/► — Cursor";
        s.langBtn = L"EN";
        s.exitBtn = L"EXIT";
    }
    return s;
}
// Time-of-day Greeting Generator
std::wstring getGreeting(const std::string& lang, const std::string& nickname) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &t);
    int hour = local_tm.tm_hour;
    sf::String nickSf = sf::String::fromUtf8(nickname.begin(), nickname.end());
    std::wstring nickW = nickSf.toWideString();
    if (lang == "Ru") {
        if (hour >= 5 && hour < 12) return L"Доброе утро, " + nickW;
        if (hour >= 12 && hour < 18) return L"Добрый день, " + nickW;
        if (hour >= 18 && hour < 23) return L"Добрый вечер, " + nickW;
        return L"Доброй ночи, " + nickW;
    }
    else {
        if (hour >= 5 && hour < 12) return L"Good morning, " + nickW;
        if (hour >= 12 && hour < 18) return L"Good afternoon, " + nickW;
        if (hour >= 18 && hour < 23) return L"Good evening, " + nickW;
        return L"Good night, " + nickW;
    }
}
// Discord Rich Presence Manager
class RichPresenceManager {
public:
    bool InitializeDiscord() {
        client_ = std::make_shared<discordpp::Client>();
        client_->SetApplicationId(DISCORD_APPLICATION_ID);

        client_->AddLogCallback([](auto message, auto severity) {}, discordpp::LoggingSeverity::Info);
        client_->SetStatusChangedCallback(
            [this](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
                if (status == discordpp::Client::Status::Ready) discord_ready_ = true;
                else if (error != discordpp::Client::Error::None) discord_ready_ = false;
            });
        client_->Connect();
        discord_initialized_ = true;
        return true;
    }
    void UpdatePresence(const std::string& text) {
        if (discord_initialized_ && client_) {
            discordpp::Activity activity;
            activity.SetType(discordpp::ActivityTypes::Playing);
            activity.SetDetails(std::optional<std::string>(text));
            discordpp::ActivityAssets assets;
            assets.SetLargeImage(DISCORD_LOGO_KEY);
            activity.SetAssets(assets);

            client_->UpdateRichPresence(activity, [](discordpp::ClientResult result) { (void)result; });
        }
    }
    bool discord_initialized_ = false;
    bool discord_ready_ = false;
private:
    std::shared_ptr<discordpp::Client> client_;
};
// Entry point
extern "C" int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    std::locale::global(std::locale(".UTF-8"));
    UserSettings settings;
    bool isFirstLaunch = !loadSettings(settings);
    AppState state = isFirstLaunch ? AppState::SelectLanguage : AppState::Main;
    RichPresenceManager presence;
    presence.discord_initialized_ = presence.InitializeDiscord();
    // SFML Window
    sf::RenderWindow window(sf::VideoMode(640, 420), "Me Informer by {MFDT}", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    // Font
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        return 1;
    }
    // Color Palette
    const sf::Color colorBg(10, 10, 15);               // Black
    const sf::Color colorCyan(0, 210, 255);           // Cyan Text / Accent
    const sf::Color colorDarkBox(20, 25, 35);         // Input box background
    const sf::Color colorSubtext(120, 160, 180);       // Hints / Subtitles
    const sf::Color colorSuccess(80, 255, 180);        // Mint Green
    // Caret and Navigation State
    sf::Clock caretClock;
    bool showCaret = true;
    size_t caretPos = 0;
    int historyIndex = -1;
    sf::String inputString;
    sf::String lastSubmittedStatus;
    // UI Elements Setup
    sf::Text titleText("", font, 24);
    titleText.setFillColor(colorCyan);
    sf::Text hintText("", font, 16);
    hintText.setFillColor(colorSubtext);
    sf::Text inputText("", font, 20);
    inputText.setFillColor(sf::Color::White);
    inputText.setPosition(30.f, 182.f);
    sf::Text statusText("", font, 16);
    statusText.setFillColor(colorSuccess);
    statusText.setPosition(25.f, 360.f);
    sf::Text historyHintText("", font, 14);
    historyHintText.setFillColor(colorSubtext);
    historyHintText.setPosition(25.f, 240.f);
    // Input Box Shape
    sf::RectangleShape inputBox(sf::Vector2f(590.f, 50.f));
    inputBox.setPosition(25.f, 170.f);
    inputBox.setFillColor(colorDarkBox);
    inputBox.setOutlineColor(colorCyan);
    inputBox.setOutlineThickness(1.5f);
    // Caret Line
    sf::RectangleShape caret(sf::Vector2f(2.f, 24.f));
    caret.setFillColor(colorCyan);
    // Exit Button (Bottom Right)
    sf::RectangleShape exitBtnBox(sf::Vector2f(80.f, 30.f));
    exitBtnBox.setPosition(535.f, 370.f);
    exitBtnBox.setFillColor(colorDarkBox);
    exitBtnBox.setOutlineColor(colorCyan);
    exitBtnBox.setOutlineThickness(1.f);
    sf::Text exitBtnText("", font, 13);
    exitBtnText.setFillColor(colorCyan);
    // Language Toggle Button (Main Screen - Top Right)
    sf::RectangleShape langBtnBox(sf::Vector2f(65.f, 30.f));
    langBtnBox.setPosition(550.f, 15.f);
    langBtnBox.setFillColor(colorDarkBox);
    langBtnBox.setOutlineColor(colorCyan);
    langBtnBox.setOutlineThickness(1.f);
    sf::Text langBtnText("", font, 14);
    langBtnText.setFillColor(colorCyan);
    // First Launch Language Choice Buttons
    sf::RectangleShape btnRu(sf::Vector2f(160.f, 50.f));
    btnRu.setPosition(130.f, 200.f);
    btnRu.setFillColor(colorDarkBox);
    btnRu.setOutlineColor(colorCyan);
    btnRu.setOutlineThickness(2.f);
    sf::Text textRu(L"Русский", font, 20);
    textRu.setFillColor(colorCyan);
    textRu.setPosition(165.f, 210.f);
    sf::RectangleShape btnEn(sf::Vector2f(160.f, 50.f));
    btnEn.setPosition(350.f, 200.f);
    btnEn.setFillColor(colorDarkBox);
    btnEn.setOutlineColor(colorCyan);
    btnEn.setOutlineThickness(2.f);
    sf::Text textEn("English", font, 20);
    textEn.setFillColor(colorCyan);
    textEn.setPosition(390.f, 210.f);
    // Main Event & Render Loop
    while (window.isOpen()) {
        LocalizedStrings strings = getStrings(settings.lang);
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            // Mouse Clicks
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                sf::Vector2f mousePos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
                // Global Exit Button Click
                if (exitBtnBox.getGlobalBounds().contains(mousePos)) {
                    window.close();
                }
                if (state == AppState::SelectLanguage) {
                    if (btnRu.getGlobalBounds().contains(mousePos)) {
                        settings.lang = "Ru";
                        state = AppState::InputNickname;
                    }
                    else if (btnEn.getGlobalBounds().contains(mousePos)) {
                        settings.lang = "En";
                        state = AppState::InputNickname;
                    }
                }
                else if (state == AppState::Main) {
                    if (langBtnBox.getGlobalBounds().contains(mousePos)) {
                        settings.lang = (settings.lang == "Ru") ? "En" : "Ru";
                        saveSettings(settings);
                    }
                }
            }
            // Text Input Handling
            if (event.type == sf::Event::TextEntered) {
                if (event.text.unicode == 8) { // Backspace
                    if (caretPos > 0 && !inputString.isEmpty()) {
                        inputString.erase(caretPos - 1, 1);
                        caretPos--;
                    }
                }
                else if (event.text.unicode >= 32 && event.text.unicode != 127) {
                    inputString.insert(caretPos, event.text.unicode);
                    caretPos++;
                }
                showCaret = true;
                caretClock.restart();
            }
            // Key Press Events
            if (event.type == sf::Event::KeyPressed) {
                // Enter Submission
                if (event.key.code == sf::Keyboard::Enter) {
                    if (!inputString.isEmpty()) {
                        auto utf8Bytes = inputString.toUtf8();
                        std::string utf8Text(reinterpret_cast<const char*>(utf8Bytes.data()), utf8Bytes.size());
                        if (state == AppState::InputNickname) {
                            settings.nickname = utf8Text;
                            saveSettings(settings);
                            inputString.clear();
                            caretPos = 0;
                            state = AppState::Main;
                        }
                        else if (state == AppState::Main) {
                            presence.UpdatePresence(utf8Text);
                            // Save to History (max 10)
                            auto it = std::find(settings.history.begin(), settings.history.end(), utf8Text);
                            if (it != settings.history.end()) settings.history.erase(it);
                            settings.history.push_back(utf8Text);
                            if (settings.history.size() > 10) settings.history.erase(settings.history.begin());
                            saveSettings(settings);
                            lastSubmittedStatus = inputString; // Saving the entered status
                            inputString.clear();
                            caretPos = 0;
                            historyIndex = -1;
                        }
                    }
                }
                // Caret Navigation (Left / Right / Home / End)
                else if (event.key.code == sf::Keyboard::Left) {
                    if (caretPos > 0) {
                        caretPos--;
                        showCaret = true;
                        caretClock.restart();
                    }
                }
                else if (event.key.code == sf::Keyboard::Right) {
                    if (caretPos < inputString.getSize()) {
                        caretPos++;
                        showCaret = true;
                        caretClock.restart();
                    }
                }
                else if (event.key.code == sf::Keyboard::Home) {
                    caretPos = 0;
                    showCaret = true;
                    caretClock.restart();
                }
                else if (event.key.code == sf::Keyboard::End) {
                    caretPos = inputString.getSize();
                    showCaret = true;
                    caretClock.restart();
                }
                // History Navigation (Up / Down)
                else if (state == AppState::Main && !settings.history.empty()) {
                    if (event.key.code == sf::Keyboard::Up) {
                        if (historyIndex < static_cast<int>(settings.history.size()) - 1) {
                            historyIndex++;
                            std::string hItem = settings.history[settings.history.size() - 1 - historyIndex];
                            inputString = sf::String::fromUtf8(hItem.begin(), hItem.end());
                            caretPos = inputString.getSize();
                        }
                    }
                    else if (event.key.code == sf::Keyboard::Down) {
                        if (historyIndex > 0) {
                            historyIndex--;
                            std::string hItem = settings.history[settings.history.size() - 1 - historyIndex];
                            inputString = sf::String::fromUtf8(hItem.begin(), hItem.end());
                            caretPos = inputString.getSize();
                        }
                        else if (historyIndex == 0) {
                            historyIndex = -1;
                            inputString.clear();
                            caretPos = 0;
                        }
                    }
                }
            }
        }
        discordpp::RunCallbacks();
        // Caret Blinking Timer
        if (caretClock.getElapsedTime().asSeconds() > 0.5f) {
            showCaret = !showCaret;
            caretClock.restart();
        }
        // Render Frame
        window.clear(colorBg);
        // Draw Exit Button
        exitBtnText.setString(strings.exitBtn);
        exitBtnText.setPosition(552.f, 376.f);
        window.draw(exitBtnBox);
        window.draw(exitBtnText);
        if (state == AppState::SelectLanguage) {
            titleText.setString(strings.selectLangTitle);
            titleText.setPosition(110.f, 100.f);
            window.draw(titleText);
            window.draw(btnRu);
            window.draw(textRu);
            window.draw(btnEn);
            window.draw(textEn);
        }
        else if (state == AppState::InputNickname) {
            titleText.setString(strings.nickTitle);
            titleText.setPosition(25.f, 60.f);
            hintText.setString(strings.nickHint);
            hintText.setPosition(25.f, 130.f);
            window.draw(titleText);
            window.draw(hintText);
            window.draw(inputBox);
            inputText.setString(inputString);
            window.draw(inputText);
            // Caret
            if (showCaret) {
                sf::Vector2f charPos = inputText.findCharacterPos(caretPos);
                caret.setPosition(charPos.x + 1.f, 183.f);
                window.draw(caret);
            }
        }
        else if (state == AppState::Main) {
            titleText.setString(getGreeting(settings.lang, settings.nickname));
            titleText.setPosition(25.f, 50.f);
            hintText.setString(strings.inputHint);
            hintText.setPosition(25.f, 130.f);
            historyHintText.setString(strings.historyHint);
            langBtnText.setString(strings.langBtn);
            langBtnText.setPosition(570.f, 20.f);
            window.draw(titleText);
            window.draw(hintText);
            window.draw(inputBox);
            window.draw(historyHintText);
            window.draw(langBtnBox);
            window.draw(langBtnText);
            inputText.setString(inputString);
            window.draw(inputText);
            // Caret
            if (showCaret) {
                sf::Vector2f charPos = inputText.findCharacterPos(caretPos);
                caret.setPosition(charPos.x + 1.f, 183.f);
                window.draw(caret);
            }
            // Dynamic generation of status translation within the frame
            if (!lastSubmittedStatus.isEmpty()) {
                statusText.setString(sf::String(strings.statusUpdated) + lastSubmittedStatus);
                window.draw(statusText);
            }
        }
        window.display();
    }
    return 0;
}