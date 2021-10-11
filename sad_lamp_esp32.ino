// TODO(migration to ESP32): use Serial temporary

// #include <ESP8266SSDP.h>
// #include <ESP8266WiFi.h>
// #include <FS.h>
// #include <FTPServer.h>
#include <WiFiManager.h>

// #include "src/ArduinoCommunication.h"
// #include "src/DebugServer.h"
// #include "src/WebServer.h"
// #include "src/WebSocketServer.h"
#include "src/Logger.h"

namespace
{
// WebSocketServer      web_socket_server;  // Use this instance as facade to implement other servers (ex. DebugServer)
// WebServer            web_server;
// DebugServer          debug_server(web_socket_server);
// ArduinoCommunication arduino_communication(web_socket_server, web_server, RESET_PIN);
// FTPServer            ftp_server(SPIFFS);

// bool                    is_reboot_requested{false};
// constexpr unsigned long reboot_delay{100};  // Reboot happens 100ms after receiving reboot request
}  // namespace

void
setup()
{
    WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP

    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    // // Initiate communication with Arduino as soon as possible, right after Serial is initialized.
    // arduino_communication.init();
    // delay(3000);

    init_wifi();
    // debug_server.init();
    // SPIFFS.begin();
    // web_server.init();
    // SSDP_init();
    // ftp_init();

    // web_server.set_handler(WebServer::Event::REBOOT_ESP, [&](String const& filename) { is_reboot_requested = true; });

    // // Request to reset wifi settings can come from WebUI and from Arduino (via potentiometer and switching from auto to
    // // manual mode and vice versa)
    // auto WiFiSettingsResetHandler = [&]() {
    //     WiFiManager wifi_manager(DGB_STREAM);
    //     wifi_manager.erase();
    //     is_reboot_requested = true;
    // };
    // web_server.set_handler(WebServer::Event::RESET_WIFI_SETTINGS, [&](String const&) { WiFiSettingsResetHandler(); });
    // arduino_communication.set_handler(ArduinoCommunication::Event::RESET_WIFI_SETTINGS,
    //                                   [&]() { WiFiSettingsResetHandler(); });
}

void
loop()
{
    // arduino_communication.loop();
    // debug_server.loop();
    // web_server.loop();
    // ftp_server.handleFTP();

    // if (is_reboot_requested) {
    //     // If reboot of ESP is requested, it is triggered not immediately, but with reboot_delay. It lets ESP to finish
    //     // some actions, ex. sending responses to Web-clients, etc.
    //     static unsigned long reboot_request_time = millis();
    //     if (millis() - reboot_request_time >= reboot_delay) {
    //         ESP.restart();
    //         delay(5000);
    //     }
    // }
}


void
configModeCallback(WiFiManager* wifiManager)
{
    DEBUG_PRINT(F("Could not connect to Access Point. Entering config mode. Connect to \""));
    DEBUG_PRINT(wifiManager->getConfigPortalSSID());
    DEBUG_PRINT(F("\" WiFi network and open http://"));
    DEBUG_PRINT(WiFi.softAPIP());
    DEBUG_PRINTLN(F(" (if it didn't happen automatically) to configure WiFi settings of SAD-lamp"));
}

void
init_wifi()
{
    // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager(DGB_STREAM);

    // set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setWiFiAutoReconnect(true);

    // Reset settings - wipe credentials for testing
    // wifiManager.resetSettings();

    // Aautomatically connect using saved credentials if they exist
    // If connection fails it starts an access point with the specified name (here  "SAD-Lamp_AP"). If name is empty,
    // it will auto generate it in format "ESP-chipid". If password is blank it will be anonymous
    // And goes into a blocking loop awaiting for configuration
    // If access point "SAD-Lamp_AP" is established, it's configuration is available by address http://192.168.4.1
    if (!wifiManager.autoConnect(PSTR("SAD-Lamp_AP") /*, "password"*/)) {
        DEBUG_PRINTLN(F("failed to connect and hit timeout"));
        delay(3000);
        // if we still have not connected restart and try all over again
        ESP.restart();
        delay(5000);
    }

    DEBUG_PRINT(F("Connected to Access Point \""));
    DEBUG_PRINT(WiFi.SSID());
    DEBUG_PRINT(F("\". IP: "));
    DEBUG_PRINT(WiFi.localIP().toString());
    DEBUG_PRINT(F("; mask: "));
    DEBUG_PRINT(WiFi.subnetMask().toString());
    DEBUG_PRINT(F("; gateway: "));
    DEBUG_PRINT(WiFi.gatewayIP().toString());
    DEBUG_PRINTLN();
}

/*
void
SSDP_init()
{
    SSDP.setDeviceType(F("upnp:rootdevice"));
    SSDP.setSchemaURL(F("description.xml"));
    SSDP.setHTTPPort(80);
    SSDP.setName(F("SAD-Lamp"));
    SSDP.setSerialNumber("1");
    SSDP.setURL("/");
    SSDP.setModelName(F("SAD-Lamp"));
    SSDP.setModelNumber("1");
    // SSDP.setModelURL("http://adress.ru/page/");
    SSDP.setManufacturer(F("Lambin Alexey"));
    // SSDP.setManufacturerURL("http://www.address.ru");
    SSDP.begin();

    DEBUG_PRINTLN(F("SSDP initialized"));
}

void
ftp_init()
{
    // NOTE:
    // FTP server consumes 1% of flash and 2% of RAM
    // This is IMPORTANT: for details how to configure FTP-client visit https://github.com/nailbuster/esp8266FTPServer
    // In particular:
    // 0. Take more stable fork: https://github.com/charno/FTPClientServer
    // 1. You need to setup Filezilla(or other client) to only allow 1 connection
    // 2. It does NOT support any encryption, so you'll have to disable any form of encryption
    // 3. Look at "Issues" folder - looks likr this library is quite unstable

    // Setup the ftp server with username and password
    // ports are defined in FTPCommon.h, default is
    //   21 for the control connection
    //   50009 for the data connection (passive mode by default)
    ftp_server.begin(F("esp8266"), F("esp8266"));
    DEBUG_PRINTLN(F("FTP server initialized"));
}
*/