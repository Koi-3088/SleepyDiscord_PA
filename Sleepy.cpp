#define SLEEPY_DEFAULT_REQUEST_MODE (SleepyDiscord::RequestMode)(SleepyDiscord::RequestMode::Async & ~(SleepyDiscord::RequestMode::ThrowError))
#include <set>
#include "Sleepy.h"
#include "sleepy_discord/sleepy_discord.h"

enum Buttons : uint16_t {
	Y = 1 << 0,
	B = 1 << 1,
	A = 1 << 2,
	X = 1 << 3,
	L = 1 << 4,
	R = 1 << 5,
	ZL = 1 << 6,
	ZR = 1 << 7,
	Minus = 1 << 8,
	Plus = 1 << 9,
	LStick = 1 << 10,
	RStick = 1 << 11,
	Home = 1 << 12,
	Capture = 1 << 13,
};

enum DPad : uint8_t {
	DUp = 0,
	DRight = 2,
	DDown = 4,
	DLeft = 6,
};

std::unordered_map<std::string, SleepyRequest> request_map = {
	{ "click",			SleepyRequest::Click },
	{ "leftstick",		SleepyRequest::SetLStick },
	{ "rightstick",		SleepyRequest::SetRStick },
	{ "screenshot",		SleepyRequest::ScreenshotJpg },
	{ "start",			SleepyRequest::Start },
	{ "stop",			SleepyRequest::Stop },
	{ "shutdown",		SleepyRequest::Shutdown },
	{ "hi",				SleepyRequest::Hi },
	{ "ping",			SleepyRequest::Ping },
	{ "about",			SleepyRequest::About },
	{ "help",			SleepyRequest::Help },
	{ "botinfo",		SleepyRequest::BotInfo },
	{ "reloadsettings", SleepyRequest::ReloadSettings },
	{ "getprograms",	SleepyRequest::GetProgramIDs },
};

std::unordered_map<std::string, uint16_t> button_map = {
	{ "a",		 Buttons::A },
	{ "b",		 Buttons::B },
	{ "x",		 Buttons::X },
	{ "y",		 Buttons::Y },
	{ "l",		 Buttons::L },
	{ "r",		 Buttons::R },
	{ "zl",		 Buttons::ZL },
	{ "zr",		 Buttons::ZR },
	{ "minus",	 Buttons::Minus },
	{ "plus",	 Buttons::Plus },
	{ "lstick",  Buttons::LStick },
	{ "rstick",  Buttons::RStick },
	{ "home",	 Buttons::Home },
	{ "capture", Buttons::Capture },

	{ "dup",	 DPad::DUp },
	{ "ddown",	 DPad::DDown },
	{ "dleft",	 DPad::DLeft },
	{ "dright",  DPad::DRight },
};

// Initializes Discord commands for the bot and prepares user input to be verified for validity.
class SleepyClient;
namespace CommandList {
	struct Command {
		std::string name;
		std::vector<std::string> params;
		bool require_sudo;
		bool require_owner;
		std::function<void(SleepyClient&, SleepyDiscord::Message&, std::queue<std::string>&, SleepyRequest)> func;
	};

	using CommandMap = std::map<std::string, Command>;
	using Commands = CommandMap::value_type;
	static CommandMap command_map;

	static void insert_command(Command command) {
		command_map.emplace(command.name, command);
	}

	std::queue<std::string> split_input(const std::string& input, const char delim, bool command = false) {
		std::string component;
		std::queue<std::string> result;
		std::stringstream stream(input);

		while (std::getline(stream, component, delim)) {
			if (!command || !component.empty()) {
				result.push(component);
			}
		}
		return result;
	}
}

// Struct to store settings from the main program.
static struct SleepySettings {
	bool suffix = false;
	std::unordered_map<std::string, SleepyDiscord::Server> cache;
	std::unordered_map<std::string, std::string> settings;
	std::vector<std::string> whitelist_channels;
	std::vector<std::string> echo_channels;
	std::vector<std::string> log_channels;
	std::vector<std::string> sudo;
	std::string programs;
	std::string token;
	std::string bots;
};

