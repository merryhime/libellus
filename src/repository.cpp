#include "repository.hpp"

#include <git2.h>
#include <mcl/assert.hpp>
#include <mcl/scope_exit.hpp>

namespace libellus {

Repository::Repository(const std::string& repo_path, std::string_view refname_)
    : refname(refname_)
{
    git_libgit2_init();
    check_error(git_repository_open(&repo, repo_path.c_str()));
}

Repository::~Repository()
{
    git_repository_free(repo);
    git_libgit2_shutdown();
}

std::optional<std::vector<File>> Repository::get_listing(const std::string& path)
{
    git_oid oid;
    check_error(git_reference_name_to_id(&oid, repo, refname.c_str()));

    git_commit* commit;
    check_error(git_commit_lookup(&commit, repo, &oid));
    SCOPE_EXIT { git_commit_free(commit); };

    git_tree* tree;
    check_error(git_commit_tree(&tree, commit));
    SCOPE_EXIT { git_tree_free(tree); };

    git_tree* dir;
    if (path == "" || path == "/") {
        git_tree_dup(&dir, tree);
    } else {
        git_tree_entry* dir_entry;
        check_error(git_tree_entry_bypath(&dir_entry, tree, path.c_str()));
        SCOPE_EXIT { git_tree_entry_free(dir_entry); };

        git_tree_lookup(&dir, repo, git_tree_entry_id(dir_entry));
    }
    SCOPE_EXIT { git_tree_free(dir); };

    std::vector<File> files;
    size_t sz = git_tree_entrycount(dir);
    for (size_t i = 0; i < sz; ++i) {
        const git_tree_entry* te = git_tree_entry_byindex(dir, i);

        File f;
        f.is_blob = git_tree_entry_type(te) == GIT_OBJECT_BLOB;
        f.name = git_tree_entry_name(te);
        std::memcpy(f.oid.data(), git_tree_entry_name(te), sizeof(Oid));

        files.emplace_back(f);
    }
    return files;
}

void Repository::commit(std::string_view commit_message, const std::string& path, std::string_view contents)
{
}

std::string Repository::read_blob(Oid oid)
{
}

void Repository::check_error(int err)
{
    ASSERT_MSG(!err, "libgit2 error: {}\n", git_error_last()->message);
}

}  // namespace libellus
