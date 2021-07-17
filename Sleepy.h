#ifndef PokemonAutomation_SleepyPA_H
#define PokemonAutomation_SleepyPA_H

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

	typedef void (*SleepyCallback)(int response);
	SLEEPY_API void client_connect(SleepyCallback callback, char* token, char* prefix, char* channel, char* sudo, char* status);
	SLEEPY_API void client_disconnect();
	SLEEPY_API void sendMsg(char* text);
	SLEEPY_API void sendEmbed(char* embedStr, char* content);

	enum SleepyResponse { // Should be brought in-line with PABotbase's definition values
		Disconnected = 0,
		Connected = 1,
		NotRunning = 2,
		Running = 3,
		CallbackSet = 4,
		InvalidCommand = 5,

		ClickA = 6,
		ClickB = 7,
		ClickX = 8,
		ClickY = 9,
		ClickL = 10,
		ClickR = 11,
		ClickZL = 12,
		ClickZR = 13,
		ClickMinus = 14,
		ClickPlus = 15,
		ClickLStick = 16,
		ClickRStick = 17,
		ClickHome = 18,
		ClickCapture = 19,
		ClickDUp = 20,
		ClickDDown = 21,
		ClickDLeft = 22,
		ClickDRight = 23,

		SetLStickUp = 24,
		SetLStickDown = 25,
		SetLStickLeft = 26,
		SetLStickRight = 27,
		SetRStickUp = 28,
		SetRStickDown = 29,
		SetRStickLeft = 30,
		SetRStickRight = 31,

		Start = 32,
		Stop = 33,
		Shutdown = 34,

		Hi = 35,
		Ping = 36,
		About = 37,
		Help = 38,
	};

#ifdef __cplusplus
}
#endif

#endif