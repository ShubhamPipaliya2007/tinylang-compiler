// TinyLang Standard Library — File I/O (Phase 1)
// Import: import "stdlib/File.tl";

class File {
    string path;

    ComeAndDo init(string filePath) {
        this.path = filePath;
    }

    // Returns 1 if the file exists, 0 otherwise.
    ComeAndDo exists() {
        return __tl_file_exists(this.path);
    }

    // Read the entire file as a string.  Returns "" on error.
    ComeAndDo readAll() {
        return __tl_file_read_all(this.path);
    }

    // Overwrite the file with content.  Returns 1 on success, 0 on error.
    ComeAndDo write(string content) {
        return __tl_file_write_all(this.path, content);
    }

    // Append content to the file.  Returns 1 on success, 0 on error.
    ComeAndDo append(string content) {
        return __tl_file_append(this.path, content);
    }

    // Delete the file.  Returns 1 on success, 0 on error.
    ComeAndDo delete() {
        return __tl_file_delete(this.path);
    }

    // Write a single line (appends newline).
    ComeAndDo writeLine(string line) {
        return __tl_file_write_all(this.path, line + "\n");
    }

    // Append a single line.
    ComeAndDo appendLine(string line) {
        return __tl_file_append(this.path, line + "\n");
    }
}

// ── Convenience free functions ────────────────────────────────────────────

ComeAndDo fileExists(string path) {
    return __tl_file_exists(path);
}

ComeAndDo fileRead(string path) {
    return __tl_file_read_all(path);
}

ComeAndDo fileWrite(string path, string content) {
    return __tl_file_write_all(path, content);
}

ComeAndDo fileAppend(string path, string content) {
    return __tl_file_append(path, content);
}

ComeAndDo fileDelete(string path) {
    return __tl_file_delete(path);
}