std::unique_ptr<SleepyClient> m_client;
std::unique_ptr<std::thread> m_client_thread;
class SleepyClient : public SleepyDiscord::DiscordClient {
public:
	bool m_connected;
	SleepySettings m_settings;

	// Set callback functions to be called in the main program.
	void set_callbacks(SleepyCallback callback, SleepyCommandCallback cmd_callback) {
		if (m_callback == nullptr) {
			m_callback = callback;
			m_cmd = cmd_callback;
			send_callback(SleepyResponse::CallbacksSet);
		}
	}

	// Send a status/event callback to the main program.
	void send_callback(int response) {
		if (m_callback != nullptr) {
			m_callback(response);
		}
	}

	// Main program's response to various callbacks sent by the wrapper.
	void program_response(int response, char* message = nullptr, char* channel = nullptr) {
		if (channel != nullptr && message != nullptr) {
			sendMessage(channel, message);
		}
		else if (response == (int)SleepyRequest::GetConnectedBots && message != nullptr) {
			m_settings.bots = message;
		}
		else if (response == (int)SleepyRequest::GetProgramIDs && message != nullptr) {
			m_settings.programs = message;
		}
	}

	// Sends a log message to configured log channels.
	void send_log(char* message) {
		for (auto it : m_settings.log_channels) {
			sendMessage(it, message);
		}
	}

	// Sends an embed to configured echo channels.
	void send_embed(char* message, char* json) {
		for (auto it : m_settings.echo_channels) {
			sendMessage(it, message, SleepyDiscord::Embed(json));
		}
	}

	// Sends a file to configured echo channels.
	void send_file(char* filePath, char* message, char* json = nullptr, char* channel = nullptr) {
		SleepyDiscord::Embed embed;
		if (json != nullptr) {
			embed = SleepyDiscord::Embed(json);
		}

		if (channel != nullptr) {
			uploadFile(channel, filePath, message, embed);
			return;
		}

		for (auto it : m_settings.echo_channels) {
			uploadFile(it, filePath, message, embed);
		}
	}

private:
	SleepyCallback m_callback = nullptr;
	SleepyCommandCallback m_cmd = nullptr;
	std::chrono::steady_clock::time_point m_last_ack = std::chrono::steady_clock::now();

	// Send a Discord command-specific callback to the main program.
	void send_command_callback(int request, char* channel = nullptr, uint64_t console_id = 0, uint16_t button = 0, uint16_t hold_ticks = 0, uint8_t x = 0, uint8_t y = 0) {
		if (m_cmd != nullptr) {
			m_cmd(request, channel, console_id, button, hold_ticks, x, y);
		}
	}

	// Helps ensure a parameter supplied by a user is a valid number.
	bool number_parse(const std::string input, SleepyDiscord::Snowflake<SleepyDiscord::Channel> channel, int& parse) {
		try {
			parse = std::stoi(input);
			return true;
		}
		catch (...) {
			sendMessage(channel, "Invalid command input: \"" + input + "\" is not valid for this parameter.");
			return false;
		}
	}

