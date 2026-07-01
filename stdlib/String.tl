// TinyLang Standard Library — String utilities (Phase 1)
// Import: import "stdlib/String.tl";
// All functions operate on the built-in `string` type.

ComeAndDo strLen(string s) {
    return __tl_str_len(s);
}

ComeAndDo strSub(string s, int start, int len) {
    return __tl_str_sub(s, start, len);
}

ComeAndDo strUpper(string s) {
    return __tl_str_upper(s);
}

ComeAndDo strLower(string s) {
    return __tl_str_lower(s);
}

ComeAndDo strTrim(string s) {
    return __tl_str_trim(s);
}

ComeAndDo strContains(string s, string sub) {
    return __tl_str_contains(s, sub);
}

ComeAndDo strStartsWith(string s, string prefix) {
    return __tl_str_starts_with(s, prefix);
}

ComeAndDo strEndsWith(string s, string suffix) {
    return __tl_str_ends_with(s, suffix);
}

ComeAndDo strIndexOf(string s, string sub) {
    return __tl_str_index_of(s, sub);
}

ComeAndDo strReplace(string s, string from, string to) {
    return __tl_str_replace(s, from, to);
}

ComeAndDo strToInt(string s) {
    return __tl_str_to_int(s);
}

ComeAndDo strToFloat(string s) {
    return __tl_str_to_float(s);
}

ComeAndDo intToStr(int n) {
    return __tl_i32_to_str(n);
}

ComeAndDo floatToStr(float f) {
    return __tl_f64_to_str(f);
}

ComeAndDo boolToStr(bool b) {
    return __tl_bool_to_str(b);
}

ComeAndDo strEq(string a, string b) {
    return __tl_str_eq(a, b);
}

ComeAndDo strCharAt(string s, int i) {
    return __tl_str_char_at(s, i);
}

ComeAndDo strIsEmpty(string s) {
    if (__tl_str_len(s) == 0) { return 1; }
    return 0;
}

ComeAndDo strRepeat(string s, int n) {
    string result = "";
    int i = 0;
    while (i < n) {
        result = result + s;
        i = i + 1;
    }
    return result;
}

// Pad string on the left to at least width chars.
ComeAndDo strPadLeft(string s, int width, string pad) {
    string result = s;
    while (__tl_str_len(result) < width) {
        result = pad + result;
    }
    return result;
}

// Pad string on the right to at least width chars.
ComeAndDo strPadRight(string s, int width, string pad) {
    string result = s;
    while (__tl_str_len(result) < width) {
        result = result + pad;
    }
    return result;
}

// Count occurrences of sub in s.
ComeAndDo strCount(string s, string sub) {
    int count = 0;
    int idx = __tl_str_index_of(s, sub);
    int subLen = __tl_str_len(sub);
    while (idx != -1) {
        count = count + 1;
        s = __tl_str_sub(s, idx + subLen, __tl_str_len(s) - idx - subLen);
        idx = __tl_str_index_of(s, sub);
    }
    return count;
}
