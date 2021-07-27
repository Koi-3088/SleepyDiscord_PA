#include "Sleepy.h"
#include "sleepy_discord/sleepy_discord.h"

class SleepyClient;
namespace CommandList {
	struct Command {
		std::string name;
		std::vector<std::string> params;
		bool require_sudo;
		bool require_owner;
		std::function<void(SleepyClient&, SleepyDiscord::Message&, std::queue<std::string>&, SleepyResponse)> func;
	};

	using CommandMap = std::map<std::string, Command>;
	using Commands = CommandMap::value_type;
	static CommandMap command_map;

	static void insert_command(Command command) {
		command_map.emplace(command.name, command);
	}

	std::queue<std::string> split_input(std::string& input, char delim) {
		std::string component;
		std::queue<std::string> result;
		std::stringstream stream(input);

		while (std::getline(stream, component, delim)) {
			if (!component.empty())
				result.push(component);
		}
		return result;
	}
}

enum Buttons {
	Y = (uint16_t)1 << 0,
	B = (uint16_t)1 << 1,
	A = (uint16_t)1 << 2,
	X = (uint16_t)1 << 3,
	L = (uint16_t)1 << 4,
	R = (uint16_t)1 << 5,
	ZL = (uint16_t)1 << 6,
	ZR = (uint16_t)1 << 7,
	Minus = (uint16_t)1 << 8,
	Plus = (uint16_t)1 << 9,
	LStick = (uint16_t)1 << 10,
	RStick = (uint16_t)1 << 11,
	Home = (uint16_t)1 << 12,
	Capture = (uint16_t)1 << 13,
};

enum DPad {
	DUp = (uint8_t)0,
	DRight = (uint8_t)2,
	DDown = (uint8_t)4,
	DLeft = (uint8_t)6,
};

std::unordered_map<std::string, SleepyResponse> response_map = {
	{ "click", SleepyResponse::Click },
	{ "leftstick", SleepyResponse::SetLStick },
	{ "rightstick", SleepyResponse::SetRStick },
	{ "screenshot", SleepyResponse::ScreenshotJpg },
	{ "start", SleepyResponse::Start },
	{ "stop", SleepyResponse::Stop },
	{ "shutdown", SleepyResponse::Shutdown },
	{ "reset", SleepyResponse::ResetSerial },
	{ "change", SleepyResponse::ChangeProgram },
	{ "hi", SleepyResponse::Hi },
	{ "ping", SleepyResponse::Ping },
	{ "about", SleepyResponse::About },
	{ "help", SleepyResponse::Help },
};

std::unordered_map<std::string, uint16_t> button_map = {
	{ "a", Buttons::A },
	{ "b", Buttons::B },
	{ "x", Buttons::X },
	{ "y", Buttons::Y },
	{ "l", Buttons::L },
	{ "r", Buttons::R },
	{ "zl", Buttons::ZL },
	{ "zr", Buttons::ZR },
	{ "minus", Buttons::Minus },
	{ "plus", Buttons::Plus },
	{ "lstick", Buttons::LStick },
	{ "rstick", Buttons::RStick },
	{ "home", Buttons::Home },
	{ "capture", Buttons::Capture },

	{ "dup", DPad::DUp },
	{ "ddown", DPad::DDown },
	{ "dleft", DPad::DLeft },
	{ "dright", DPad::DRight },
};

class SleepyClient : public SleepyDiscord::DiscordClient {
public:
	SleepyCallback m_callback = nullptr;
	std::unordered_map<std::string, std::string> m_settings;
	std::vector<std::string> m_channels;
	std::vector<std::string> m_sudo;
	SleepyDiscord::Snowflake<SleepyDiscord::Channel> m_file_channel;
	std::vector<SleepyDiscord::Server> m_cache;
	bool m_connected;

	void send_callback(int response, uint64_t console_id, uint16_t button, uint16_t hold_ticks, uint8_t x, uint8_t y)
	{
		if (m_callback != nullptr)
			m_callback(response, console_id, button, hold_ticks, x, y);
	}

	void send_message(char* message) {
		auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
		for (auto it : m_channels)
			sendMessage(it, message, mode);
	}

	void send_embed(char* json, char* message) {
		auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
		for (auto it : m_channels)
			sendMessage(it, message, SleepyDiscord::Embed(json), SleepyDiscord::TTS::DisableTTS, mode);
	}

