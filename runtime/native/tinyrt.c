/* TinyLang native runtime — linked with every compiled TinyLang program.
 *
 * Compile with:  clang -c tinyrt.c -o tinyrt.o
 * Then link  :  clang program.ll tinyrt.o -o program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>

/* ── Print ─────────────────────────────────────────────────────────────── */

void __tl_print_i32(int32_t v) { printf("%d\n", v); }

void __tl_print_f64(double v) {
    /* Match TIRVM output: integer-valued floats print without decimal point */
    long long iv = (long long)v;
    if ((double)iv == v && v >= -1e15 && v <= 1e15)
        printf("%lld\n", iv);
    else
        printf("%g\n", v);
}

void __tl_print_bool(int32_t v) { printf("%s\n", v ? "true" : "false"); }
void __tl_print_char(int32_t v) { printf("%c\n", (char)v); }
void __tl_print_str(char* v)    { printf("%s\n", v ? v : ""); }

/* ── Input ──────────────────────────────────────────────────────────────── */

int32_t __tl_input_i32(void) {
    int32_t v = 0;
    scanf("%d", &v);
    return v;
}

/* ── String operations ──────────────────────────────────────────────────── */

char* __tl_str_concat(char* a, char* b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char* r = (char*)malloc(la + lb + 1);
    if (!r) { fprintf(stderr, "tinyrt: out of memory\n"); exit(1); }
    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}

char* __tl_i32_to_str(int32_t v) {
    char buf[32]; snprintf(buf, sizeof(buf), "%d", v); return strdup(buf);
}

char* __tl_f64_to_str(double v) {
    char buf[64]; snprintf(buf, sizeof(buf), "%g", v); return strdup(buf);
}

char* __tl_bool_to_str(int32_t v) { return strdup(v ? "true" : "false"); }

