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
	typedef void (*SleepyCallback)(int response);
	typedef void (*SleepyCommandCallback)(int request, char* channel, uint64_t console_id, uint16_t button, uint16_t hold_ticks, uint8_t x, uint8_t y);
	SLEEPY_API void client_connect(SleepyCallback callback, SleepyCommandCallback cmd_callback);
	SLEEPY_API void client_disconnect();
	SLEEPY_API void apply_settings(char* w_channels, char* e_channels, char* l_channels, char* sudo, char* params, bool suffix);
	SLEEPY_API void program_response(int response, char* message = nullptr, char* channel = nullptr);
	SLEEPY_API void sendLog(char* message);
	SLEEPY_API void sendEmbed(char* message, char* json);
	SLEEPY_API void sendFile(char* filePath, char* message, char* json = nullptr, char* channel = nullptr);
	void client_run();

	enum SleepyResponse {
		Fault = 0,
		Disconnected = 1,
		Connected = 2,
		SettingsInitialized = 3,
		SettingsUpdated = 4,
		CallbacksSet = 5,
		InvalidCommand = 6,
		Terminating = 7,
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

		Hi = 9,
		Ping = 10,
		About = 11,
		Help = 12,
		BotInfo = 13,
		GetConnectedBots = 14,
		ReloadSettings = 15,
		GetProgramIDs = 16,
		Terminate = 17,
	};

#ifdef __cplusplus
}
#endif
#endif