#include "DebugServer.h"

#include "src/Utils/BufferedLogger.h"
#include "src/Utils/Logger.h"

namespace Servers
{
DebugServer::DebugServer(SadLampWebSocketServer& web_socket_server)
  : web_socket_server_(web_socket_server)
{
}

void
DebugServer::init()
{
    web_socket_server_.set_handler(SadLampWebSocketServer::Event::START_READING_LOGS,
                                   [&](uint8_t client_id, String const& parameters) {
                                       debugger_clients_ids_.push_back(client_id);
                                       send_buffered_logs();
                                   });
    web_socket_server_.set_handler(
        SadLampWebSocketServer::Event::STOP_READING_LOGS, [&](uint8_t client_id, String const& parameters) {
            send_buffered_logs();
            debugger_clients_ids_.erase(
                std::remove(debugger_clients_ids_.begin(), debugger_clients_ids_.end(), client_id),
                debugger_clients_ids_.end());
        });
    web_socket_server_.set_handler(
        SadLampWebSocketServer::Event::DISCONNECTED, [&](uint8_t client_id, String const& parameters) {
            debugger_clients_ids_.erase(
                std::remove(debugger_clients_ids_.begin(), debugger_clients_ids_.end(), client_id),
                debugger_clients_ids_.end());
        });

    DEBUG_PRINTLN("Debug server initialized");
}

void
DebugServer::loop()
{
    static unsigned long last_print{0};
    if (millis() - last_print > 1000) {
        send_buffered_logs();
        last_print = millis();
    }
}

void
DebugServer::send_buffered_logs()
{
    if (!debugger_clients_ids_.empty() && (Utils::BufferedLogger::instance().get_log().length() > 0)) {
        auto& log = Utils::BufferedLogger::instance().get_log();
        for (auto client_id : debugger_clients_ids_) {
            web_socket_server_.send(client_id, log);
        }
        Utils::BufferedLogger::instance().clear();
    }
}

}  // namespace Servers
