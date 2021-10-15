#ifndef SRC_DEBUGSERVER_H_
#define SRC_DEBUGSERVER_H_

#include <vector>

#include "SadLampWebSocketServer.h"

// Uses BufferedLogger singleton to get buffered logs and send them to all connected debugger clients
class DebugServer
{
public:
    explicit DebugServer(SadLampWebSocketServer& web_socket_server);
    void init();
    void loop();

private:
    void send_buffered_logs();

    SadLampWebSocketServer& web_socket_server_;
    std::vector<uint8_t>    debugger_clients_ids_;
};

#endif  // SRC_DEBUGSERVER_H_
