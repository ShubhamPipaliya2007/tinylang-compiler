// TinyLang Standard Library — Directory operations (Phase 1)
// Import: import "stdlib/Dir.tl";
// Requires: import "stdlib/Vec.tl";  (for Dir.list() return type)

class Dir {
    string path;

    ComeAndDo init(string dirPath) {
        this.path = dirPath;
    }

    // Returns 1 if the directory exists, 0 otherwise.
    ComeAndDo exists() {
        return __tl_dir_exists(this.path);
    }

    // Create directory (and any missing parents).  Returns 1 on success.
    ComeAndDo create() {
        return __tl_dir_create(this.path);
    }

    // Delete directory (must be empty).  Returns 1 on success.
    ComeAndDo delete() {
        return __tl_dir_delete(this.path);
    }

    // Number of entries (excluding . and ..).
    ComeAndDo count() {
        return __tl_dir_list_count(this.path);
    }

    // Get entry at index (0-based).
    ComeAndDo entry(int idx) {
        return __tl_dir_list_entry(this.path, idx);
    }
}

// ── Convenience free functions ────────────────────────────────────────────

ComeAndDo dirExists(string path) {
    return __tl_dir_exists(path);
}

ComeAndDo dirCreate(string path) {
    return __tl_dir_create(path);
}

ComeAndDo dirDelete(string path) {
    return __tl_dir_delete(path);
}

ComeAndDo dirCount(string path) {
    return __tl_dir_list_count(path);
}

ComeAndDo dirEntry(string path, int idx) {
    return __tl_dir_list_entry(path, idx);
}