	void send_file(char* filePath, char* message, char* json = nullptr) {
		auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
		SleepyDiscord::Embed embed;
		if (json != nullptr)
			embed = SleepyDiscord::Embed(json);

		uploadFile(m_file_channel, filePath, message, embed, mode);
	}

	void disconnect() {
		m_connected = false;
		m_callback = nullptr;
		quit();
	}

private:
	std::chrono::steady_clock::time_point m_last_ack = std::chrono::steady_clock::now();
	using SleepyDiscord::DiscordClient::DiscordClient;

	void onReady(SleepyDiscord::Ready data) override {
		auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
		sleepy_commands();
		for (auto it : m_channels)
			sendMessage(it, "I'm alive!", mode);
		m_connected = true;
		send_callback(SleepyResponse::Connected, 0, 0, 0, 0, 0);
	}

	void onServer(SleepyDiscord::Server server) override {
		m_cache.push_back(server);
	}

	void onHeartbeat() override {
		auto time = std::chrono::steady_clock::now();
		auto delta = std::chrono::duration_cast<std::chrono::minutes>(time - m_last_ack).count();
		if (delta > 10)
			send_callback(SleepyResponse::Disconnected, 0, 0, 0, 0, 0);
	}

	void onHeartbeatAck() override {
		auto time = std::chrono::steady_clock::now();
		auto delta = std::chrono::duration_cast<std::chrono::minutes>(time - m_last_ack).count();
		if (delta > 2) {
			SleepyDiscord::BaseDiscordClient::updateStatus(m_settings["status"], m_last_ack.time_since_epoch().count(), SleepyDiscord::idle);
			m_last_ack = time;
		}
	}

	void onMessage(SleepyDiscord::Message message) override {
		if (message.author.bot)
			return;

		auto it = std::find(m_channels.begin(), m_channels.end(), message.channelID.string());
		if (it == m_channels.end())
			return;

		std::string prefix = m_settings["prefix"];
		if (message.startsWith(prefix) && message.content.length() > 1) {
			SleepyDiscord::BaseDiscordClient::updateStatus(m_settings["status"], m_last_ack.time_since_epoch().count(), SleepyDiscord::online);
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));

			auto it0 = message.content.find(prefix);
			if (it0 != std::string::npos)
				message.content.erase(it0, prefix.length());

			std::locale loc;
			std::string content;
			for (auto c : message.content)
				content += std::tolower(c, loc);

			std::queue<std::string> params = CommandList::split_input(content, ' ');

			if (params.size() < 1) {
				sendMessage(message.channelID, "No command entered.", mode);
				return;
			}

			auto it = CommandList::command_map.find(params.front());
			if (it == CommandList::command_map.end()) {
				sendMessage(message.channelID, "Command not found.", mode);
				return;
			}

			params.pop();
			bool sudo = std::find(m_sudo.begin(), m_sudo.end(), message.author.ID.string()) != m_sudo.end();
			bool owner = m_settings["owner"] == message.author.ID.string();

			if ((it->second.require_owner && !owner) || (it->second.require_sudo && !sudo && !owner)) {
				sendMessage(message.channelID, "You don't have permission to use this command.", mode);
				return;
			}
			else if (params.size() < it->second.params.size()) {
				sendMessage(message.channelID, "Too few parameters entered.", mode);
				return;
			}
			else if (params.size() > it->second.params.size()) {
				sendMessage(message.channelID, "Too many parameters entered.", mode);
				return;
			}

			auto response = response_map.find(it->second.name)->second;
			it->second.func(*this, message, params, response);
		}
	}

	void onQuit() override {
		//send_callback(SleepyResponse::NotRunning);
	}

	void onDisconnect() override {
		//send_callback(SleepyResponse::Disconnected);
	}
};
std::unique_ptr<SleepyClient> client;

