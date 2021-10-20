#include "SadLampWebServer.h"

#include <map>

#include <Update.h>

#include "src/Utils/Logger.h"

namespace
{
static const char TEXT_PLAIN[]     = "text/plain";
static const char TEXT_JSON[]      = "text/json";
static const char FS_INIT_ERROR[]  = "FS INIT ERROR";
static const char FILE_NOT_FOUND[] = "FileNotFound";

String
getContentType(String const& filename)
{
    if (filename.endsWith(".htm")) {
        return "text/html";
    }
    else if (filename.endsWith(".html")) {
        return "text/html";
    }
    else if (filename.endsWith(".css")) {
        return "text/css";
    }
    else if (filename.endsWith(".js")) {
        return "application/javascript";
    }
    else if (filename.endsWith(".png")) {
        return "image/png";
    }
    else if (filename.endsWith(".gif")) {
        return "image/gif";
    }
    else if (filename.endsWith(".jpg")) {
        return "image/jpeg";
    }
    else if (filename.endsWith(".ico")) {
        return "image/x-icon";
    }
    else if (filename.endsWith(".xml")) {
        return "text/xml";
    }
    else if (filename.endsWith(".pdf")) {
        return "application/x-pdf";
    }
    else if (filename.endsWith(".zip")) {
        return "application/x-zip";
    }
    else if (filename.endsWith(".gz")) {
        return "application/x-gzip";
    }
    return "text/plain";
}

}  // namespace