int32_t __tl_str_eq(char* a, char* b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

int32_t __tl_str_len(char* s) { return s ? (int32_t)strlen(s) : 0; }

/* ── String extended operations ─────────────────────────────────────────── */

char* __tl_str_sub(char* s, int32_t start, int32_t len) {
    if (!s) return strdup("");
    int32_t slen = (int32_t)strlen(s);
    if (start < 0) start = 0;
    if (start >= slen) return strdup("");
    if (len < 0 || start + len > slen) len = slen - start;
    char* r = (char*)malloc(len + 1);
    if (!r) return strdup("");
    memcpy(r, s + start, len);
    r[len] = '\0';
    return r;
}

char* __tl_str_upper(char* s) {
    if (!s) return strdup("");
    char* r = strdup(s);
    for (char* p = r; *p; p++) *p = (char)toupper((unsigned char)*p);
    return r;
}

char* __tl_str_lower(char* s) {
    if (!s) return strdup("");
    char* r = strdup(s);
    for (char* p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}

char* __tl_str_trim(char* s) {
    if (!s) return strdup("");
    const char* l = s;
    while (*l && isspace((unsigned char)*l)) l++;
    const char* r = s + strlen(s) - 1;
    while (r > l && isspace((unsigned char)*r)) r--;
    size_t len = r - l + 1;
    if (r < l) return strdup("");
    char* out = (char*)malloc(len + 1);
    memcpy(out, l, len); out[len] = '\0';
    return out;
}

int32_t __tl_str_contains(char* s, char* sub) {
    if (!s || !sub) return 0;
    return strstr(s, sub) != NULL ? 1 : 0;
}

int32_t __tl_str_starts_with(char* s, char* prefix) {
    if (!s || !prefix) return 0;
    size_t pl = strlen(prefix);
    return strncmp(s, prefix, pl) == 0 ? 1 : 0;
}

int32_t __tl_str_ends_with(char* s, char* suffix) {
    if (!s || !suffix) return 0;
    size_t sl = strlen(s), pl = strlen(suffix);
    if (sl < pl) return 0;
    return strncmp(s + sl - pl, suffix, pl) == 0 ? 1 : 0;
}

int32_t __tl_str_index_of(char* s, char* sub) {
    if (!s || !sub) return -1;
    char* p = strstr(s, sub);
    return p ? (int32_t)(p - s) : -1;
}

char* __tl_str_replace(char* s, char* from, char* to) {
    if (!s) return strdup("");
    if (!from || !*from) return strdup(s);
    if (!to) to = "";
    size_t flen = strlen(from), tlen = strlen(to);
    /* Count occurrences */
    size_t count = 0;
    const char* p = s;
    while ((p = strstr(p, from)) != NULL) { count++; p += flen; }
    /* Allocate result */
    size_t slen = strlen(s);
    size_t rlen = slen + count * (tlen > flen ? tlen - flen : 0) + 1;
    char* r = (char*)malloc(rlen);
    char* w = r;
    const char* cur = s;
    while (*cur) {
        if (strncmp(cur, from, flen) == 0) {
            memcpy(w, to, tlen); w += tlen; cur += flen;
        } else { *w++ = *cur++; }
    }
    *w = '\0';
    return r;
}

int32_t __tl_str_to_int(char* s)    { return s ? (int32_t)atoi(s) : 0; }
double  __tl_str_to_float(char* s)  { return s ? atof(s)           : 0.0; }
int32_t __tl_str_char_at(char* s, int32_t i) {
    if (!s || i < 0 || i >= (int32_t)strlen(s)) return 0;
    return (int32_t)(unsigned char)s[i];
}

/* ── File operations ────────────────────────────────────────────────────── */

int32_t __tl_file_exists(char* path) {
    if (!path) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

char* __tl_file_read_all(char* path) {
    if (!path) return strdup("");
    FILE* f = fopen(path, "r");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char* buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return strdup(""); }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

int32_t __tl_file_write_all(char* path, char* content) {
    if (!path || !content) return 0;
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

int32_t __tl_file_append(char* path, char* content) {
    if (!path || !content) return 0;
    FILE* f = fopen(path, "a");
    if (!f) return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

int32_t __tl_file_delete(char* path) {
    return path && remove(path) == 0 ? 1 : 0;
}

/* ── Directory operations ─────────────────────────────────────────────────── */

int32_t __tl_dir_exists(char* path) {
    if (!path) return 0;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

int32_t __tl_dir_create(char* path) {
    if (!path) return 0;
    /* Create all missing components (like mkdir -p). */
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 ? 1 : (__tl_dir_exists(path) ? 1 : 0);
}

int32_t __tl_dir_delete(char* path) {
    if (!path) return 0;
    return rmdir(path) == 0 ? 1 : 0;
}

int32_t __tl_dir_list_count(char* path) {
    if (!path) return 0;
    DIR* d = opendir(path);
    if (!d) return 0;
    int32_t count = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* skip . and .. */
        count++;
    }
    closedir(d);
    return count;
}

char* __tl_dir_list_entry(char* path, int32_t idx) {
    if (!path) return "";
    DIR* d = opendir(path);
    if (!d) return "";
    int32_t i = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (i == idx) {
            closedir(d);
            char* s = strdup(entry->d_name);
            return s ? s : "";
        }
        i++;
    }
    closedir(d);
    return "";
}

/* ── Array resize ────────────────────────────────────────────────────────── */

void* __tl_arr_resize(void* arr, int32_t new_cap) {
    /* Realloc the element area preserving the header.
     * This invalidates the old pointer — callers must update their reference. */
    if (!arr) return __tl_alloc_arr(new_cap);
    TLArrHeader* h = (TLArrHeader*)arr;
    int32_t old_cap = (int32_t)h->length;
    size_t new_sz = sizeof(TLArrHeader) + (size_t)new_cap * 8;
    TLArrHeader* new_arr = (TLArrHeader*)realloc(arr, new_sz);
    if (!new_arr) return arr; /* keep old on failure */
    /* Zero the new slots */
    if (new_cap > old_cap) {
        uint64_t* elems = (uint64_t*)(new_arr + 1);
        memset(elems + old_cap, 0, (size_t)(new_cap - old_cap) * 8);
    }
    new_arr->length = new_cap;
    return new_arr;
}

/* __tl_load_arr / __tl_store_arr / __tl_alloc_arr already defined above */

/* ── Object layout ──────────────────────────────────────────────────────── */
/*
 * Phase 4.5 native object layout (matches abi.md §4.2):
 *
 *   offset  0 : gcWord    (8 bytes)
 *   offset  8 : vtable*   (8 bytes, currently NULL)
 *   offset 16 : field[0]  (8 bytes each, raw 64-bit value)
 *   offset 24 : field[1]
 *   ...
 */

typedef struct TLObjHeader {
    uint64_t gcWord;
    void*    vtable;
} TLObjHeader;

void* __tl_alloc_obj(int32_t numFields) {
    size_t sz = sizeof(TLObjHeader) + (size_t)numFields * 8;
    void* p = calloc(1, sz);
    if (!p) { fprintf(stderr, "tinyrt: out of memory\n"); exit(1); }
    return p;
}

uint64_t __tl_load_field(void* obj, int32_t idx) {
    if (!obj) return 0;
    uint64_t* fields = (uint64_t*)((char*)obj + sizeof(TLObjHeader));
    return fields[idx];
}

void __tl_store_field(void* obj, int32_t idx, uint64_t val) {
    if (!obj) return;
    uint64_t* fields = (uint64_t*)((char*)obj + sizeof(TLObjHeader));
    fields[idx] = val;
}

/* ── Array layout ───────────────────────────────────────────────────────── */
/*
 *   offset  0 : gcWord   (8 bytes)
 *   offset  8 : length   (8 bytes)
 *   offset 16 : element[0] (8 bytes each)
 */

typedef struct {
    uint64_t gcWord;
    int64_t  length;
} TLArrHeader;

void* __tl_alloc_arr(int32_t size) {
    if (size < 0) size = 0;
    size_t sz = sizeof(TLArrHeader) + (size_t)size * 8;
    void* p = calloc(1, sz);
    if (!p) { fprintf(stderr, "tinyrt: out of memory\n"); exit(1); }
    ((TLArrHeader*)p)->length = size;
    return p;
}

int32_t __tl_arr_len(void* arr) {
    if (!arr) return 0;
    return (int32_t)((TLArrHeader*)arr)->length;
}

uint64_t __tl_load_arr(void* arr, int32_t idx) {
    if (!arr) { fprintf(stderr, "tinyrt: null array\n"); exit(1); }
    TLArrHeader* h = (TLArrHeader*)arr;
    if (idx < 0 || (int64_t)idx >= h->length) {
        fprintf(stderr, "tinyrt: array index %d out of bounds (len=%lld)\n",
                idx, (long long)h->length);
        exit(1);
    }
    uint64_t* elems = (uint64_t*)((char*)arr + sizeof(TLArrHeader));
    return elems[idx];
}

void __tl_store_arr(void* arr, int32_t idx, uint64_t val) {
    if (!arr) { fprintf(stderr, "tinyrt: null array\n"); exit(1); }
    TLArrHeader* h = (TLArrHeader*)arr;
    if (idx < 0 || (int64_t)idx >= h->length) {
        fprintf(stderr, "tinyrt: array index %d out of bounds (len=%lld)\n",
                idx, (long long)h->length);
        exit(1);
    }
    uint64_t* elems = (uint64_t*)((char*)arr + sizeof(TLArrHeader));
    elems[idx] = val;
}