void client_connect(SleepyCallback callback, char* channels, char* sudo, char* params) {
	std::string s_channels = channels;
	std::string s_sudo = sudo;
	std::string s_params = params;
	std::queue<std::string> q_channels = CommandList::split_input(s_channels, ',');
	std::queue<std::string> q_sudo = CommandList::split_input(s_sudo, ',');
	std::queue<std::string> q_settings = CommandList::split_input(s_params, '|');

	client.reset(new SleepyClient(q_settings.front(), 1));
	client->m_callback = callback;
	client->send_callback(SleepyResponse::CallbackSet, 0, 0, 0, 0, 0);

	while (!q_channels.empty()) {
		client->m_channels.push_back(q_channels.front());
		q_channels.pop();
	}

	while (!q_sudo.empty()) {
		client->m_sudo.push_back(q_sudo.front());
		q_sudo.pop();
	}

	q_settings.pop();
	client->m_settings.emplace("prefix", q_settings.front());
	q_settings.pop();

	client->m_settings.emplace("owner", q_settings.front());
	q_settings.pop();

	client->m_settings.emplace("status", q_settings.front());
	q_settings.pop();

	client->m_settings.emplace("version", q_settings.front());
	q_settings.pop();

	client->m_settings.emplace("github", q_settings.front());
	client->setIntents(SleepyDiscord::Intent::SERVER_MESSAGES, SleepyDiscord::Intent::SERVERS, SleepyDiscord::Intent::SERVER_MEMBERS, SleepyDiscord::Intent::SERVER_PRESENCES);
	try {
		client->run();
	}
	catch (...) {
		client->m_callback = nullptr;
		client.reset();
	}
}

void client_disconnect() {
	if (client == nullptr) {
		client.reset();
		return;
	}

	client->disconnect();
	client.reset();
}

void sendMsg(char* message) {
	client->send_message(message);
}

void sendEmbed(char* json, char* message) {
	client->send_embed(json, message);
}

void sendFile(char* filePath, char* message, char* json) {
	client->send_file(filePath, message, json);
}