	// Initializes Discord commands on startup.
	void initialize_commands() {
		CommandList::insert_command({
			"click", { "console id", "button", "hold ticks" }, true, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyRequest request) {
				int id, hold;
				if (!sleepy.number_parse(params.front(), message.channelID, id)) {
					return;
				}
				params.pop();

				if (request == SleepyRequest::Click && (params.front() == "dup" || params.front() == "ddown" || params.front() == "dleft" || params.front() == "dright")) {
					request = SleepyRequest::DPad;
				}

				uint16_t button;
				auto it = button_map.find(params.front());
				if (it == button_map.end()) {
					sleepy.sendMessage(message.channelID, "Invalid button.");
					return;
				}
				button = it->second;
				params.pop();

				if (!sleepy.number_parse(params.front(), message.channelID, hold)) {
					return;
				}

				std::string channel = message.channelID.string();
				sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id), request == SleepyRequest::DPad ? static_cast<uint8_t>(button) : button, static_cast<uint16_t>(hold));
			}
		});
		CommandList::insert_command({
			"leftstick", { "console id", "x pos (0-255)", "y pos (0-255)", "hold ticks" }, true, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyRequest request) {
				int id, x, y, hold;
				if (!sleepy.number_parse(params.front(), message.channelID, id)) {
					return;
				}
				params.pop();

				if (!sleepy.number_parse(params.front(), message.channelID, x)) {
					return;
				}
				params.pop();

				if (!sleepy.number_parse(params.front(), message.channelID, y)) {
					return;
				}
				params.pop();

				if (!sleepy.number_parse(params.front(), message.channelID, hold)) {
					return;
				}
				std::string channel = message.channelID.string();
				sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id), 0, static_cast<uint16_t>(hold), static_cast<uint8_t>(x), static_cast<uint8_t>(y));
			}
		});
		CommandList::insert_command({
			"rightstick", { "console id", "x pos (0-255)", "y pos (0-255)", "hold ticks" }, true, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyRequest request) {
				int id, x, y, hold;
				if (!sleepy.number_parse(params.front(), message.channelID, id)) {
					return;
				}
				params.pop();

				if (!sleepy.number_parse(params.front(), message.channelID, x)) {
					return;
				}
				params.pop();

				if (!sleepy.number_parse(params.front(), message.channelID, y)) {
					return;
				}
				params.pop();

				if (!sleepy.number_parse(params.front(), message.channelID, hold)) {
					return;
				}
				std::string channel = message.channelID.string();
				sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id), 0, static_cast<uint16_t>(hold), static_cast<uint8_t>(x), static_cast<uint8_t>(y));
			}
		});
		CommandList::insert_command({
			"screenshot", { "console id", "format (png or jpg)" }, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyRequest request) {
				int id;
				if (!sleepy.number_parse(params.front(), message.channelID, id)) {
					return;
				}
				params.pop();
				if (params.front() == "png") {
					request = SleepyRequest::ScreenshotPng;
				}

				std::string channel = message.channelID.string();
				sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id));
			}
		});
		CommandList::insert_command({
			"start", { "console id" }, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyRequest request) {
				int id;
				if (!sleepy.number_parse(params.front(), message.channelID, id)) {
					return;
				}
				std::string channel = message.channelID.string();
				sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id));
			}
		});
		CommandList::insert_command({
			"stop", { "console id" }, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>& params, SleepyRequest request) {
				int id;
				if (!sleepy.number_parse(params.front(), message.channelID, id)) {
					return;
				}
				std::string channel = message.channelID.string();
				sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id));
			}
		});
		CommandList::insert_command({
			"shutdown", {}, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyRequest request) {
				auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Sync & ~(SleepyDiscord::ThrowError));
				std::string msg = "***Shutting down the program...***";
				sleepy.sendMessage(message.channelID, msg, mode);
				sleepy.send_command_callback(request);
			}
		});
		CommandList::insert_command({
			"hi", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyRequest request) {
				std::string msg = "<@!" + message.author.ID + ">" + sleepy.m_settings.settings["hello"];
				if (sleepy.m_settings.settings["hello"].empty()) {
					msg += ", you're breathtaking!";
				}

				sleepy.sendMessage(message.channelID, msg);
				sleepy.send_command_callback(request);
			}
		});
		CommandList::insert_command({
			"ping", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyRequest request) {
				auto mode = SleepyDiscord::RequestMode(SleepyDiscord::Sync & ~(SleepyDiscord::ThrowError));
				std::string msg = "Pong!";
				auto time1 = std::chrono::steady_clock::now();
				SleepyDiscord::Message reply = sleepy.sendMessage(message.channelID, msg, mode);
				auto time2 = std::chrono::steady_clock::now();
				auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1).count();

				std::string latency = msg + " (" + std::to_string(delta) + "ms)";
				sleepy.editMessage(reply, latency);
				sleepy.send_command_callback(request);
			}
		});
		CommandList::insert_command({
			"help", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyRequest request) {
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

				bool sudo = std::find(sleepy.m_settings.sudo.begin(), sleepy.m_settings.sudo.end(), message.author.ID.string()) != sleepy.m_settings.sudo.end();
				bool owner = sleepy.m_settings.settings["owner"] == message.author.ID.string();
				for (CommandList::Commands& command : CommandList::command_map) {
					if (command.second.require_sudo && !sudo && !owner) {
						continue;
					}
					else if (command.second.require_owner && !owner) {
						continue;
					}

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

				if (!field_owner.value.empty()) {
					embed.fields.push_back(field_owner);
				}

				if (!field_sudo.value.empty()) {
					embed.fields.push_back(field_sudo);
				}

				embed.fields.push_back(field_misc);
				sleepy.sendMessage(message.channelID, msg, embed);
				sleepy.send_command_callback(request);
			}
		});
		CommandList::insert_command({
			"about", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyRequest request) {
				SleepyDiscord::Embed embed;
				embed.color = 14737632;
				embed.title = "Here's a little bit about me!";

				SleepyDiscord::User owner;
				int channels = 0;
				int users = 0;

				for (auto it : sleepy.m_settings.cache) {
					channels += it.second.channels.size();
					users += it.second.members.size();
					if (owner.empty() && !sleepy.m_settings.settings["owner"].empty()) {
						for (auto it_owner : it.second.members) {
							if (it_owner.ID.string() == sleepy.m_settings.settings["owner"]) {
								owner = it_owner;
								break;
							}
						}
					}
				}

				std::string e = u8"é";
				SleepyDiscord::EmbedField field;
				field.isInline = false;
				field.name = "Info";
				if (!owner.empty()) {
					field.value += "\n**Owner:** " + owner.username + "#" + owner.discriminator + " (" + std::to_string(owner.ID.number()) + ")";
				}
				else field.value += "\n**Owner:** *N/A*";

				field.value += "\n**Guilds:** " + std::to_string(sleepy.m_settings.cache.size());
				field.value += "\n**Channels:** " + std::to_string(channels);
				field.value += "\n**Users:** " + std::to_string(users);
				field.value += "\n**Running Pok" + e + "mon Automation Serial Programs " + sleepy.m_settings.settings["version"] + "**";
				field.value += "\n[Source code](" + sleepy.m_settings.settings["github"] + ")";

				embed.fields.push_back(field);
				sleepy.sendMessage(message.channelID, "", embed);
				sleepy.send_command_callback(request);
			}
		});
		CommandList::insert_command({
			"botinfo", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyRequest request) {
				SleepyDiscord::Embed embed;
				SleepyDiscord::EmbedField bots;
				embed.title = "Currently Running Bots";
				embed.color = 14737632;
				bots.isInline = false;
				bots.name = "Bot Information";

				sleepy.send_command_callback(SleepyRequest::GetConnectedBots);
				int timer = 1000;
				while (sleepy.m_settings.bots.empty()) {
					Sleep(50);
					timer -= 50;
					if (timer <= 0) {
						sleepy.sendMessage(message.channelID, "Failed to retrieve bot info.");
						return;
					}
				}

				bots.value = sleepy.m_settings.bots;
				embed.fields.push_back(bots);
				sleepy.m_settings.bots.clear();
				sleepy.sendMessage(message.channelID, "", embed);
				sleepy.send_command_callback(request);
			}
		});
		CommandList::insert_command({
			"reloadsettings", {}, false, true, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyRequest request) {
				std::string channel = message.channelID.string();
				sleepy.send_command_callback(SleepyRequest::ReloadSettings, &channel[0]);
			}
		});
		CommandList::insert_command({
			"getprograms", {}, false, false, [](SleepyClient& sleepy, SleepyDiscord::Message& message, std::queue<std::string>&, SleepyRequest request) {
				SleepyDiscord::Embed embed;
				embed.title = "Currently Running Programs";
				embed.color = 14737632;
				sleepy.send_command_callback(request);

				int timer = 1000;
				while (sleepy.m_settings.programs.empty()) {
					Sleep(50);
					timer -= 50;
					if (timer <= 0) {
						sleepy.sendMessage(message.channelID, "Failed to retrieve program info.");
						return;
					}
				}

				embed.description = sleepy.m_settings.programs;
				sleepy.m_settings.programs.clear();
				sleepy.sendMessage(message.channelID, "", embed);
			}
		});
	}

	// Subscribe to various Discord events. Helps with rate limits and allows us to react accordingly.
	using SleepyDiscord::DiscordClient::DiscordClient;
	void onReady(SleepyDiscord::Ready data) override {
		if (!m_connected) {
			initialize_commands();
			m_connected = true;
			send_callback(SleepyResponse::Connected);

			for (auto it : m_settings.echo_channels) {
				sendMessage(it, "I'm alive!");
			}
		}
	}

	void onServer(SleepyDiscord::Server server) override {
		if (server.empty()) {
			return;
		}

		std::string id = server.ID.string();
		bool updated = false;
		for (auto it : m_settings.cache) {
			if (it.first == id) {
				m_settings.cache.at(id) = server;
				updated = true;
				break;
			}
		}
		
		if (!updated) {
			m_settings.cache.try_emplace(id, server);
		}
	}

	void onQuit() override {
		m_connected = false;
		try {
			send_callback(SleepyResponse::Disconnected);
		} catch (...) {}
	}

	void onHeartbeat() override {
		auto time = std::chrono::steady_clock::now();
		auto delta = std::chrono::duration_cast<std::chrono::minutes>(time - m_last_ack).count();
		if (delta > 6 && m_connected) {
			m_connected = false;
			try {
				send_callback(SleepyResponse::Disconnected);
			} catch (...) {}
		}
	}

	void onHeartbeatAck() override {
		auto time = std::chrono::steady_clock::now();
		auto delta = std::chrono::duration_cast<std::chrono::seconds>(time - m_last_ack).count();
		if (delta > 20) {
			updateStatus(m_settings.settings["status"], m_last_ack.time_since_epoch().count(), SleepyDiscord::idle);
			m_last_ack = time;
		}
	}

	void onMessage(SleepyDiscord::Message message) override {
		if (message.author.bot) {
			return;
		}

		auto it = std::find(m_settings.whitelist_channels.begin(), m_settings.whitelist_channels.end(), message.channelID.string());
		if (it == m_settings.whitelist_channels.end()) {
			return;
		}

		std::string prefix = m_settings.settings["prefix"];
		size_t msgLen = message.content.length();
		size_t prefLen = prefix.length();
		bool correctPrefix = msgLen > prefLen && (m_settings.suffix ? message.content.compare(msgLen - prefLen, prefLen, prefix) == 0 : message.startsWith(prefix));
		if (correctPrefix) {
			updateStatus(m_settings.settings["status"], m_last_ack.time_since_epoch().count(), SleepyDiscord::online);
			auto it0 = message.content.find(prefix);
			if (it0 != std::string::npos) {
				message.content.erase(it0, prefix.length());
			}

			std::locale loc;
			std::string content;
			for (auto c : message.content) {
				content += std::tolower(c, loc);
			}

			std::queue<std::string> params = CommandList::split_input(content, ' ', true);
			if (params.size() < 1) {
				sendMessage(message.channelID, "No command entered.");
				return;
			}

			auto it = CommandList::command_map.find(params.front());
			if (it == CommandList::command_map.end()) {
				sendMessage(message.channelID, "Command not found.");
				return;
			}
			params.pop();

			bool sudo = std::find(m_settings.sudo.begin(), m_settings.sudo.end(), message.author.ID.string()) != m_settings.sudo.end();
			bool owner = m_settings.settings["owner"] == message.author.ID.string();
			if ((it->second.require_owner && !owner) || (it->second.require_sudo && !sudo && !owner)) {
				sendMessage(message.channelID, "You don't have permission to use this command.");
				return;
			}
			else if (params.size() < it->second.params.size()) {
				sendMessage(message.channelID, "Too few parameters entered.");
				return;
			}
			else if (params.size() > it->second.params.size()) {
				sendMessage(message.channelID, "Too many parameters entered.");
				return;
			}

			auto response = request_map.find(it->second.name)->second;
			it->second.func(*this, message, params, response);
		}
	}
};

