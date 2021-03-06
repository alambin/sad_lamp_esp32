// TODO(migration to ESP32): use Serial temporary

// TODO: replace all F(), FPSTR(), and PSTR()

// NOTE! To make code buildable you should open platform.txt (ex. in this folder
// C:\Users\<user>\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6) and add "-I{build.path}/sketch" to the
// end of compiler.cpreprocessor.flags variable definition.
// Its a shame for Arduino IDE developers not to let users to configure include pathes! Change above is just enabling
// common practive - its adding root of source tree to include path.
#include <ESP32SSDP.h>
#include <FTPServer.h>
#include <SPIFFS.h>
#include <WiFiManager.h>


// #include "src/ArduinoCommunication.h"
#include "src/Servers/DebugServer.h"
#include "src/Servers/SadLampWebServer.h"
#include "src/Servers/SadLampWebSocketServer.h"
#include "src/Utils/Logger.h"

namespace
{
Servers::SadLampWebSocketServer
                          web_socket_server;  // Use this instance as facade to implement other servers (ex. DebugServer)
Servers::SadLampWebServer web_server;
Servers::DebugServer      debug_server(web_socket_server);
// ArduinoCommunication arduino_communication(web_socket_server, web_server, RESET_PIN);
FTPServer ftp_server(SPIFFS);

bool                    is_reboot_requested{false};
constexpr unsigned long reboot_delay{100};  // Reboot happens 100ms after receiving reboot request
}  // namespace

void
setup()
{
    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    // // Initiate communication with Arduino as soon as possible, right after Serial is initialized.
    // arduino_communication.init();
    // delay(3000);

    init_wifi();
    SPIFFS.begin();
    debug_server.init();
    web_socket_server.init();
    web_server.init();
    SSDP_init();
    ftp_init();

    // TODO: in 1/4-1/10 times after reboot ESP32 can not connect with previous WiFi settings and you have to reset it
    // manually 1 more time
    web_server.set_handler(Servers::SadLampWebServer::Event::REBOOT_ESP,
                           [&is_reboot_requested](String const& filename) { is_reboot_requested = true; });

    // Request to reset wifi settings can come from WebUI and from Arduino (via potentiometer and switching from auto to
    // manual mode and vice versa)
    auto WiFiSettingsResetHandler = [&is_reboot_requested](String const&) {
        WiFiManager wifi_manager(DGB_STREAM);
        wifi_manager.erase();
        is_reboot_requested = true;
    };
    web_server.set_handler(Servers::SadLampWebServer::Event::RESET_WIFI_SETTINGS, WiFiSettingsResetHandler);
    // arduino_communication.set_handler(ArduinoCommunication::Event::RESET_WIFI_SETTINGS, WiFiSettingsResetHandler);

    web_server.set_get_ssdp_description_handler([&SSDP](WiFiClient wiFiClient) { SSDP.schema(std::move(wiFiClient)); });
}

void
loop()
{
    // arduino_communication.loop();
    debug_server.loop();
    web_server.loop();
    web_socket_server.loop();
    ftp_server.handleFTP();

    if (is_reboot_requested) {
        // If reboot of ESP is requested, it is triggered not immediately, but with reboot_delay. It lets ESP to finish
        // some actions, ex. sending responses to Web-clients, etc.
        static unsigned long reboot_request_time = millis();
        if (millis() - reboot_request_time >= reboot_delay) {
            ESP.restart();
            delay(5000);
        }
    }
}


void
configModeCallback(WiFiManager* wifiManager)
{
    DEBUG_PRINT("Could not connect to Access Point. Entering config mode. Connect to \"");
    DEBUG_PRINT(wifiManager->getConfigPortalSSID());
    DEBUG_PRINT("\" WiFi network and open http://");
    DEBUG_PRINT(WiFi.softAPIP());
    DEBUG_PRINTLN(" (if it didn't happen automatically) to configure WiFi settings of SAD-lamp");
}

void
init_wifi()
{
    WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP

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
    if (!wifiManager.autoConnect("SAD-Lamp_AP" /*, "password"*/)) {
        DEBUG_PRINTLN("failed to connect and hit timeout");
        delay(3000);
        // if we still have not connected restart and try all over again
        ESP.restart();
        delay(5000);
    }

    DEBUG_PRINT("Connected to Access Point \"");
    DEBUG_PRINT(WiFi.SSID());
    DEBUG_PRINT("\". IP: ");
    DEBUG_PRINT(WiFi.localIP().toString());
    DEBUG_PRINT("; mask: ");
    DEBUG_PRINT(WiFi.subnetMask().toString());
    DEBUG_PRINT("; gateway: ");
    DEBUG_PRINT(WiFi.gatewayIP().toString());
    DEBUG_PRINTLN();
}

void
SSDP_init()
{
    SSDP.setDeviceType("upnp:rootdevice");
    SSDP.setSchemaURL("ssdp_description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("SAD-Lamp");
    SSDP.setSerialNumber("1");
    SSDP.setURL("/");
    SSDP.setModelName("SAD-Lamp");
    SSDP.setModelNumber("1");
    // SSDP.setModelURL("http://adress.ru/page/");
    SSDP.setManufacturer("Lambin Alexey");
    // SSDP.setManufacturerURL("http://www.address.ru");
    auto res = SSDP.begin();

    DEBUG_PRINTLN(String{"SSDP initialization result: "} + (res ? "SUCCESS" : "FAILED"));
}

void
ftp_init()
{
    // NOTE:
    // This is IMPORTANT: for details how to configure FTP-client visit https://github.com/nailbuster/esp8266FTPServer
    // In particular:
    // 0. Take more stable fork: https://github.com/charno/FTPClientServer
    // 1. You need to setup Filezilla(or other client) to only allow 1 connection
    // 2. It does NOT support any encryption, so you'll have to disable any form of encryption
    // 3. Look at "Issues" folder - looks like this library is quite unstable

    // Setup the ftp server with username and password
    // ports are defined in FTPCommon.h, default is
    //   21 for the control connection
    //   50009 for the data connection (passive mode by default)
    ftp_server.begin("esp32", "esp32");

    DEBUG_PRINTLN("FTP server initialized");
}
