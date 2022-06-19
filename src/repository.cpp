#include "repository.hpp"

#include <fmt/format.h>
#include <git2.h>
#include <mcl/assert.hpp>
#include <mcl/scope_exit.hpp>

namespace libellus {

Oid::Oid() = default;

Oid::Oid(const git_oid* g)
{
    std::memcpy(oid.data(), g->id, oid.size());
}

std::string Oid::to_string() const
{
    return fmt::format(
        "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}"
        "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        oid[0], oid[1], oid[2], oid[3], oid[4], oid[5], oid[6], oid[7], oid[8], oid[9],
        oid[10], oid[11], oid[12], oid[13], oid[14], oid[15], oid[16], oid[17], oid[18], oid[19]);
}

void Repository::check_error(int err) const
{
    ASSERT_MSG(!err, "libgit2 error: {}\n", git_error_last()->message);
}

std::string Repository::get_full_reference_name() const
{
    git_reference* ref;
    git_reference_dwim(&ref, repo, refname.c_str());
    SCOPE_EXIT { git_reference_free(ref); };

    return git_reference_name(ref);
}

git_commit* Repository::get_current_commit() const
{
    git_reference* ref;
    git_reference_dwim(&ref, repo, refname.c_str());
    SCOPE_EXIT { git_reference_free(ref); };

    git_reference* resolved_ref;
    git_reference_resolve(&resolved_ref, ref);
    SCOPE_EXIT { git_reference_free(resolved_ref); };

    const git_oid* ref_target = git_reference_target(resolved_ref);
    git_commit* result;
    git_commit_lookup(&result, repo, ref_target);
    return result;
}

git_tree* Repository::get_current_tree() const
{
    git_commit* commit = get_current_commit();
    SCOPE_EXIT { git_commit_free(commit); };

    git_tree* result;
    git_commit_tree(&result, commit);
    return result;
}

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

std::optional<std::vector<File>> Repository::list(std::string path) const
{
    path.erase(0, path.find_first_not_of('/'));

    git_tree* root = get_current_tree();
    SCOPE_EXIT { git_tree_free(root); };

    git_tree* dir = nullptr;
    if (path.empty() || path == "/") {
        git_tree_dup(&dir, root);
    } else {
        const int err = git_object_lookup_bypath((git_object**)&dir, (const git_object*)root, path.c_str(), GIT_OBJ_TREE);
        if (err == GIT_ENOTFOUND) {
            git_tree_free(dir);
            return {};
        }
        check_error(err);
    }
    SCOPE_EXIT { git_tree_free(dir); };

    std::vector<File> files;
    const size_t sz = git_tree_entrycount(dir);
    for (size_t i = 0; i < sz; ++i) {
        const git_tree_entry* te = git_tree_entry_byindex(dir, i);

        files.emplace_back(File{
            .is_blob = git_tree_entry_type(te) == GIT_OBJECT_BLOB,
            .oid = git_tree_entry_id(te),
            .name = git_tree_entry_name(te),
        });
    }
    return files;
}

void Repository::commit(const std::string& commit_message, std::string path, std::string_view contents)
{
    path.erase(0, path.find_first_not_of('/'));
    ASSERT(!path.empty());

    git_tree* old_root = get_current_tree();
    SCOPE_EXIT { git_tree_free(old_root); };

    git_commit* old_commit = get_current_commit();
    SCOPE_EXIT { git_commit_free(old_commit); };

    git_oid blob_oid;
    git_blob_create_frombuffer(&blob_oid, repo, contents.data(), contents.size());

    git_tree_update update{
        GIT_TREE_UPDATE_UPSERT,
        blob_oid,
        GIT_FILEMODE_BLOB,
        path.c_str(),
    };

    git_oid new_tree_oid;
    git_tree_create_updated(&new_tree_oid, repo, old_root, 1, &update);
    git_tree* new_root;
    git_tree_lookup(&new_root, repo, &new_tree_oid);
    SCOPE_EXIT { git_tree_free(new_root); };

    git_signature* sig;
    git_signature_now(&sig, "libellus", "libellus@mary.rs");
    SCOPE_EXIT { git_signature_free(sig); };

    const std::string update_ref = get_full_reference_name();
    const git_commit* parents[1]{old_commit};
    git_oid new_commit_oid;
    git_commit_create(&new_commit_oid, repo, update_ref.c_str(), sig, sig, "UTF-8", commit_message.c_str(), new_root, 1, parents);
}

std::optional<std::string> Repository::read(std::string path) const
{
    path.erase(0, path.find_first_not_of('/'));
    ASSERT(!path.empty());

    git_tree* root = get_current_tree();
    SCOPE_EXIT { git_tree_free(root); };

    git_blob* blob = nullptr;
    const int err = git_object_lookup_bypath((git_object**)&blob, (const git_object*)root, path.c_str(), GIT_OBJ_BLOB);
    if (err == GIT_ENOTFOUND) {
        git_blob_free(blob);
        return {};
    }
    check_error(err);
    SCOPE_EXIT { git_blob_free(blob); };

    const size_t size = git_blob_rawsize(blob);
    std::string result;
    result.resize(size);
    std::memcpy(result.data(), git_blob_rawcontent(blob), size);
    return result;
}

}  // namespace libellus