// Finalize initialization before running the client in a new thread in order to not block the main one.
void client_connect(SleepyCallback callback, SleepyCommandCallback cmd_callback) {
	if (m_client != nullptr) {
		m_client->set_callbacks(callback, cmd_callback);
		m_client->send_callback(SleepyResponse::SettingsInitialized);
		m_client->setIntents(SleepyDiscord::Intent::SERVER_MESSAGES, SleepyDiscord::Intent::SERVERS, SleepyDiscord::Intent::SERVER_MEMBERS, SleepyDiscord::Intent::SERVER_PRESENCES);
		try {
			m_client_thread = std::unique_ptr<std::thread>(new std::thread(client_run));
		} catch (...) {}
	}
}

// Main run thread.
void client_run() {
	try {
		m_client->run();
	} catch (...) {}
}

// Disconnect and clean up.
void client_disconnect() {
	try {
		if (m_client != nullptr && !m_client->isQuiting()) {
			m_client->quit();
		}
		m_client.reset();
		if (m_client_thread != nullptr) {
			m_client_thread.reset();
		}
	}
	catch (...) {}
}

void program_response(int response, char* message, char* channel) {
	if (m_client != nullptr) {
		if (response == SleepyRequest::Terminate) {
			try {
				client_disconnect();
				return;
			}
			catch (...) {
				return;
			}
		}
		m_client->program_response(response, message, channel);
	}
}

