#ifndef PokemonAutomation_SleepyPA_H
#define PokemonAutomation_SleepyPA_H

#include <stdint.h>

#ifdef SLEEPY_STATIC
#define SLEEPY_API
#else

#ifdef _WIN32

#ifdef _WINDLL
#define SLEEPY_API __declspec(dllexport)
#else
#define SLEEPY_API __declspec(dllimport)
#endif

#else
#define SLEEPY_API __attribute__((visibility("default")))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
    typedef void (*SleepyCallback)(int response, char* message);
    typedef void (*SleepyCommandCallback)(int request, char* channel, uint64_t console_id, uint16_t button, uint16_t hold_ticks, uint8_t x, uint8_t y);
    SLEEPY_API void client_connect();
    SLEEPY_API void client_disconnect();
    SLEEPY_API void apply_settings(SleepyCallback callback, SleepyCommandCallback cmd_callback, char* w_channels, char* e_channels, char* l_channels, char* sudo, char* params, bool suffix);
    SLEEPY_API void program_response(int response, char* channel = nullptr, char* message = nullptr, char* filepath = nullptr);
    SLEEPY_API void sendLog(char* message);
    SLEEPY_API void sendEmbed(char* message, char* json);
    SLEEPY_API void sendFile(char* filePath, char* channel, char* message, char* json = nullptr);
    void client_run();

    enum SleepyResponse {
        Fault = 0,
        API = 1,
        Disconnected = 2,
        Connected = 3,
        SettingsInitialized = 4,
        SettingsUpdated = 5,
        CommandsInitialized = 6,
        CallbacksSet = 7,
        InvalidCommand = 8,
        Terminating = 9,
    };

    enum SleepyRequest {
        Click = 0,
        DPad = 1,

        SetLStick = 2,
        SetRStick = 3,

        ScreenshotJpg = 4,
        ScreenshotPng = 5,
        Start = 6,
        Stop = 7,
        Shutdown = 8,
        GetConnectedBots = 9,
        ReloadSettings = 10,
        Terminate = 11,

        Hi = 12,
        Ping = 13,
        About = 14,
        Help = 15,
    };

#ifdef __cplusplus
}
#endif
#endif