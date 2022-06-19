#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct git_commit;
struct git_oid;
struct git_repository;
struct git_tree;

namespace libellus {

struct Oid {
    Oid();
    /* implicit */ Oid(const git_oid* g);

    std::array<unsigned char, 20> oid;

    operator const git_oid*() const { return (const git_oid*)oid.data(); }
    std::string to_string() const;
};

struct File {
    bool is_blob;
    std::string name;
    Oid oid;
};

class Repository {
public:
    Repository(const std::string& repo_path, std::string_view refname = "refs/heads/master");
    ~Repository();

    std::optional<std::vector<File>> list(std::string path) const;

    void commit(const std::string& commit_message, std::string path, std::string_view contents);

    std::optional<std::string> read(std::string path) const;

private:
    void check_error(int err) const;

    std::string get_full_reference_name() const;
    git_commit* get_current_commit() const;
    git_tree* get_current_tree() const;

    git_repository* repo = nullptr;
    std::string refname;
};

}  // namespace libellus
