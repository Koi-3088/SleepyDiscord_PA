#define SLEEPY_DEFAULT_REQUEST_MODE (SleepyDiscord::RequestMode)(SleepyDiscord::RequestMode::Async & ~(SleepyDiscord::RequestMode::ThrowError))
#include "Sleepy.h"
#include "sleepy_discord/sleepy_discord.h"

namespace SleepyDiscord {

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
        { "click",            Click },
        { "leftstick",        SetLStick },
        { "rightstick",       SetRStick },
        { "screenshot",       ScreenshotJpg },
        { "start",            Start },
        { "stop",             Stop },
        { "shutdown",         Shutdown },
        { "hi",               Hi },
        { "ping",             Ping },
        { "about",            About },
        { "help",             Help },
        { "botinfo",          GetConnectedBots },
        { "reloadsettings",   ReloadSettings },
        { "resetcamera",      ResetCamera },
        { "resetserial",      ResetSerial },
    };

    std::unordered_map<std::string, uint16_t> button_map = {
        { "a",         A },
        { "b",         B },
        { "x",         X },
        { "y",         Y },
        { "l",         L },
        { "r",         R },
        { "zl",        ZL },
        { "zr",        ZR },
        { "minus",     Minus },
        { "plus",      Plus },
        { "lstick",    LStick },
        { "rstick",    RStick },
        { "home",      Home },
        { "capture",   Capture },

        { "dup",       DUp },
        { "ddown",     DDown },
        { "dleft",     DLeft },
        { "dright",    DRight },
    };

    // Initializes Discord commands for the bot and prepares user input to be verified for validity.
    class SleepyClient;
    namespace CommandList {
        struct Command {
            std::string name;
            std::vector<std::string> params;
            bool require_sudo;
            bool require_owner;
            std::function<void(SleepyClient&, Message&, std::queue<std::string>&, SleepyRequest)> func;
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
    struct SleepySettings {
        bool suffix = false;
        std::unordered_map<std::string, Server> cache;
        std::unordered_map<std::string, std::string> settings;
        std::vector<std::string> whitelist_channels;
        std::vector<std::string> sudo;
        std::string token;
    };

    std::unique_ptr<SleepyClient> m_client;
    std::thread* m_client_thread;
    class SleepyClient : public DiscordClient {
    public:
        bool m_connecting = false;
        bool m_stopping = false;
        SleepySettings m_settings;

        // Set callback functions to be called in the main program.
        void set_callbacks(SleepyCallback callback, SleepyCommandCallback cmd_callback) {
            if (m_callback == nullptr || m_cmd == nullptr) {
                m_callback = callback;
                m_cmd = cmd_callback;
                send_callback(SleepyResponse::CallbacksSet, "[SleepyDiscord]: Callbacks between the program and the wrapper have been set.");
            }
        }

        // Send a status/event callback to the main program.
        void send_callback(int response, char* message) {
            if (m_callback != nullptr && !m_stopping) {
                m_callback(response, message);
            }
        }

        // Main program's responses and requests to various callbacks sent by the wrapper.
        void program_response(int response, char* channel = nullptr, char* message = nullptr, char* filepath = nullptr) {
            if (channel == nullptr) {
                return;
            }

            Embed embed;
            embed.color = 14737632;
            std::string path = filepath;

            if (response == (int)SleepyRequest::GetConnectedBots) {
                EmbedField bots;
                embed.title = "Currently Running Bots";
                bots.isInline = false;
                bots.name = "Bot Information";

                bots.value = message;
                embed.fields.push_back(bots);
                sendMessage(channel, "", embed);
                return;
            }
            else if (!path.empty()) {
                auto mode = RequestMode(Sync & ~(ThrowError));
                embed.image.url = "attachment://" + path;
                embed.footer.text = message;
                uploadFile(channel, path, "", embed, mode);
                send_callback(SleepyResponse::RemoveFile, filepath);
            }
            else if (message != nullptr) {
                sendMessage(channel, message);
            }
        }

        // Sends an embed to configured echo channels.
        void send_embed(char* messages, char* channels, char* json) {
            auto map = getChannelsMessages(channels, messages);
            Embed embed = Embed(json);
            for (auto& it : map) {
                sendMessage(it.first, it.second, embed);
            }
        }

        // Sends a file to configured echo channels.
        void send_file(char* messages, char* channels, char* filePath, char* json) {
            auto mode = RequestMode(Sync & ~(ThrowError));
            auto map = getChannelsMessages(channels, messages);
            Embed embed = Embed(json);
            for (auto& it : map) {
                uploadFile(it.first, filePath, it.second, embed, mode);
            }
            send_callback(SleepyResponse::RemoveFile, filePath);
        }

        // Initializes Discord commands on startup.
        void initialize_commands() {
            CommandList::insert_command
            ({
                "click", { "console id", "button", "hold ticks" }, true, false, [](SleepyClient& sleepy, Message& message, std::queue<std::string>& params, SleepyRequest request) {
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
            CommandList::insert_command
            ({
                "leftstick", { "console id", "x pos (0-255)", "y pos (0-255)", "hold ticks" }, true, false, [](SleepyClient& sleepy, Message& message, std::queue<std::string>& params, SleepyRequest request) {
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
            CommandList::insert_command
            ({
                "rightstick", { "console id", "x pos (0-255)", "y pos (0-255)", "hold ticks" }, true, false, [](SleepyClient& sleepy, Message& message, std::queue<std::string>& params, SleepyRequest request) {
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
            CommandList::insert_command
            ({
                "screenshot", { "console id", "format (png or jpg)" }, false, true, [](SleepyClient& sleepy, Message& message, std::queue<std::string>& params, SleepyRequest request) {
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
            CommandList::insert_command
            ({
                "start", { "console id" }, false, true, [](SleepyClient& sleepy, Message& message, std::queue<std::string>& params, SleepyRequest request) {
                    int id;
                    if (!sleepy.number_parse(params.front(), message.channelID, id)) {
                        return;
                    }
                    std::string channel = message.channelID.string();
                    sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id));
                }
            });
            CommandList::insert_command
            ({
                "stop", { "console id" }, false, true, [](SleepyClient& sleepy, Message& message, std::queue<std::string>& params, SleepyRequest request) {
                    int id;
                    if (!sleepy.number_parse(params.front(), message.channelID, id)) {
                        return;
                    }
                    std::string channel = message.channelID.string();
                    sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id));
                }
            });
            CommandList::insert_command
            ({
                "shutdown", {}, false, true, [](SleepyClient& sleepy, Message& message, std::queue<std::string>&, SleepyRequest request) {
                    auto mode = RequestMode(Sync & ~(ThrowError));
                    std::string msg = "***Shutting down the program...***";
                    sleepy.sendMessage(message.channelID, msg, mode);
                    sleepy.send_command_callback(request);
                }
            });
            CommandList::insert_command
            ({
                "hi", {}, false, false, [](SleepyClient& sleepy, Message& message, std::queue<std::string>&, SleepyRequest request) {
                    std::string msg;
                    bool ping = sleepy.m_settings.settings["hello"].substr(0, 3) == "<!>";
                    if (sleepy.m_settings.settings["hello"].empty()) {
                        msg = "<@!" + message.author.ID + ">, you're breathtaking!";
                    }
                    else if (ping) {
                        msg = "<@!" + message.author.ID + ">" + sleepy.m_settings.settings["hello"].replace(0, 3, "");
                    }
                    else msg = sleepy.m_settings.settings["hello"];

                    sleepy.sendMessage(message.channelID, msg);
                    sleepy.send_command_callback(request);
                }
            });
            CommandList::insert_command
            ({
                "ping", {}, false, false, [](SleepyClient& sleepy, Message& message, std::queue<std::string>&, SleepyRequest request) {
                    auto mode = RequestMode(Sync & ~(ThrowError));
                    std::string msg = "Pong!";
                    auto time1 = std::chrono::steady_clock::now();
                    Message reply = sleepy.sendMessage(message.channelID, msg, mode);
                    auto time2 = std::chrono::steady_clock::now();
                    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1).count();

                    std::string latency = msg + " (" + std::to_string(delta) + "ms)";
                    sleepy.editMessage(reply, latency);
                    sleepy.send_command_callback(request);
                }
            });
            CommandList::insert_command
            ({
                "help", {}, false, false, [](SleepyClient& sleepy, Message& message, std::queue<std::string>&, SleepyRequest request) {
                    Embed embed;
                    EmbedField field_owner;
                    EmbedField field_sudo;
                    EmbedField field_misc;
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
            CommandList::insert_command
            ({
                "about", {}, false, false, [](SleepyClient& sleepy, Message& message, std::queue<std::string>&, SleepyRequest request) {
                    Embed embed;
                    embed.color = 14737632;
                    embed.title = "Here's a little bit about me!";

                    User owner;
                    int channels = 0;
                    int users = 0;

                    for (auto& it : sleepy.m_settings.cache) {
                        channels += it.second.channels.size();
                        users += it.second.members.size();
                        if (owner.empty() && !sleepy.m_settings.settings["owner"].empty()) {
                            for (auto& it_owner : it.second.members) {
                                if (it_owner.ID.string() == sleepy.m_settings.settings["owner"]) {
                                    owner = it_owner;
                                    break;
                                }
                            }
                        }
                    }

                    std::string e = u8"é";
                    EmbedField field;
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
            CommandList::insert_command
            ({
                "botinfo", {}, false, false, [](SleepyClient& sleepy, Message& message, std::queue<std::string>&, SleepyRequest request) {
                    std::string channel = message.channelID.string();
                    sleepy.send_command_callback(request, &channel[0]);
                }
            });
            CommandList::insert_command
            ({
                "reloadsettings", {}, false, true, [](SleepyClient& sleepy, Message& message, std::queue<std::string>&, SleepyRequest request) {
                    std::string channel = message.channelID.string();
                    sleepy.send_command_callback(request, &channel[0]);
                }
            });
            CommandList::insert_command
            ({
                "resetcamera", { "console id" }, false, true, [](SleepyClient& sleepy, Message& message, std::queue<std::string>& params, SleepyRequest request) {
                    int id;
                    if (!sleepy.number_parse(params.front(), message.channelID, id)) {
                        return;
                    }
                    std::string channel = message.channelID.string();
                    sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id));
                }
            });
            CommandList::insert_command
            ({
                "resetserial", { "console id" }, false, true, [](SleepyClient& sleepy, Message& message, std::queue<std::string>& params, SleepyRequest request) {
                    int id;
                    if (!sleepy.number_parse(params.front(), message.channelID, id)) {
                        return;
                    }
                    std::string channel = message.channelID.string();
                    sleepy.send_command_callback(request, &channel[0], static_cast<uint64_t>(id));
                }
            });
        }

    private:
        bool m_connected = false;
        std::chrono::steady_clock::time_point m_last_ack = std::chrono::steady_clock::now();
        SleepyCallback m_callback = nullptr;
        SleepyCommandCallback m_cmd = nullptr;

        // Send a Discord command-specific callback to the main program.
        void send_command_callback(int request, char* channel = nullptr, uint64_t console_id = 0, uint16_t button = 0, uint16_t hold_ticks = 0, uint8_t x = 0, uint8_t y = 0) {
            if (m_cmd != nullptr && !m_stopping) {
                m_cmd(request, channel, console_id, button, hold_ticks, x, y);
            }
        }

        // Sends errors from the API to the main program for internal logging.
        void error_response(std::string msg) {
            send_callback(SleepyResponse::API, &msg[0]);
        }

        // Helps ensure a parameter supplied by a user is a valid number.
        bool number_parse(const std::string input, Snowflake<Channel> channel, int& parse) {
            try {
                parse = std::stoi(input);
                return true;
            }
            catch (...) {
                sendMessage(channel, "Invalid command input: \"" + input + "\" is not valid for this parameter.");
                return false;
            }
        }

        void confirmReady() {
            if (m_stopping) {
                m_connected = false;
                m_connecting = false;
                return;
            }

            if (!m_connected && !m_stopping) {
                m_connected = true;
                updateStatus(m_settings.settings["status"], m_last_ack.time_since_epoch().count(), online);
                send_callback(SleepyResponse::Connected, "[SleepyDiscord]: Connected successfully, client is ready.");
                m_connecting = false;
            }
        }

        std::unordered_map<std::string, std::string> getChannelsMessages(char* channels, char* messages) {
            auto channelQ = CommandList::split_input(channels, ',');
            auto messageQ = CommandList::split_input(messages, '|');
            std::unordered_map<std::string, std::string> channel_map;
            while (!channelQ.empty()) {
                bool size = messageQ.size() > 0;
                channel_map.emplace(channelQ.front(), size ? messageQ.front() : "");
                channelQ.pop();
                if (size) {
                    messageQ.pop();
                }
            }
            return channel_map;
        }

        using DiscordClient::DiscordClient;
        void onServer(Server server) override {
            try {
                if (server.empty()) {
                    return;
                }

                std::string id = server.ID.string();
                bool updated = false;
                for (auto& it : m_settings.cache) {
                    if (it.first == id) {
                        m_settings.cache.at(id) = server;
                        updated = true;
                        break;
                    }
                }

                if (!updated) {
                    m_settings.cache.try_emplace(id, server);
                }
                confirmReady();
            }
            catch (...) {}
        }

        void onHeartbeat() override {
            auto time = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::minutes>(time - m_last_ack).count();
            if (delta > 5 && m_connected) {
                m_connected = false;
                send_callback(SleepyResponse::Disconnected, "[SleepyDiscord]: Event: Last heartbeat ack more than 5 minutes ago.");
            }
        }

        void onHeartbeatAck() override {
            auto time = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::seconds>(time - m_last_ack).count();
            if (delta > 20) {
                updateStatus(m_settings.settings["status"], m_last_ack.time_since_epoch().count(), idle);
                m_last_ack = time;
            }
        }

        void onError(ErrorCode code, const std::string msg) override {
            std::string error_message = "[SleepyDiscord]: Error code: " + std::to_string(code) + ".\n" + msg;
            error_response(error_message);
        }

        void onMessage(Message message) override {
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
                updateStatus(m_settings.settings["status"], m_last_ack.time_since_epoch().count(), online);
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
    void client_connect() {
        if (m_client != nullptr && !m_client->m_connecting && !m_client->m_stopping) {
            try {
                m_client_thread = new std::thread(client_run);
            }
            catch (...) {}
        }
    }

    // Main run thread.
    void client_run() {
        try {
            m_client->m_connecting = true;
            m_client->setIntents(Intent::SERVER_MESSAGES, Intent::SERVERS, Intent::SERVER_MEMBERS, Intent::SERVER_PRESENCES);
            m_client->run();
        }
        catch (...) {}
    }

    // Disconnect and clean up.
    void client_disconnect() {
        try {
            if (m_client != nullptr && !m_client->isQuiting()) {
                m_client->m_stopping = true;
                m_client->quit();
                m_client.reset();
            }
            if (m_client_thread != nullptr && m_client_thread->joinable()) {
                m_client_thread->join();
            }
        }
        catch (...) {}
    }

    // Responses and requests from the main program.
    void program_response(int response, char* channel, char* message, char* filepath) {
        if (m_client != nullptr) {
            if (response == SleepyRequest::Terminate) {
                if (m_client->m_connecting || m_client->m_stopping) {
                    return;
                }

                try {
                    client_disconnect();
                }
                catch (...) {}
            }
            else m_client->program_response(response, channel, message, filepath);
        }
    }

    // Parse and apply user settings from the main program, do initial client setup.
    void apply_settings(SleepyCallback callback, SleepyCommandCallback cmd_callback, char* w_channels, char* sudo, char* params, bool suffix) {
        if (m_client != nullptr && (m_client->m_connecting || m_client->m_stopping)) {
            return;
        }

        SleepySettings settings;
        std::queue<std::string> qw_channels = CommandList::split_input((std::string)w_channels, ',');
        std::queue<std::string> q_sudo = CommandList::split_input((std::string)sudo, ',');
        std::queue<std::string> q_settings = CommandList::split_input((std::string)params, '|');
        settings.token = q_settings.front();
        q_settings.pop();

        while (!qw_channels.empty()) {
            settings.whitelist_channels.push_back(qw_channels.front());
            qw_channels.pop();
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

        if (m_client == nullptr) {
            m_client = std::unique_ptr<SleepyClient>(new SleepyClient(settings.token, 1));
            m_client->set_callbacks(callback, cmd_callback);
            m_client->send_callback(SleepyResponse::SettingsInitialized, "[SleepyDiscord]: Settings initialized.");
        }
        else m_client->send_callback(SleepyResponse::SettingsUpdated, "[SleepyDiscord]: Settings reloaded.");

        if (!m_client->m_settings.cache.empty()) {
            settings.cache = m_client->m_settings.cache;
        }
        m_client->m_settings = settings;

        bool initialized = CommandList::command_map.size() > 0;
        if (!initialized) {
            m_client->initialize_commands();
        }

        if (CommandList::command_map.size() == 0) {
            std::string error_msg = "[SleepyDiscord]: Unknown error: Failed to initialize Discord commands.";
            m_client->send_callback(SleepyResponse::Fault, &error_msg[0]);
        }
        else m_client->send_callback(SleepyResponse::CommandsInitialized, "[SleepyDiscord]: Discord commands initialized.");
    }

    void sendMessage(char* channels, char* messages, char* json, char* filePath) {
        if (m_client != nullptr) {
            if (filePath != nullptr) {
                m_client->send_file(messages, channels, filePath, json);
            }
            else if (json != nullptr) {
                m_client->send_embed(messages, channels, json);
            }
        }
    }
}