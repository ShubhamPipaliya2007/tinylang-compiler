// TinyLang Standard Library — Vec (dynamic array) (Phase 1)
// Import: import "stdlib/Vec.tl";
//
// IntVec  — dynamic array of int values.
// StrVec  — dynamic array of string values.
//
// Both classes use __tl_alloc_arr / __tl_load_arr / __tl_store_arr /
// __tl_arr_resize for their backing store, so they run on the interpreter
// path; the native (--emit-llvm) path works for programs that only use
// IntVec with integer values.

class IntVec {
    int buf[];    // backing array (TLArray*)
    int size;
    int cap;

    ComeAndDo init(int capacity) {
        this.buf = __tl_alloc_arr(capacity);
        this.size = 0;
        this.cap = capacity;
    }

    ComeAndDo push(int val) {
        if (this.size == this.cap) {
            this.cap = this.cap * 2;
            this.buf = __tl_arr_resize(this.buf, this.cap);
        }
        __tl_store_arr(this.buf, this.size, val);
        this.size = this.size + 1;
    }

    ComeAndDo pop() {
        if (this.size > 0) {
            this.size = this.size - 1;
        }
        return __tl_load_arr(this.buf, this.size);
    }

    ComeAndDo get(int idx) {
        return __tl_load_arr(this.buf, idx);
    }

    ComeAndDo set(int idx, int val) {
        __tl_store_arr(this.buf, idx, val);
    }

    ComeAndDo len() {
        return this.size;
    }

    ComeAndDo cap() {
        return this.cap;
    }

    ComeAndDo isEmpty() {
        if (this.size == 0) { return 1; }
        return 0;
    }

    ComeAndDo clear() {
        this.size = 0;
    }

    ComeAndDo contains(int val) {
        int i = 0;
        while (i < this.size) {
            if (__tl_load_arr(this.buf, i) == val) { return 1; }
            i = i + 1;
        }
        return 0;
    }

    ComeAndDo indexOf(int val) {
        int i = 0;
        while (i < this.size) {
            if (__tl_load_arr(this.buf, i) == val) { return i; }
            i = i + 1;
        }
        return -1;
    }

    ComeAndDo sum() {
        int total = 0;
        int i = 0;
        while (i < this.size) {
            total = total + __tl_load_arr(this.buf, i);
            i = i + 1;
        }
        return total;
    }

    ComeAndDo min() {
        if (this.size == 0) { return 0; }
        int m = __tl_load_arr(this.buf, 0);
        int i = 1;
        while (i < this.size) {
            int v = __tl_load_arr(this.buf, i);
            if (v < m) { m = v; }
            i = i + 1;
        }
        return m;
    }

    ComeAndDo max() {
        if (this.size == 0) { return 0; }
        int m = __tl_load_arr(this.buf, 0);
        int i = 1;
        while (i < this.size) {
            int v = __tl_load_arr(this.buf, i);
            if (v > m) { m = v; }
            i = i + 1;
        }
        return m;
    }

    ComeAndDo reverse() {
        int left = 0;
        int right = this.size - 1;
        while (left < right) {
            int tmp = __tl_load_arr(this.buf, left);
            __tl_store_arr(this.buf, left, __tl_load_arr(this.buf, right));
            __tl_store_arr(this.buf, right, tmp);
            left = left + 1;
            right = right - 1;
        }
    }
}

class StrVec {
    int buf[];
    int size;
    int cap;

    ComeAndDo init(int capacity) {
        this.buf = __tl_alloc_arr(capacity);
        this.size = 0;
        this.cap = capacity;
    }

    ComeAndDo push(string val) {
        if (this.size == this.cap) {
            this.cap = this.cap * 2;
            this.buf = __tl_arr_resize(this.buf, this.cap);
        }
        __tl_store_arr(this.buf, this.size, val);
        this.size = this.size + 1;
    }

    ComeAndDo get(int idx) {
        return __tl_load_arr(this.buf, idx);
    }

    ComeAndDo set(int idx, string val) {
        __tl_store_arr(this.buf, idx, val);
    }

    ComeAndDo len() {
        return this.size;
    }

    ComeAndDo isEmpty() {
        if (this.size == 0) { return 1; }
        return 0;
    }

    ComeAndDo clear() {
        this.size = 0;
    }

    ComeAndDo contains(string val) {
        int i = 0;
        while (i < this.size) {
            if (__tl_str_eq(__tl_load_arr(this.buf, i), val) == 1) { return 1; }
            i = i + 1;
        }
        return 0;
    }

    ComeAndDo indexOf(string val) {
        int i = 0;
        while (i < this.size) {
            if (__tl_str_eq(__tl_load_arr(this.buf, i), val) == 1) { return i; }
            i = i + 1;
        }
        return -1;
    }

    ComeAndDo join(string sep) {
        string result = "";
        int i = 0;
        while (i < this.size) {
            if (i > 0) { result = result + sep; }
            result = result + __tl_load_arr(this.buf, i);
            i = i + 1;
        }
        return result;
    }
}
