# CSOPESY MO2 — Multitasking OS Emulator with Demand-Paging Memory Management

A C++ command-line emulator of an operating system's process scheduler
(FCFS / round-robin) combined with a **demand-paging memory manager** backed by a
plain-text backing store.

> This file is the SOURCE deliverable's README. The spec accepts a GitHub link in
> place of a `README.txt`, so this is the single copy — there is no separate
> plain-text version to keep in sync.

## Authors

- Cabato, Diane
- Gumapos, Cedric
- Foo, James
- Julian, Jedidiah

## Entry class file

**`main.cpp`** (project root). `main()` constructs a `ConsoleManager` and calls
`ConsoleManager::run()`, which is the CLI loop and lives in `ConsoleManager.cpp`.

## Requirements

- Windows
- Visual Studio 2022 (or MSBuild) with the C++ desktop workload
- Platform toolset v143, Windows SDK 10.0, C++17

## Build and run

**Visual Studio** — open `MO1.sln` (the only solution file), select the **x64**
platform, build with <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd>, run with
<kbd>Ctrl</kbd>+<kbd>F5</kbd>.

**Command line:**

```bash
msbuild MO1.sln /p:Configuration=Debug /p:Platform=x64
```

```bash
x64\Debug\MO1.exe
```

> **Run from the project root.** `config.txt` is read from the current working
> directory, so launching `x64\Debug\MO1.exe` from inside that folder fails to
> find it and `initialize` will not succeed. Visual Studio already uses the
> project root.

## Configuration (`config.txt`)

Read by `initialize`, space-separated. **All memory values must be a power of two
within [64, 65536] bytes.**

| Parameter | Meaning | Range |
|---|---|---|
| `num-cpu` | Number of CPU cores | [1, 128] |
| `scheduler` | `"fcfs"` or `"rr"` | — |
| `quantum-cycles` | Round-robin time slice | [1, 2³²] |
| `batch-process-freq` | Ticks between generated processes | [1, 2³²] |
| `min-ins` / `max-ins` | Instructions per generated process | [1, 2³²] |
| `delay-per-exec` | Busy-wait ticks per instruction | [0, 2³²] |
| `max-overall-mem` | Total physical memory, in bytes | power of 2 |
| `mem-per-frame` | Bytes per frame (= page size) | power of 2 |
| `min-mem-per-proc` | Lower bound of the per-process roll | power of 2 |
| `max-mem-per-proc` | Upper bound of the per-process roll | power of 2 |

`max-overall-mem` must be divisible by `mem-per-frame`, and `min-mem-per-proc`
must not exceed `max-mem-per-proc`. Invalid values are reported and `initialize`
fails, leaving the emulator uninitialized.

## Commands

`initialize` must be run first — no other command is recognized before it.

| Command | Behaviour |
|---|---|
| `initialize` | Read `config.txt` and start the emulator |
| `screen -s <name> <mem_size>` | Create a process and attach to its screen |
| `screen -c <name> [mem_size] "<instructions>"` | Create a process running 1–50 semicolon-separated instructions |
| `screen -r <name>` | Re-attach and show the process's log; reports the memory access violation if the process was killed by one |
| `screen -ls` | List running and finished processes |
| `scheduler-start` / `scheduler-test` | Begin generating processes in batch |
| `scheduler-stop` | Stop generating processes |
| `report-util` | Write the `screen -ls` report to `csopesy-log.txt` |
| `process-smi` | Memory summary, nvidia-smi style |
| `vmstat` | Detailed memory and CPU-tick statistics |
| `exit` | Quit |

Inside an attached process screen: `process-smi` shows that process's logs and
progress; `exit` returns to the main menu.

## Process instructions

Usable with `screen -c`:

| Instruction | Meaning |
|---|---|
| `DECLARE <var> <value>` | Declare a uint16 variable |
| `ADD <dest> <a> <b>` | `dest = a + b` (operands may be variables or literals) |
| `SUBTRACT <dest> <a> <b>` | `dest = a - b` |
| `PRINT("text")` | Print a message |
| `PRINT("text" + <var>)` | Print a message with a variable appended |
| `SLEEP <ticks>` | Sleep for a number of CPU ticks |
| `FOR([<instructions>], <n>)` | Repeat a block n times (may be nested) |
| `READ <var> <hex_address>` | Load a uint16 from memory into `var` |
| `WRITE <hex_address> <value>` | Store a uint16 to memory |

