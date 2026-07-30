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
