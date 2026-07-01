// TinyLang Standard Library — Map (hash map) (Phase 1)
// Import: import "stdlib/Map.tl";
//
// StrMap  — string-to-string map.  Capacity is fixed at construction time.
// IntMap  — string-to-int   map.
//
// Both use parallel key/value arrays with linear probing.
// Load factor target: fill at most cap/2 slots.

// ── StrMap ───────────────────────────────────────────────────────────────────

class StrMap {
    int keys[];    // array of string keys   (empty string = empty slot)
    int vals[];    // array of string values
    int used[];    // 1 = slot occupied, 0 = empty
    int size;
    int cap;

    ComeAndDo init(int capacity) {
        this.cap  = capacity;
        this.size = 0;
        this.keys = __tl_alloc_arr(capacity);
        this.vals = __tl_alloc_arr(capacity);
        this.used = __tl_alloc_arr(capacity);
    }

    // Internal: map key to a slot index using linear probing.
    ComeAndDo findSlot(string key) {
        int h = __tl_str_len(key);
        int i = 0;
        while (i < __tl_str_len(key)) {
            h = h + __tl_str_char_at(key, i);
            i = i + 1;
        }
        if (h < 0) { h = -h; }
        int slot = h - (h / this.cap) * this.cap;
        int probes = 0;
        while (probes < this.cap) {
            int occ = __tl_load_arr(this.used, slot);
            if (occ == 0) { return slot; }
            if (__tl_str_eq(__tl_load_arr(this.keys, slot), key) == 1) { return slot; }
            slot = slot + 1;
            if (slot == this.cap) { slot = 0; }
            probes = probes + 1;
        }
        return -1;
    }

    ComeAndDo put(string key, string val) {
        int slot = this.findSlot(key);
        if (slot < 0) { return 0; }
        int occ = __tl_load_arr(this.used, slot);
        if (occ == 0) { this.size = this.size + 1; }
        __tl_store_arr(this.keys, slot, key);
        __tl_store_arr(this.vals, slot, val);
        __tl_store_arr(this.used, slot, 1);
        return 1;
    }

    ComeAndDo get(string key) {
        int slot = this.findSlot(key);
        if (slot < 0) { return ""; }
        int occ = __tl_load_arr(this.used, slot);
        if (occ == 0) { return ""; }
        return __tl_load_arr(this.vals, slot);
    }

    ComeAndDo has(string key) {
        int slot = this.findSlot(key);
        if (slot < 0) { return 0; }
        return __tl_load_arr(this.used, slot);
    }

    ComeAndDo remove(string key) {
        int slot = this.findSlot(key);
        if (slot < 0) { return 0; }
        int occ = __tl_load_arr(this.used, slot);
        if (occ == 0) { return 0; }
        __tl_store_arr(this.used, slot, 0);
        this.size = this.size - 1;
        return 1;
    }

    ComeAndDo len() {
        return this.size;
    }

    ComeAndDo isEmpty() {
        if (this.size == 0) { return 1; }
        return 0;
    }

    ComeAndDo clear() {
        int i = 0;
        while (i < this.cap) {
            __tl_store_arr(this.used, i, 0);
            i = i + 1;
        }
        this.size = 0;
    }
}

// ── IntMap ───────────────────────────────────────────────────────────────────

class IntMap {
    int keys[];
    int vals[];
    int used[];
    int size;
    int cap;

    ComeAndDo init(int capacity) {
        this.cap  = capacity;
        this.size = 0;
        this.keys = __tl_alloc_arr(capacity);
        this.vals = __tl_alloc_arr(capacity);
        this.used = __tl_alloc_arr(capacity);
    }

    ComeAndDo findSlot(string key) {
        int h = __tl_str_len(key);
        int i = 0;
        while (i < __tl_str_len(key)) {
            h = h + __tl_str_char_at(key, i);
            i = i + 1;
        }
        if (h < 0) { h = -h; }
        int slot = h - (h / this.cap) * this.cap;
        int probes = 0;
        while (probes < this.cap) {
            int occ = __tl_load_arr(this.used, slot);
            if (occ == 0) { return slot; }
            if (__tl_str_eq(__tl_load_arr(this.keys, slot), key) == 1) { return slot; }
            slot = slot + 1;
            if (slot == this.cap) { slot = 0; }
            probes = probes + 1;
        }
        return -1;
    }

    ComeAndDo put(string key, int val) {
        int slot = this.findSlot(key);
        if (slot < 0) { return 0; }
        int occ = __tl_load_arr(this.used, slot);
        if (occ == 0) { this.size = this.size + 1; }
        __tl_store_arr(this.keys, slot, key);
        __tl_store_arr(this.vals, slot, val);
        __tl_store_arr(this.used, slot, 1);
        return 1;
    }

    ComeAndDo get(string key) {
        int slot = this.findSlot(key);
        if (slot < 0) { return 0; }
        int occ = __tl_load_arr(this.used, slot);
        if (occ == 0) { return 0; }
        return __tl_load_arr(this.vals, slot);
    }

    ComeAndDo has(string key) {
        int slot = this.findSlot(key);
        if (slot < 0) { return 0; }
        return __tl_load_arr(this.used, slot);
    }

    ComeAndDo remove(string key) {
        int slot = this.findSlot(key);
        if (slot < 0) { return 0; }
        int occ = __tl_load_arr(this.used, slot);
        if (occ == 0) { return 0; }
        __tl_store_arr(this.used, slot, 0);
        this.size = this.size - 1;
        return 1;
    }

    ComeAndDo getOrDefault(string key, int def) {
        if (this.has(key) == 0) { return def; }
        return this.get(key);
    }

    ComeAndDo increment(string key) {
        int cur = this.getOrDefault(key, 0);
        this.put(key, cur + 1);
    }

    ComeAndDo len() {
        return this.size;
    }

    ComeAndDo isEmpty() {
        if (this.size == 0) { return 1; }
        return 0;
    }
}
