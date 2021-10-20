#ifndef SRC_UTILS_FS_H_
#define SRC_UTILS_FS_H_

#include <utility>

#include <FS.h>
#include <WString.h>

namespace Utils
{
class FS
{
public:
    enum class OpenMode : uint8_t
    {
        kRead = 0,
        kWrite
    };
    enum class EntryType : uint8_t
    {
        kFile = 0,
        kDirectory,
        kUnknown
    };
    using File = fs::File;

    static bool                    exists(String const& path);
    static String                  get_file_list_json(String const& path);
    static std::pair<File, String> create_file(String path);
    static std::pair<bool, String> create_folder(String path);
    static bool                    close(File& file);
    static std::pair<bool, String> rename(String from, String to);
    static std::pair<bool, String> remove(String const& path);
    static File                    open(String const& path, OpenMode mode = OpenMode::kRead);
    static size_t                  write(File& file, uint8_t const* data, size_t size);
    static size_t                  total_bytes();
    static size_t                  used_bytes();
    static bool                    is_directory(String const& path);
    static bool                    is_file(String const& path);
    static EntryType               get_entry_type(String const& path);
};

}  // namespace Utils

#endif  // SRC_UTILS_FS_H_
