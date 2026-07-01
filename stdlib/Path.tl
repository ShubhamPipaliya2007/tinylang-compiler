// TinyLang Standard Library — Path utilities (Phase 1)
// Import: import "stdlib/Path.tl";
// Requires: import "stdlib/String.tl";

// Return the directory portion of a path.
// "a/b/c.txt" → "a/b"   "/foo" → "/"   "file.txt" → "."
ComeAndDo pathDir(string p) {
    int idx = __tl_str_index_of(p, "/");
    int last = -1;
    int i = 0;
    int plen = __tl_str_len(p);
    while (i < plen) {
        string ch = __tl_str_sub(p, i, 1);
        if (__tl_str_eq(ch, "/") == 1) { last = i; }
        i = i + 1;
    }
    if (last < 0) { return "."; }
    if (last == 0) { return "/"; }
    return __tl_str_sub(p, 0, last);
}

// Return the filename (last component) of a path.
// "a/b/c.txt" → "c.txt"   "file.txt" → "file.txt"
ComeAndDo pathBase(string p) {
    int last = -1;
    int i = 0;
    int plen = __tl_str_len(p);
    while (i < plen) {
        string ch = __tl_str_sub(p, i, 1);
        if (__tl_str_eq(ch, "/") == 1) { last = i; }
        i = i + 1;
    }
    if (last < 0) { return p; }
    return __tl_str_sub(p, last + 1, plen - last - 1);
}

// Return the file extension (including the dot).
// "file.txt" → ".txt"   "file" → ""   ".hidden" → ".hidden"
ComeAndDo pathExt(string p) {
    string base = pathBase(p);
    int last = -1;
    int i = 0;
    int blen = __tl_str_len(base);
    while (i < blen) {
        string ch = __tl_str_sub(base, i, 1);
        if (__tl_str_eq(ch, ".") == 1) { last = i; }
        i = i + 1;
    }
    if (last < 1) { return ""; }
    return __tl_str_sub(base, last, blen - last);
}

// Return the filename without extension.
// "file.txt" → "file"   "archive.tar.gz" → "archive.tar"
ComeAndDo pathStem(string p) {
    string base = pathBase(p);
    int last = -1;
    int i = 0;
    int blen = __tl_str_len(base);
    while (i < blen) {
        string ch = __tl_str_sub(base, i, 1);
        if (__tl_str_eq(ch, ".") == 1) { last = i; }
        i = i + 1;
    }
    if (last < 1) { return base; }
    return __tl_str_sub(base, 0, last);
}

// Join two path components with a "/".
// pathJoin("a/b", "c.txt") → "a/b/c.txt"
ComeAndDo pathJoin(string dir, string name) {
    if (__tl_str_len(dir) == 0) { return name; }
    if (__tl_str_len(name) == 0) { return dir; }
    string last = __tl_str_sub(dir, __tl_str_len(dir) - 1, 1);
    if (__tl_str_eq(last, "/") == 1) { return dir + name; }
    return dir + "/" + name;
}

// Return 1 if path is absolute (starts with "/").
ComeAndDo pathIsAbsolute(string p) {
    return __tl_str_starts_with(p, "/");
}

// Replace extension in a path.
// pathWithExt("file.txt", ".md") → "file.md"
ComeAndDo pathWithExt(string p, string ext) {
    string dir = pathDir(p);
    string stem = pathStem(pathBase(p));
    if (__tl_str_eq(dir, ".") == 1) { return stem + ext; }
    return dir + "/" + stem + ext;
}