void sleepy_commands() {
	CommandList::insert_command({
		"click", { "console id", "button", "hold ticks" }, true, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			int id = std::stoi(params.front());
			params.pop();

			if (response == SleepyResponse::Click && (params.front() == "dup" || params.front() == "ddown" || params.front() == "dleft" || params.front() == "dright"))
				response = SleepyResponse::DPad;

			uint16_t button;
			auto it = button_map.find(params.front());
			if (it == button_map.end()) {
				sleepy.sendMessage(message.channelID, "Invalid button.", mode);
				return;
			}
			button = it->second;
			params.pop();

			int hold = std::stoi(params.front());
			std::string msg = "Sent button click to console ID " + std::to_string(id);
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, static_cast<uint64_t>(id), response == SleepyResponse::DPad ? static_cast<uint8_t>(button) : button, static_cast<uint16_t>(hold), 0, 0);
		}
	});
	CommandList::insert_command({
		"leftstick", { "console id", "x pos (0-255)", "y pos (0-255)", "hold ticks" }, true, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			int id = std::stoi(params.front());
			params.pop();

			int x = std::stoi(params.front());
			params.pop();

			int y = std::stoi(params.front());
			params.pop();

			int hold = std::stoi(params.front());

			std::string msg = "Moving left joystick for console ID " + std::to_string(id);
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, static_cast<uint64_t>(id), 0, static_cast<uint16_t>(hold), static_cast<uint8_t>(x), static_cast<uint8_t>(y));
		}
	});
	CommandList::insert_command({
		"rightstick", { "console id", "x pos (0-255)", "y pos (0-255)", "hold ticks" }, true, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			int id = std::stoi(params.front());
			params.pop();

			int x = std::stoi(params.front());
			params.pop();

			int y = std::stoi(params.front());
			params.pop();

			int hold = std::stoi(params.front());

			std::string msg = "Moving right joystick for console ID " + std::to_string(id);
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, static_cast<uint64_t>(id), 0, static_cast<uint16_t>(hold), static_cast<uint8_t>(x), static_cast<uint8_t>(y));
		}
	});
	CommandList::insert_command({
		"screenshot", { "console id", "format (jpg or png)" }, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			int id = std::stoi(params.front());

			params.pop();
			if (params.front() == "png")
				response = SleepyResponse::ScreenshotPng;

			std::string msg = response == SleepyResponse::ScreenshotPng ? "Requested a png " : "Requested a jpg ";
			msg += "screenshot from console ID " + std::to_string(id);
			sleepy.m_file_channel = message.channelID;
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, static_cast<uint64_t>(id), 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"start", { "program id" }, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			int id = std::stoi(params.front());
			std::string msg = "Starting the specified program...";
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, static_cast<uint64_t>(id), 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"stop", { "program id" }, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			int id = std::stoi(params.front());
			std::string msg = "Stopping the specified program...";
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, static_cast<uint64_t>(id), 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"change", { "program id" }, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			int id = std::stoi(params.front());
			std::string msg = "Changing to the specified program...";
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, static_cast<uint64_t>(id), 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"reset", { "console id" }, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			int id = std::stoi(params.front());
			std::string msg = "Resetting serial connection for console ID " + std::to_string(id);
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, static_cast<uint64_t>(id), 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"shutdown", {}, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			std::string msg = "Shutting down the program...";
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, 0, 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"hi", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			std::string msg = "You're breathtaking, <@!" + message.author.ID + ">";
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, 0, 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"ping", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			std::string msg = "Pong!";
			sleepy.sendMessage(message.channelID, msg, mode);
			sleepy.send_callback(response, 0, 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"help", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			SleepyDiscord::Embed embed;
			SleepyDiscord::EmbedField field_owner;
			SleepyDiscord::EmbedField field_sudo;
			SleepyDiscord::EmbedField field_misc;
			embed.color = 14737632;
			field_owner.isInline = field_sudo.isInline = field_misc.isInline = false;
			embed.title = "These are the commands you can use!";
			field_owner.name = "Discord Commands (Owner)";
			field_sudo.name = "Discord Commands (Sudo)";
			field_misc.name = "Miscellaneous Commands";
			std::string msg = "Help has arrived!";

			bool sudo = std::find(sleepy.m_sudo.begin(), sleepy.m_sudo.end(), message.author.ID.string()) != sleepy.m_sudo.end();
			bool owner = sleepy.m_settings["owner"] == message.author.ID.string();
			for (CommandList::Commands& command : CommandList::command_map) {
				if (command.second.require_sudo && !sudo && !owner)
					continue;

				if (command.second.require_sudo) {
					field_sudo.value += "\n**" + command.first + "**";
					for (std::string param : command.second.params) {
						field_sudo.value += "  `(" + param + ")`  ";
					}
				}
				else if (command.second.require_owner) {
					field_owner.value += "\n**" + command.first + "**";
					for (std::string param : command.second.params) {
						field_owner.value += "  `(" + param + ")`  ";
					}
				}
				else {
					field_misc.value += "\n**" + command.first + "**";
					for (std::string param : command.second.params) {
						field_misc.value += "  `(" + param + ")`  ";
					}
				}
			}

			if (!field_owner.value.empty())
				embed.fields.push_back(field_owner);

			if (!field_sudo.value.empty())
				embed.fields.push_back(field_sudo);

			embed.fields.push_back(field_misc);
			sleepy.sendMessage(message.channelID, msg, embed, SleepyDiscord::TTS::DisableTTS, mode);
			sleepy.send_callback(response, 0, 0, 0, 0, 0);
		}
	});
	CommandList::insert_command({
		"about", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyResponse response) {
			auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Async & ~(SleepyDiscord::ThrowError));
			SleepyDiscord::Embed embed;
			embed.color = 14737632;
			embed.title = "Here's a little bit about me!";

			SleepyDiscord::User owner;
			int channels = 0;
			int users = 0;

			for (auto it : sleepy.m_cache) {
				channels += it.channels.size();
				users += it.members.size();
				if (owner.empty()) {
					for (auto it_owner : it.members) {
						if (it_owner.ID.string() == sleepy.m_settings["owner"]) {
							owner = it_owner;
							break;
						}
					}
				}
			}

			SleepyDiscord::EmbedField field;
			field.isInline = false;
			field.name = "Info";
			field.value += "\n**Owner:** " + owner.username + " (" + std::to_string(owner.ID.number()) + ")";
			field.value += "\n**Guilds:** " + std::to_string(sleepy.m_cache.size());
			field.value += "\n**Channels:** " + std::to_string(channels);
			field.value += "\n**Users:** " + std::to_string(users);
			field.value += "\n**Running Pokemon Automation Serial Programs " + sleepy.m_settings["version"] + "**";
			field.value += "\n[Source code](" + sleepy.m_settings["github"] + ")";

			embed.fields.push_back(field);
			sleepy.sendMessage(message.channelID, "", embed, SleepyDiscord::TTS::DisableTTS, mode);
			sleepy.send_callback(response, 0, 0, 0, 0, 0);
		}
	});
}