// Parse and apply user settings from the main program.
void apply_settings(char* w_channels, char* e_channels, char* l_channels, char* sudo, char* params, bool suffix) {
	SleepySettings settings;
	std::string whitelist = w_channels;
	std::string echo = e_channels;
	std::string log = l_channels;
	std::string s_sudo = sudo;
	std::string s_params = params;
	std::queue<std::string> qw_channels = CommandList::split_input(whitelist, ',');
	std::queue<std::string> qe_channels = CommandList::split_input(echo, ',');
	std::queue<std::string> ql_channels = CommandList::split_input(log, ',');
	std::queue<std::string> q_sudo = CommandList::split_input(s_sudo, ',');
	std::queue<std::string> q_settings = CommandList::split_input(s_params, '|');
	settings.token = q_settings.front();
	q_settings.pop();

	if (m_client == nullptr) {
		m_client = std::unique_ptr<SleepyClient>(new SleepyClient(settings.token, 1));
	}

	while (!qw_channels.empty()) {
		settings.whitelist_channels.push_back(qw_channels.front());
		qw_channels.pop();
	}

	while (!qe_channels.empty()) {
		settings.echo_channels.push_back(qe_channels.front());
		qe_channels.pop();
	}

	while (!ql_channels.empty()) {
		settings.log_channels.push_back(ql_channels.front());
		ql_channels.pop();
	}

	while (!q_sudo.empty()) {
		settings.sudo.push_back(q_sudo.front());
		q_sudo.pop();
	}

	settings.settings.emplace("prefix", q_settings.front());
	q_settings.pop();

	settings.settings.emplace("owner", q_settings.front());
	q_settings.pop();

	std::string e = u8"é";
	std::string status = q_settings.front();
	if (status.empty()) {
		status = "Pok" + e + "mon Automation Serial Programs";
	}

	settings.settings.emplace("status", status);
	q_settings.pop();

	settings.settings.emplace("hello", q_settings.front());
	q_settings.pop();

	settings.settings.emplace("version", q_settings.front());
	q_settings.pop();

	settings.settings.emplace("github", q_settings.front());
	settings.suffix = suffix;

	m_client->m_settings = settings;
	m_client->send_callback(SleepyResponse::SettingsUpdated);
}

void sendLog(char* message) {
	if (m_client != nullptr) {
		m_client->send_log(message);
	}
}

void sendEmbed(char* message, char* json) {
	if (m_client != nullptr) {
		m_client->send_embed(message, json);
	}
}

void sendFile(char* filePath, char* message, char* json, char* channel) {
	if (m_client != nullptr) {
		m_client->send_file(filePath, message, json, channel);
	}
}
