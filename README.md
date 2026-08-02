# CSOPESY MO2 — Multitasking OS Emulator with Demand-Paging Memory Management

A C++ command-line emulator of an operating system's process scheduler
(FCFS / round-robin) combined with a **demand-paging memory manager** backed by a
plain-text backing store.

> `README.txt` is the same document in plain text — it is the copy submitted with
> the SOURCE deliverable. Keep the two in sync if you edit either.

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
| `screen -r <name>` | Re-attach; reports the memory access violation if the process was killed by one |
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

**Example** (single line; the inner quotes are *not* escaped):

```
screen -c process2 2048 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT("Result: " + varC)"
```

Prints `Result: 15`. View it with `screen -r process2` then `process-smi` while
the process is still running.

## Behaviour notes

- Each process has an emulated 16-bit address space. Bytes `[0, 64)` are its
  **symbol table segment**, holding up to 32 uint16 variables of 2 bytes each;
  declarations past the 32nd are silently ignored. Hexadecimal READ/WRITE
  addresses range from `0x0000` through `0xFFFF` (a uint16 word must start no
  later than `0xFFFE`).
- Reading or writing a uint16 word outside that address space is an **access
  violation**: the process is killed, and `screen -r` reports the time and the
  offending address.
- Memory is allocated **lazily**. A new process's pages all start in the backing
  store marked invalid, and a frame is claimed only when a page fault occurs
  while the process is on a CPU core. When memory is full a FIFO victim page is
  evicted to the backing store and the faulting instruction is restarted.
- **CPU utilization counts cores that are not stalled on a page fault.** Under
  memory pressure an occupied core can fault every tick without executing an
  instruction, so utilization drops below 100% — that is the intended signal.

## Files generated at runtime

| File | Contents |
|---|---|
| `csopesy-backing-store.txt` | The backing store: every live process (id, name, memory size, page count, command counter) and its currently swapped-out pages. Readable at any time while running. |
| `csopesy-log.txt` | Written by `report-util`. |
| `log\memory_stamp_NN.txt` | Periodic snapshots of the frame table. |