namespace Servers
{
SadLampWebServer::SadLampWebServer()
  : web_server_{port_}
  , handlers_{
        nullptr,
    }
{
}

void
SadLampWebServer::init()
{
    // SSDP description
    web_server_.on("/ssdp_description.xml", HTTP_GET, [this]() {
        if (get_ssdp_description_handler_ != nullptr) {
            get_ssdp_description_handler_(std::move(web_server_.client()));
        }
    });

    // HTTP pages to work with file system
    // List directory
    web_server_.on("/list", HTTP_GET, [this]() { handle_file_list(); });
    // Load editor
    web_server_.on("/edit", HTTP_GET, [this]() {
        if (!handle_file_read("/edit.htm")) {
            reply_not_found(FILE_NOT_FOUND);
        }
    });
    // Create file
    web_server_.on("/edit", HTTP_PUT, [this]() { handle_file_create(); });
    // Delete file
    web_server_.on("/edit", HTTP_DELETE, [this]() { handle_file_delete(); });

    // Upload file
    // - first callback is called after the request has ended with all parsed arguments
    // - second callback handles file upload at that location
    web_server_.on(
        "/edit", HTTP_POST, [this]() { reply_ok(); }, [this]() { handle_file_upload(); });

    // Called when the url is not defined here
    // Use it to load content from FS
    web_server_.onNotFound([this]() {
        if (!handle_file_read(web_server_.uri())) {
            reply_not_found(FILE_NOT_FOUND);
        }
    });

    // HTTP pages for Settings
    // Update ESP firmware
    web_server_.on(
        "/update",
        HTTP_POST,
        [this]() {
            // This code is from example. Not sure if we really need it
            //
            // web_server_.sendHeader("Connection", "close");
            // web_server_.sendHeader("Access-Control-Allow-Origin", "*");
            // reply_ok_with_msg(Update.hasError() ? "FAIL" : "ESP firmware update completed! Rebooting...");

            // If there were errors during uploading of file, this is the only place, where we can send message to
            // client about it
            if (esp_firmware_upload_error_.length() != 0) {
                DEBUG_PRINTLN(String{"Sending error to WebUI: "} + esp_firmware_upload_error_);
                reply_server_error(esp_firmware_upload_error_);
                esp_firmware_upload_error_ = "";
            }
            else {
                web_server_.client().setNoDelay(true);
                reply_ok_with_msg("ESP firmware update completed! Rebooting...");
                delay(100);
                web_server_.client().stop();

                DEBUG_PRINTLN("Rebooting...");
                handle_reboot_esp();  // Schedule reboot
            }
        },
        [this]() { handle_esp_sw_upload(); });

    web_server_.on("/reset_wifi_settings", HTTP_POST, [this]() { handle_reset_wifi_settings(); });
    web_server_.on("/reboot_esp", HTTP_POST, [this]() {
        reply_ok();
        handle_reboot_esp();
    });

    web_server_.begin();
    DEBUG_PRINTLN("Web server initialized");
}

void
SadLampWebServer::loop()
{
    web_server_.handleClient();
}

void
SadLampWebServer::set_handler(Event event, EventHandler handler)
{
    handlers_[static_cast<size_t>(event)] = handler;
}

void
SadLampWebServer::set_get_ssdp_description_handler(GetSsdpDescriptionHandler handler)
{
    get_ssdp_description_handler_ = handler;
}

void
SadLampWebServer::reply_ok()
{
    web_server_.send(200, TEXT_PLAIN, "");
}

void
SadLampWebServer::reply_ok_with_msg(String const& msg)
{
    web_server_.send(200, TEXT_PLAIN, msg);
}

void
SadLampWebServer::reply_ok_json_with_msg(String const& msg)
{
    web_server_.send(200, TEXT_JSON, msg);
}

void
SadLampWebServer::reply_not_found(String const& msg)
{
    DEBUG_PRINTLN(msg);
    web_server_.send(404, TEXT_PLAIN, msg);
}

void
SadLampWebServer::reply_bad_request(String const& msg)
{
    DEBUG_PRINTLN(msg);
    web_server_.send(400, TEXT_PLAIN, msg + "\r\n");
}

void
SadLampWebServer::reply_server_error(String const& msg)
{
    DEBUG_PRINTLN(msg);
    web_server_.send(500, TEXT_PLAIN, msg + "\r\n");
}

void
SadLampWebServer::handle_file_list()
{
    if (!web_server_.hasArg("dir")) {
        reply_bad_request("DIR ARG MISSING");
    }
    String path{web_server_.arg("dir")};
    if (!Utils::FS::is_directory(path)) {
        return reply_bad_request("BAD PATH");
    }

    DEBUG_PRINTLN(String{"handle_file_list: "} + path);
    auto output = Utils::FS::get_file_list_json(path);
    web_server_.sendContent(Utils::FS::get_file_list_json(path));
}

// Handle the creation/rename of a new file
// Operation      | req.responseText
// ---------------+--------------------------------------------------------------
// Create file    | parent of created file
// Create folder  | parent of created folder
// Rename file    | parent of source file
// Move file      | parent of source file, or remaining ancestor
// Rename folder  | parent of source folder
// Move folder    | parent of source folder, or remaining ancestor
void
SadLampWebServer::handle_file_create()
{
    if (!web_server_.hasArg("path")) {
        reply_bad_request("PATH ARG MISSING");
    }

    String path{web_server_.arg("path")};
    if (path.isEmpty()) {
        return reply_bad_request("PATH ARG MISSING");
    }
    if (Utils::FS::exists(path)) {
        return reply_bad_request("FILE EXISTS");
    }

    String src{web_server_.arg("src")};
    if (src.isEmpty()) {
        // No source specified: creation
        DEBUG_PRINTLN(String{"handle_file_create: "} + path);
        if (path.endsWith("/")) {
            // Create a folder
            auto result{Utils::FS::create_folder(std::move(path))};
            if (!result.first) {
                return reply_server_error(result.second);
            }
            reply_ok_with_msg(result.second);
        }
        else {
            // Create a file
            auto result{Utils::FS::create_file(std::move(path))};
            if (!result.first) {
                return reply_server_error(result.second);
            }
            Utils::FS::close(result.first);
            reply_ok_with_msg(result.second);
        }
    }
    else {
        // Source specified: rename
        if (src == "/") {
            return reply_bad_request("BAD SRC");
        }
        if (!Utils::FS::exists(src)) {
            return reply_bad_request("SRC FILE NOT FOUND");
        }

        DEBUG_PRINTLN(String{"handle_file_create: renaming "} + src + " to " + path);
        auto result{Utils::FS::rename(std::move(src), std::move(path))};
        if (!result.first) {
            return reply_server_error(result.second);
        }
        reply_ok_with_msg(result.second);
    }
}

// Handle a file deletion request
// Operation      | req.responseText
// ---------------+--------------------------------------------------------------
// Delete file    | parent of deleted file, or remaining ancestor
// Delete folder  | parent of deleted folder, or remaining ancestor
void
SadLampWebServer::handle_file_delete()
{
    if (web_server_.args() == 0) {
        reply_server_error("BAD ARGS");
        return;
    }
    String path{web_server_.arg(0)};
    if (path.isEmpty()) {
        return reply_bad_request("PATH ARG MISSING");
    }
    if (path == "/") {
        return reply_bad_request("BAD PATH");
    }
    if (!Utils::FS::exists(path)) {
        return reply_not_found(FILE_NOT_FOUND);
    }

    DEBUG_PRINTLN(String{"handle_file_delete: "} + path);
    auto result{Utils::FS::remove(path)};
    if (!result.first) {
        return reply_server_error(result.second);
    }
    reply_ok_with_msg(result.second);
}

void
SadLampWebServer::handle_file_upload()
{
    if (web_server_.uri() != "/edit") {
        return;
    }

    HTTPUpload& upload{web_server_.upload()};
    if (upload.status == UPLOAD_FILE_START) {
        DEBUG_PRINTLN(String{"handle_file_upload: filename: "} + upload.filename);
        auto result{Utils::FS::create_file(std::move(upload.filename))};
        if (!result.first) {
            return reply_server_error(result.second);
        }
        upload_file_ = result.first;
        DEBUG_PRINTLN("handle_file_upload: STARTED");
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (upload_file_) {
            size_t bytesWritten{Utils::FS::write(upload_file_, upload.buf, upload.currentSize)};
            if (bytesWritten != upload.currentSize) {
                return reply_server_error("WRITE FAILED");
            }
        }
        DEBUG_PRINTLN(String{"Upload: WRITE, Bytes: "} + String(upload.currentSize));
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (upload_file_) {
            Utils::FS::close(upload_file_);
        }
        DEBUG_PRINTLN(String{"Upload: END, Size: "} + String(upload.totalSize));
    }
}

bool
SadLampWebServer::handle_file_read(String path)
{
    DEBUG_PRINTLN(String{"handle_file_read: "} + path);

    if (path.endsWith("/")) {
        path += "index.htm";
    }

    String contentType{web_server_.hasArg("download") ? "application/octet-stream" : getContentType(path)};
    if (!Utils::FS::is_file(path)) {
        // File not found, try gzip version
        path += ".gz";
    }

    if (!Utils::FS::is_file(path)) {
        return false;
    }

    Utils::FS::File file{Utils::FS::open(path, Utils::FS::OpenMode::kRead)};
    if (web_server_.streamFile(file, contentType) != file.size()) {
        DEBUG_PRINTLN("Sent less data than expected!");
        Utils::FS::close(file);
        return false;
    }
    Utils::FS::close(file);
    return true;
}

void
SadLampWebServer::handle_esp_sw_upload()
{
    // NOTE! There is no way in HTML to stop client from uploading file. So, if it starts uploading of too big file and
    // ESP, as a server, identifies it, the only thing ESP can do is to mark this uploaded file as invalid, receive it
    // completely and only then send error. But error should be sent only once, otherwise when client will finish
    // uploading it will receive all error messages, sent during upload, at one time

    if (web_server_.args() < 1) {
        esp_firmware_upload_error_ = "ERROR: file size is not provided!";
        return;
    }

    HTTPUpload& upload{web_server_.upload()};
    if (upload.status == UPLOAD_FILE_START) {
        // TODO(migration to ESP32): use new code for update from examples for ESP32 (+ adaptions). Test it!
        auto file_size = web_server_.arg("file_size").toInt();
        if (upload.name == "filesystem") {
            if (!Update.begin(file_size, U_SPIFFS)) {
                Update.end();
                Update.printError(DGB_STREAM);
                esp_firmware_upload_error_ =
                    "ERROR: not enough space on SPIFFS (upload.name == 'filesystem')! Available: " +
                    String{Utils::FS::total_bytes() - Utils::FS::used_bytes()};
                return;
            }
        }
        else {
            if (!Update.begin(file_size, U_FLASH)) {  // start with max available size
                Update.end();
                Update.printError(DGB_STREAM);
                esp_firmware_upload_error_ =
                    "ERROR: not enough space on U_FLASH (upload.name != 'filesystem')! Available: " +
                    String{ESP.getFreeSketchSpace()};
                return;
            }
        }
        // if (!Update.begin(file_size)) {
        //     Update.end();
        //     Update.printError(DGB_STREAM);
        //     esp_firmware_upload_error_ =
        //         "ERROR: not enough space! Available: ") + String(ESP.getFreeSketchSpace();
        //     return;
        // }

        DGB_STREAM.setDebugOutput(true);
        // WiFiUDP::stop();
        DEBUG_PRINTLN(String{"Start uploading file: "} + upload.filename);
        esp_firmware_upload_error_ = "";
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (esp_firmware_upload_error_.length() != 0) {
            // Ignore uploading of file which is already marked as invalid. Do NOT send any errors to client here,
            // because there is no way to stop uploading but all sent messages will come together to client when it
            // finish uploading
            return;
        }

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.end();
            Update.printError(DGB_STREAM);
            esp_firmware_upload_error_ = String{"ERROR: can not write file! Update error "} + String(Update.getError());
            DGB_STREAM.setDebugOutput(false);
            return;
        }
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (esp_firmware_upload_error_.length() != 0) {
            // Ignore finalizing of upload of file which is already marked as invalid. Do NOT send any errors to client
            // here, because there is no way to stop uploading but all sent messages will come together to client when
            // it finish uploading
            return;
        }

        if (Update.end(true)) {  // true to set the size to the current progress
            DEBUG_PRINTLN(String{"Update completed. Uploaded file size: "} + String(upload.totalSize));
        }
        else {
            Update.end();
            Update.printError(DGB_STREAM);
            esp_firmware_upload_error_ =
                String{"ERROR: can not finalize file! Update error "} + String(Update.getError());
        }
        DGB_STREAM.setDebugOutput(false);
    }
    else {
        Update.end();
        Update.printError(DGB_STREAM);
        esp_firmware_upload_error_ = "ERROR: uploading of file was aborted";
        DGB_STREAM.setDebugOutput(false);
        return;
    }
    yield();
}

void
SadLampWebServer::handle_reset_wifi_settings()
{
    reply_ok();
    delay(500);  // Add delay to make sure response is sent to client

    DEBUG_PRINTLN("handle_reset_wifi_settings");
    if (handlers_[static_cast<size_t>(Event::RESET_WIFI_SETTINGS)] != nullptr) {
        handlers_[static_cast<size_t>(Event::RESET_WIFI_SETTINGS)]("");
    }
}

void
SadLampWebServer::handle_reboot_esp()
{
    DEBUG_PRINTLN("handle_reboot_esp");
    if (handlers_[static_cast<size_t>(Event::REBOOT_ESP)] != nullptr) {
        handlers_[static_cast<size_t>(Event::REBOOT_ESP)]("");
    }
}

}  // namespace Servers