Memory addresses use the `0x`-prefixed hexadecimal form, e.g. `0x500`.

**Example** (single line):

```
screen -c process2 2048 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT("Result: " + varC)"
```

Prints `Result: 15`. The escaped form the spec prints its examples in works too —
`PRINT(\"Result: \" + varC)` — so a line copied straight out of the spec or a
quiz sheet can be pasted as-is.

View the output with `screen -r process2` then `process-smi` **while the process
is still running**. Once it finishes, `screen -r` correctly reports
`Process process2 not found.` and the log is no longer reachable, so attach
promptly or raise `delay-per-exec` to slow execution down.

## Behaviour notes

- Each process owns its own address space. Bytes `[0, 64)` are its **symbol table
  segment**, holding up to 32 uint16 variables of 2 bytes each; declarations past
  the 32nd are silently ignored. The rest is user-addressable.
- Reading or writing outside a process's own memory space is an **access
  violation**: the process is killed, and `screen -r` reports the time and the
  offending address.
- Memory is allocated **lazily**. A new process's pages all start in the backing
  store marked invalid, and a frame is claimed only when a page fault occurs
  while the process is on a CPU core. When memory is full a FIFO victim page is
  evicted to the backing store and the faulting instruction is restarted.
- `READ` and `WRITE` touch two pages — the symbol table segment for their
  variable operand, and the page holding the target address — and make them
  resident **one at a time**, resuming mid-instruction after a fault. Demanding
  both at once would hang any configuration with a single frame, since neither
  page can be brought in without evicting the other.
- `screen -c` **without** a memory size (the form the spec's own examples use)
  gives the process the largest legal address space, 65536 bytes. `min-mem-per-proc`
  and `max-mem-per-proc` govern processes created by `scheduler-start`, not this
  one. Demand paging means the space costs no frames until it is touched.
- A process killed by an access violation is listed by `screen -ls` as
  **`Terminated`** at the line it died on, not as `Finished`.
- **`screen -r` reaches a finished process** and prints its log on attach. The
  spec words this as "not found/finished execution" → `not found`, but a short
  instruction list completes in well under a second, which would make its output
  permanently unreachable — so a finished process stays attachable and only a
  genuinely unknown name reports `Process <name> not found.` Attaching renders
  the process view immediately, as the MO1 mockup shows; `process-smi` then
  refreshes the same view.
- **`CPU utilization` and `Cores used` measure different things.** `Cores used`
  is how many cores currently hold a process, so it always matches the list
  printed beneath it. `CPU utilization` is the share of core-ticks that actually
  executed an instruction, averaged over the last second. Under memory pressure
  an occupied core can fault every tick without executing anything, so all cores
  can read as used while utilization sits in single digits — that is the
  intended signal, not a contradiction.
- `process-smi` labels memory **MiB** to match the spec's mockup, but the figures
  are the raw byte values from `config.txt`. Since every memory parameter is
  capped at 65,536 bytes, converting to real MiB would display `0` everywhere.

## Files generated at runtime

All are written to the working directory — the project root when launched from
Visual Studio.

| File | Contents | Cleared by `initialize` |
|---|---|---|
| `csopesy-backing-store.txt` | Append-only log of page movement, one line per event: `Process 2 page 1 evicted to backing store at Tue Aug 5 22:09:05 2025`. Readable at any time while running. | yes |
| `proc-logs\proc<id>.txt` | Per-process execution trace — one line per instruction executed and per page-in, stamped with the CPU tick and core. | yes |
| `log\memory_stamp_NN.txt` | Periodic snapshots of the frame table. Written only under the `rr` scheduler; `fcfs` produces none. | **no** — see below |
| `csopesy-log.txt` | Written by `report-util`. | n/a (overwritten each time) |

> `log\` is the one exception: snapshots are written per filename, so a short run
> does not remove higher-numbered files left by a longer one. Delete the folder
> between runs if you need its contents to belong to a single session.
