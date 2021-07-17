#include "Sleepy.h"
#include "sleepy_discord/sleepy_discord.h"

class SleepyClient : public SleepyDiscord::DiscordClient {
public:
	SleepyCallback m_callback;
	char* m_channel;
	char* m_prefix;
	char* m_token;
	char* m_sudo;
	char* m_game_status;

	void send_callback(int callback)
	{
		if (m_callback != nullptr)
			m_callback(callback);
	}

private:
	bool m_connected;
	std::map<std::string, enum SleepyResponse> m_command_map = {
	{"click a", SleepyResponse::ClickA},
	{"click b", SleepyResponse::ClickB},
	{"click x", SleepyResponse::ClickX},
	{"click y", SleepyResponse::ClickY},
	{"click l", SleepyResponse::ClickL},
	{"click r", SleepyResponse::ClickR},
	{"click zl", SleepyResponse::ClickZL},
	{"click zr", SleepyResponse::ClickZR},
	{"click minus", SleepyResponse::ClickMinus},
	{"click plus", SleepyResponse::ClickPlus},
	{"click lstick", SleepyResponse::ClickLStick},
	{"click rstick", SleepyResponse::ClickRStick},
	{"click home", SleepyResponse::ClickHome},
	{"click capture", SleepyResponse::ClickCapture},
	{"click dup", SleepyResponse::ClickDUp},
	{"click ddown", SleepyResponse::ClickDDown},
	{"click dleft", SleepyResponse::ClickDLeft},
	{"click dright", SleepyResponse::ClickDRight},
	{"setstick left up", SleepyResponse::SetLStickUp},
	{"setstick left down", SleepyResponse::SetLStickDown},
	{"setstick left left", SleepyResponse::SetLStickLeft},
	{"setstick left right", SleepyResponse::SetLStickRight},
	{"setstick right up", SleepyResponse::SetRStickUp},
	{"setstick right down", SleepyResponse::SetRStickDown},
	{"setstick right left", SleepyResponse::SetRStickLeft},
	{"setstick right right", SleepyResponse::SetRStickRight},
	{"start", SleepyResponse::Start},
	{"stop", SleepyResponse::Stop},
	{"shutdown", SleepyResponse::Shutdown},
	{"hi", SleepyResponse::Hi},
	{"ping", SleepyResponse::Ping},
	{"about", SleepyResponse::About},
	{"help", SleepyResponse::Help},
	};
	using SleepyDiscord::DiscordClient::DiscordClient;

	void onReady(SleepyDiscord::Ready data) override {
		auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
		sendMessage(m_channel, "I'm alive!", mode);
		m_connected = true;
		send_callback(SleepyResponse::Connected);
	}

	void onHeartbeatAck() override {
		send_callback(SleepyResponse::Running);
	}

	/*void onServer(SleepyDiscord::Server server) override {
		server_list.emplace(server, server.channels);
	}*/

	void onMessage(SleepyDiscord::Message message) override {
		if (message.startsWith(m_prefix)) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			auto cmd = command_parser(message.content);
			switch (cmd.first) {
			case SleepyResponse::InvalidCommand: sendMessage(message.channelID, "Invalid command input: " + cmd.second, mode); break;
			case SleepyResponse::Hi: sendMessage(message.channelID, "Hi, " + message.author.username, mode); break;
			case SleepyResponse::Ping: sendMessage(message.channelID, "Pong!", mode);
			case SleepyResponse::Help: {
				sendMessage(message.channelID, "**Placeholder**", mode);
			}; break;
			case SleepyResponse::About: {
				sendMessage(message.channelID, "**Placeholder**", mode);
			}; break;
			default: {
				if (message.author.ID != m_sudo) {
					sendMessage(message.channelID, "You don't have permission to execute this command.", mode);
					return;
				}
				sendMessage(message.channelID, "Executing command: " + cmd.second, mode);
			}; break;
			};

			send_callback(cmd.first);
		}
	}

	void onQuit() override {
		send_callback(SleepyResponse::NotRunning);
		client_disconnect();
	}

	void onDisconnect() override {
		send_callback(SleepyResponse::Disconnected);
		client_disconnect();
	}

	std::pair<SleepyResponse, std::string> command_parser(std::string content) {
		std::string prefix = m_prefix;
		auto it0 = content.find(m_prefix);
		if (it0 != std::string::npos)
			content.erase(it0, prefix.length());

		auto it = m_command_map.find(content);
		if (it == m_command_map.end())
			return std::make_pair(SleepyResponse::InvalidCommand, content);

		return std::make_pair(m_command_map[content], content);
	}
};
std::unique_ptr<SleepyClient> client;

void client_connect(SleepyCallback callback, char* token, char* prefix, char* channel, char* sudo, char* status) {
	client.reset(new SleepyClient(token, 1));
	client->m_callback = callback;
	client->m_token = token;
	client->m_prefix = prefix;
	client->m_channel = channel;
	client->m_sudo = sudo;
	client->m_game_status = status;
	client->setIntents(SleepyDiscord::Intent::SERVER_MESSAGES);
	client->send_callback(SleepyResponse::CallbackSet);
	client->run();
	client->send_callback(SleepyResponse::Running);
}

void client_disconnect() {
	client->quit();
	client.reset();
}

void sendMsg(char* text) {
	auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
	client->sendMessage(client->m_channel, text, mode);
}

void sendEmbed(char* embedStr, char* content) {
	auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
	client->sendMessage(client->m_channel, content, SleepyDiscord::Embed(embedStr), SleepyDiscord::TTS::DisableTTS, mode);
}