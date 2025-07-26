# Refactoring Plan: Port JSON Diff/Patch from cJSON to JSMN

This document captures the high‑level plan to migrate the `json_diff` and `json_patch` logic off of cJSON and onto JSMN token processing. It also includes the earlier micro‑benchmark & parsing cleanup phases for reference.

---

## Original Micro‑benchmark & Parser Cleanup Plan

**Phase 0 – Benchmark Harness**
1. Add standalone `bench_medium.c` to isolate the medium diff test (5 warm‑up + 50 diffs).
2. Integrate the harness into Meson (`bench-medium`) and Makefile (`make bench`).

**Phase 1 – Hotspot Identification**
1. Build a release‑mode binary (`-O3 -DNDEBUG`).
2. Profile the harness (perf/Callgrind) to pinpoint time spent in `json_diff`, `cJSON_Duplicate`, and malloc/free.

**Phase 2 – Arena Allocator for Diff Trees**
1. Introduce a bump‑pointer `json_diff_arena` allocator.
2. Replace deep copies (`cJSON_Duplicate`) in `create_*_array()` with JSMN references or scalar copies.
3. Wrap `json_diff()` to install arena hooks for fast `malloc()`/`free()`.

**Phase 3 – Parser Micro‑benchmark**
1. Add `bench_parse.c` to measure raw `cJSON_Parse` vs. JSMN parse + cJSON builder.
2. Integrate `bench-parse` & `bench-jsmn` targets.

**Phase 4 – JSMN→cJSON Parser Prototype**
1. Vendor JSMN, add `parse_jsmn.c/h` to build and build a cJSON tree.
2. Expose `cjson_parse_jsmn()` and `json_diff_str()` convenience API.

---

## New Phase 2–3: Full cJSON→JSMN Token Tree Port

**Phase 2 – Token Tree Helpers**
1. **`src/jsmn_tree.h/c`**: Implement `jsmntree_t` that wraps JSMN tokens and source text.
2. Provide:
   - `jsmntree_init(tree, js)` and `jsmntree_free(tree)`
   - `jsmntree_num_children(tree, idx)` and `jsmntree_child(tree, parent, n)`
   - `jsmntree_token_equal(tree1, idx1, tree2, idx2, strict)`

**Phase 3 – Diff & Patch on JSMN Tokens**
1. Refactor `json_diff()` to `diff_jsmn(tree1, idx1, tree2, idx2, opts)` returning a JSMN token diff or raw JSON.
2. Refactor `json_patch()` to `patch_jsmn(tree, diff, opts)` and serialize back to JSON.
3. Update `json_diff_str()/json_patch_str()` and `meson.build`/`Makefile` to remove cJSON build‐deps.

**Phase 4 – Cleanup & Removal of cJSON**
1. Delete all `<cjson/cJSON.h>` includes and code paths.
2. Remove `libcjson` from Meson and Makefile deps.
3. Update tests to use JSMN token diff/patch outputs directly.

---

Once Phase 2 is complete, we will have a pure JSMN token‑based implementation of JSON diff & patch, eliminating the cJSON dependency entirely and further improving performance and maintainability.