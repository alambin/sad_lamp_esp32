#include "FS.h"

#include <dirent.h>
#include <sys/stat.h>

#include <map>

#include <SPIFFS.h>
#include "src/Utils/Logger.h"

namespace
{
const String kSpiffsMountingPoint{"/spiffs"};  // Check it in SPIFFS.h

bool
is_real_SPIFFS_dir(String const& dir_path)
{
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        return false;
    }
    // Looks like directory. Try to read it
    struct dirent* dir_entry = readdir(dir);  // Should not be deallocated
    if (!dir_entry) {
        // Non-existing item. On SPIFSS such items are reported as directories!
        closedir(dir);
        return false;
    }
    closedir(dir);
    return true;
}

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

String
last_existing_parent(String path)
{
    while (!path.isEmpty() && !Utils::FS::exists(path)) {
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

// Delete the file or folder designed by the given path.
// If it's a file, delete it.
// If it's a folder, delete all nested contents first then the folder itself

// IMPORTANT NOTE: using recursion is generally not recommended on embedded devices and can lead to crashes
// (stack overflow errors). It might crash in case of deeply nested filesystems.
// Please don't do this on a production system.
void
delete_recursive(String const& path)
{
    // If it's a plain file, delete it
    if (!Utils::FS::is_directory(path)) {
        SPIFFS.remove(path);
        return;
    }

    // Otherwise delete its contents first
    File dir = Utils::FS::open(path, Utils::FS::OpenMode::kRead);
    while (File file = dir.openNextFile()) {
        delete_recursive(path + '/' + file.name());
    }

    // Then delete the folder itself
    SPIFFS.rmdir(path);
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
}  // namespace

namespace Utils
{
bool
FS::exists(String const& path)
{
    if (path == "/") {
        return true;
    }
    return (get_entry_type(path) != FS::EntryType::kUnknown);
}

String
FS::get_file_list_json(String const& path)
{
    File root = open(path, OpenMode::kRead);

    String                                            name;  // Use the same string for every line
    std::map<String, String, decltype(filename_less)> files_data(filename_less);
    String                                            output;
    output.reserve(64);
    while (File file = root.openNextFile()) {
        String error{check_for_unsupported_path(file.name())};
        if (!error.isEmpty()) {
            DEBUG_PRINTLN(String{"Ignoring "} + file.name() + ": " + error);
            continue;
        }

        output += "{\"type\":\"";
        if (FS::is_directory(file.name())) {
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

        files_data.insert({name, output});
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
    return output;
}

std::pair<File, String>
FS::create_file(String path)
{
    // Make sure paths always start with "/"
    if (!path.startsWith("/")) {
        path = "/" + path;
    }

    if (!check_for_unsupported_path(path).isEmpty()) {
        return {File{}, "INVALID FILENAME"};
    }

    File file = open(path, OpenMode::kWrite);
    if (!file) {
        return {file, "CREATE FILE FAILED"};
    }
    file.write((const uint8_t*)0, 0);

    if (path.lastIndexOf('/') > -1) {
        path = path.substring(0, path.lastIndexOf('/'));
    }
    return {file, path};
}

std::pair<bool, String>
FS::create_folder(String path)
{
    // Doesn't work for SPIFFS on ESP32. SPIFFS.mkdir("/testdir") returns true, but folder is not created.
    // Most probably this is expected behavior, because SPIFFS doesn't support folders.
    if (check_for_unsupported_path(path) != "!TRAILING_SLASH! ") {
        return {false, "INVALID FOLDER NAME"};
    }

    path.remove(path.length() - 1);
    if (!SPIFFS.mkdir(path)) {
        return {false, "MKDIR FAILED"};
    }

    if (path.lastIndexOf('/') > -1) {
        path = path.substring(0, path.lastIndexOf('/'));
    }
    return {true, path};
}

bool
FS::close(File& file)
{
    file.close();
}

std::pair<bool, String>
FS::rename(String from, String to)
{
    if (to.endsWith("/")) {
        to.remove(to.length() - 1);
    }
    if (from.endsWith("/")) {
        from.remove(from.length() - 1);
    }
    if (!check_for_unsupported_path(to).isEmpty()) {
        return {false, "INVALID FINAL FILENAME"};
    }
    if (!check_for_unsupported_path(from).isEmpty()) {
        return {false, "INVALID SOURCE FILENAME"};
    }

    if (!SPIFFS.rename(from, to)) {
        return {false, "RENAME FAILED"};
    }

    // As some FS (e.g. LittleFS) delete the parent folder when the last child has been removed,
    // return the path of the closest parent still existing
    return {true, last_existing_parent(std::move(from))};
}

std::pair<bool, String>
FS::remove(String const& path)
{
    delete_recursive(path);
    return {true, last_existing_parent(path)};
}

File
FS::open(String const& path, OpenMode mode)
{
    return SPIFFS.open(path, (mode == OpenMode::kWrite) ? "w" : "r");
}

size_t
FS::write(File& file, uint8_t const* data, size_t size)
{
    return file.write(data, size);
}

size_t
FS::total_bytes()
{
    return SPIFFS.totalBytes();
}

size_t
FS::used_bytes()
{
    return SPIFFS.usedBytes();
}

bool
FS::is_directory(String const& path)
{
    // Do NOT use File::isDirectory(), because SPIFFS on ESP32 returns true for everything, which is not existing
    // file. Even for files, which don't exists - such files is also treated as directories!

    return (get_entry_type(path) == FS::EntryType::kDirectory);
}

bool
FS::is_file(String const& path)
{
    return (get_entry_type(path) == FS::EntryType::kFile);
}

FS::EntryType
FS::get_entry_type(String const& path)
{
    struct stat _stat;
    String      fullPath{kSpiffsMountingPoint + path};
    if (!stat(fullPath.c_str(), &_stat)) {
        // File found
        if (S_ISREG(_stat.st_mode)) {
            // Looks like regular file. But try to open it
            FILE* file = fopen(fullPath.c_str(), "r");
            if (!file) {
                return FS::EntryType::kUnknown;
            }
            fclose(file);
            return FS::EntryType::kFile;
        }
        else if (S_ISDIR(_stat.st_mode)) {
            // Directory or non-existing item (on SPIFFS)
            return is_real_SPIFFS_dir(fullPath) ? FS::EntryType::kDirectory : FS::EntryType::kUnknown;
        }
        else {
            // Unknown type
            return FS::EntryType::kUnknown;
        }
    }
    else {
        // File not found. Try to open as directory
        return is_real_SPIFFS_dir(fullPath) ? FS::EntryType::kDirectory : FS::EntryType::kUnknown;
    }
}

}  // namespace Utils
