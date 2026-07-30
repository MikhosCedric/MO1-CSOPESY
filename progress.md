# Progress log

Memory logbook for CSOPESY MO2. Newest entries go at the **bottom**.
Read this alongside `plan.md`: `plan.md` says what is *planned*, this says what is *done*.

## Notes

- **Each entry is 2–3 lines, maximum.** If it needs more, it belongs in `plan.md` or a commit body.
- One entry per meaningful chunk of work, not per commit.
- Say what changed and what it unblocks — not how it was implemented.

---

## 2026-07-30 — Phase 1 & 2

Config: `mem-per-proc` split into `min`/`max-mem-per-proc`, shared `isValidMemSize` (power of two, [64, 65536]); `config.txt` updated to a spec-valid set.
`Process` gains `memorySize`, a 64-byte symbol table (32 slots), READ/WRITE with 32-bit addresses, `TERMINATED` + violation time/address, and tri-state `advance()` so a `PAGE_FAULT` does not consume the line.
Unblocks Phase 3 (allocator returns `PAGE_FAULT`); `screen -s <mem_size>` and the `screen -r` violation branch are still Phase 6.

## 2026-07-30 — Phase 3

`PagingAllocator` rewritten for true demand paging: per-process page tables, lazy `createProcess` (all pages invalid, written to the backing store at creation), FIFO eviction with the faulting instruction's frames pinned.
`Scheduler` now owns it instead of `MemoryManager` — memory is built at process creation, dispatch never blocks on it, and `destroyProcess` runs on completion and violation alike.
Remaining: Phase 4 collapses the swap directory into `csopesy-backing-store.txt`; Phase 5 reads the new stats getters for `process-smi` / `vmstat`.

## 2026-07-30 — Phase 4

`BackingStore` is now one readable `csopesy-backing-store.txt`: a record per live process (id, name, memory size, page count, command counter) followed by its swapped-out pages and their bytes.
State is authoritative in memory and the file is rewritten once per scheduler tick when dirty — rewriting inside each page-out was quadratic (~13k page records in a 20s batch run); dropping per-page file I/O made the emulator measurably faster.
Remaining: Phase 5 (`process-smi` / `vmstat` off the allocator's stats getters), Phase 6 (command surface).

## 2026-07-30 — Phase 5

`Scheduler` counts idle/active ticks per core per tick (verified exact at `num-cpu` 1 and 128) and re-exports the allocator's memory and paging counters.
Main-menu `process-smi` and `vmstat` added to `ConsoleManager`, laid out to match the spec's mockups — the attached-screen `process-smi` is untouched and still separate.
Remaining: Phase 6 (`screen -s <mem_size>`, `screen -c`, the instruction parser, and the `screen -r` violation branch).

## 2026-07-30 — Phase 6

`screen -s` now requires a memory size and `screen -c` takes 1–50 quoted instructions; both reject with the spec's exact `invalid memory allocation` / `invalid command`, and `screen -r` reports the violation line for a process killed by a bad address.
Real instruction parser lives in `Process.cpp` beside `generateInstructions` (depth-aware `;` splitting, so `FOR([...], n)` bodies nest) — the spec's worked example prints `Result: 15` end to end.
All six phases are in; plan.md's 12-item checklist passes. Remaining: Phase 0 repo hygiene (delete the `MO1-CSOPESY/` stub, retire `MemoryManager`), README.txt, and the PPT.

## 2026-07-30 — Phase 0 (hygiene)

Deleted the dead `MO1-CSOPESY/` stub project and its `.slnx`, leaving `MO1.sln` as the single entry point, and retired `MemoryManager.{h,cpp}` + `IMemoryAllocator.h` now that `PagingAllocator` is the only allocator.
Dropped their `MO1.vcxproj` / `.filters` entries; clean rebuild is 8 source files and the app still passes the smoke test. `CLAUDE.md`'s State section updated to match.
Remaining: `README.txt` (names, run instructions, entry file) and the PPT.
