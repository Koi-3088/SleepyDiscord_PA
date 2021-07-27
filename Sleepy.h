#ifndef PokemonAutomation_SleepyPA_H
#define PokemonAutomation_SleepyPA_H

#include <stdint.h>

#ifdef SLEEPY_STATIC
#define SLEEPY_API
#else

#ifdef _WINDLL
#define SLEEPY_API __declspec(dllexport)
#else
#define SLEEPY_API __declspec(dllimport)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
	typedef void (*SleepyCallback)(int response, uint64_t id, uint16_t button, uint16_t hold_ticks, uint8_t x, uint8_t y);
	SLEEPY_API void client_connect(SleepyCallback callback, char* channels, char* sudo, char* params);
	SLEEPY_API void client_disconnect();
	SLEEPY_API void sendMsg(char* message);
	SLEEPY_API void sendEmbed(char* json, char* message);
	SLEEPY_API void sendFile(char* filePath, char* message, char* json = nullptr);
			   void sleepy_commands();

	enum SleepyResponse {
		Disconnected = 0,
		Connected = 1,
		NotRunning = 2,
		Running = 3,
		CallbackSet = 4,
		RequestReceived = 5,
		InvalidCommand = 6,

		Click = 7,
		DPad = 8,

		SetLStick = 9,
		SetRStick = 10,

		ScreenshotJpg = 11,
		ScreenshotPng = 12,
		Start = 13,
		Stop = 14,
		Shutdown = 15,
		ResetSerial = 16,
		ChangeProgram = 17,

		Hi = 18,
		Ping = 19,
		About = 20,
		Help = 21,
	};

#ifdef __cplusplus
}
#endif

#endif