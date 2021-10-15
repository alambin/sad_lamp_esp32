#include "SadLampWebServer.h"

#include <map>

#include <ESP32SSDP.h>
#include <SPIFFS.h>
#include <Update.h>

#include "src/Logger/Logger.h"

namespace
{
static const char TEXT_PLAIN[]     = "text/plain";
static const char TEXT_JSON[]      = "text/json";
static const char FS_INIT_ERROR[]  = "FS INIT ERROR";
static const char FILE_NOT_FOUND[] = "FileNotFound";

String
check_for_unsupported_path(String const& filename)
{
    String error;
    if (!filename.startsWith("/")) {
        error += "!NO_LEADING_SLASH! ";
    }
    if (filename.indexOf("//") != -1) {
        error += "!DOUBLE_SLASH! ";
    }
    if (filename.endsWith("/")) {
        error += "!TRAILING_SLASH! ";
    }
    return error;
}

// As some FS (e.g. LittleFS) delete the parent folder when the last child has been removed,
// return the path of the closest parent still existing
String
last_existing_parent(String path)
{
    while (!path.isEmpty() && !SPIFFS.exists(path)) {
        if (path.lastIndexOf('/') > 0) {
            path = path.substring(0, path.lastIndexOf('/'));
        }
        else {
            path.clear();  // No slash => the top folder does not exist
        }
    }
    DEBUG_PRINTLN(String{"Last existing parent: '"} + path + "'");
    return path;
}

bool
case_insensitive_string_less(String const& str1, String const& str2)
{
    auto min_size = (str1.length() < str2.length()) ? str1.length() : str2.length();
    for (size_t i = 0; i < min_size; ++i) {
        if (std::toupper(str1[i]) != std::toupper(str2[i])) {
            return std::toupper(str1[i]) < std::toupper(str2[i]);
        }
    }
    return str1.length() < str2.length();
}

auto filename_less = [](String const& l, String const& r) {
    // return std::lexicographical_compare(l.begin(), l.end(), r.begin(), r.end());
    if ((l[0] == '/') && (r[0] != '/')) {
        // "l" is directory
        return true;
    }
    else if ((r[0] == '/') && (l[0] != '/')) {
        // "r" is directory
        return false;
    }
    else if ((l[0] == '/') && (r[0] == '/')) {
        // Both strings are directory names
        return case_insensitive_string_less(l.substring(1), r.substring(1));
    }
    // Regular file names
    return case_insensitive_string_less(l, r);
};

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
        // TODO: implement it in main.ino using call set_handler. Doing so we will get rid of dependency on SSDP from
        // SadLampWebServer
        SSDP.schema(web_server_.client());
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
    // Use it to load content from SPIFFS
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
    if (path != "/" && !SPIFFS.exists(path)) {
        return reply_bad_request("BAD PATH");
    }

    DEBUG_PRINTLN(String{"handle_file_list: "} + path);
    fs::File root = SPIFFS.open(path);

    // Use the same string for every line
    String                                            name;
    std::map<String, String, decltype(filename_less)> files_data(filename_less);
    String                                            output;
    output.reserve(64);
    while (File file = root.openNextFile()) {
        String error{check_for_unsupported_path(file.name())};
        if (error.length() > 0) {
            DEBUG_PRINTLN(String{"Ignoring "} + error + file.name());
            continue;
        }

        output += "{\"type\":\"";
        if (file.isDirectory()) {
            // There is no directories on SPIFFS. This code is here for compatibility with another (possible)
            // filesystems
            output += "dir";
        }
        else {
            output += "file\",\"size\":\"";
            output += file.size();
        }

        output += "\",\"name\":\"";
        // Always return names without leading "/"
        if (file.name()[0] == '/') {
            output += &(file.name()[1]);
            name = file.name();  // name in map should contain leading "/"
        }
        else {
            output += file.name();
            name = '/' + file.name();  // name in map should contain leading "/"
        }

        output += "\"}";

        files_data.insert(std::make_pair(name, output));
        output.clear();
    }

    // Sort file list before building output.
    // NOTE: SPIFFS doesn't support directories! Path <directory_name/filename> can exist, but directory, as entity,
    // not. Directories are used just as part of path.
    output = '[';
    for (auto const& data_pair : files_data) {
        output += data_pair.second + ',';
    }
    output.remove(output.length() - 1);
    output += ']';

    web_server_.sendContent(output);
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
    if (check_for_unsupported_path(path).length() > 0) {
        return reply_server_error("INVALID FILENAME");
    }
    if (path == "/") {
        return reply_bad_request("BAD PATH");
    }
    if (SPIFFS.exists(path)) {
        return reply_bad_request("FILE EXISTS");
    }

