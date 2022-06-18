#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct git_repository;

namespace libellus {

using Oid = std::array<unsigned char, 20>;

struct File {
    bool is_blob;
    std::string name;
    Oid oid;
};

class Repository {
public:
    Repository(const std::string& repo_path, std::string_view refname = "refs/heads/master");
    ~Repository();

    std::optional<std::vector<File>> get_listing(const std::string& path);

    void commit(std::string_view commit_message, const std::string& path, std::string_view contents);

    std::string read_blob(Oid oid);

private:
    void check_error(int err);

    git_repository* repo = nullptr;
    std::string refname;
};

}  // namespace libellus