    String src = web_server_.arg("src");
    if (src.isEmpty()) {
        // No source specified: creation
        DEBUG_PRINTLN(String{"handle_file_create: "} + path);
        if (path.endsWith("/")) {
            // Create a folder
            path.remove(path.length() - 1);
            if (!SPIFFS.mkdir(path)) {
                return reply_server_error("MKDIR FAILED");
            }
        }
        else {
            // Create a file
            fs::File file = SPIFFS.open(path, "w");
            if (file) {
                file.write((const uint8_t*)0, 0);
                file.close();
            }
            else {
                return reply_server_error("CREATE FAILED");
            }
        }
        if (path.lastIndexOf('/') > -1) {
            path = path.substring(0, path.lastIndexOf('/'));
        }
        reply_ok_with_msg(path);
    }
    else {
        // Source specified: rename
        if (src == "/") {
            return reply_bad_request("BAD SRC");
        }
        if (!SPIFFS.exists(src)) {
            return reply_bad_request("SRC FILE NOT FOUND");
        }

        DEBUG_PRINTLN(String{"handle_file_create: "} + path + " from " + src);

        if (path.endsWith("/")) {
            path.remove(path.length() - 1);
        }
        if (src.endsWith("/")) {
            src.remove(src.length() - 1);
        }
        if (!SPIFFS.rename(src, path)) {
            return reply_server_error("RENAME FAILED");
        }
        reply_ok_with_msg(last_existing_parent(src));
    }
}

// Delete the file or folder designed by the given path.
// If it's a file, delete it.
// If it's a folder, delete all nested contents first then the folder itself

// IMPORTANT NOTE: using recursion is generally not recommended on embedded devices and can lead to crashes
// (stack overflow errors). It might crash in case of deeply nested filesystems.
// Please don't do this on a production system.
void
SadLampWebServer::delete_recursive(String const& path)
{
    fs::File file  = SPIFFS.open(path, "r");
    bool     isDir = file.isDirectory();
    file.close();

    // If it's a plain file, delete it
    if (!isDir) {
        SPIFFS.remove(path);
        return;
    }

    // Otherwise delete its contents first
    fs::File dir = SPIFFS.open(path);
    while (File file = dir.openNextFile()) {
        delete_recursive(path + '/' + file.name());
    }

    // Then delete the folder itself
    SPIFFS.rmdir(path);
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
    if (path.isEmpty() || path == "/") {
        return reply_bad_request("BAD PATH");
    }

    DEBUG_PRINTLN(String{"handle_file_delete: "} + path);

    if (!SPIFFS.exists(path)) {
        return reply_not_found(FILE_NOT_FOUND);
    }
    delete_recursive(path);

    reply_ok_with_msg(last_existing_parent(path));
}

void
SadLampWebServer::handle_file_upload()
{
    if (web_server_.uri() != "/edit") {
        return;
    }

    HTTPUpload& upload = web_server_.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        // Make sure paths always start with "/"
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        DEBUG_PRINTLN(String{"handle_file_upload Name: "} + filename);
        upload_file_ = SPIFFS.open(filename, "w");
        if (!upload_file_) {
            return reply_server_error("CREATE FAILED");
        }
        DEBUG_PRINTLN(String{"Upload: START, filename: "} + filename);
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (upload_file_) {
            size_t bytesWritten = upload_file_.write(upload.buf, upload.currentSize);
            if (bytesWritten != upload.currentSize) {
                return reply_server_error("WRITE FAILED");
            }
        }
        DEBUG_PRINTLN(String{"Upload: WRITE, Bytes: "} + String(upload.currentSize));
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (upload_file_) {
            upload_file_.close();
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

    String contentType;
    if (web_server_.hasArg("download")) {
        contentType = "application/octet-stream";
    }
    else {
        contentType = getContentType(path);
    }

    if (!SPIFFS.exists(path)) {
        // File not found, try gzip version
        path = path + ".gz";
    }
    if (SPIFFS.exists(path)) {
        fs::File file = SPIFFS.open(path, "r");
        if (web_server_.streamFile(file, contentType) != file.size()) {
            DEBUG_PRINTLN("Sent less data than expected!");
        }
        file.close();
        return true;
    }

    return false;
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

    HTTPUpload& upload = web_server_.upload();
    if (upload.status == UPLOAD_FILE_START) {
        // TODO(migration to ESP32): use new code for update from examples for ESP32 (+ adaptions). Test it!
        auto file_size = web_server_.arg("file_size").toInt();
        if (upload.name == "filesystem") {
            if (!Update.begin(file_size, U_SPIFFS)) {
                Update.end();
                Update.printError(DGB_STREAM);
                esp_firmware_upload_error_ =
                    "ERROR: not enough space on SPIFFS (upload.name == 'filesystem')! Available: " +
                    String(SPIFFS.totalBytes());
                return;
            }
        }
        else {
            if (!Update.begin(file_size, U_FLASH)) {  // start with max available size
                Update.end();
                Update.printError(DGB_STREAM);
                esp_firmware_upload_error_ =
                    "ERROR: not enough space on U_FLASH (upload.name != 'filesystem')! Available: " +
                    String(ESP.getFreeSketchSpace());
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
