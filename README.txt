================================================================================
CSOPESY MO2 - Multitasking OS Emulator with Demand-Paging Memory Management
================================================================================

A C++ command-line emulator of an operating system's process scheduler (FCFS /
round-robin) combined with a demand-paging memory manager backed by a text-file
backing store.

GitHub: https://github.com/MikhosCedric/MO1-CSOPESY   (branch: feature/mo2)


--------------------------------------------------------------------------------
AUTHORS
--------------------------------------------------------------------------------

  Cabato, Diane
  Gumapos, Cedric
  Foo, James
  Julian, Jedidiah


--------------------------------------------------------------------------------
ENTRY CLASS FILE
--------------------------------------------------------------------------------

  main.cpp  (project root)

The main() function is located in main.cpp. It constructs a ConsoleManager and
calls ConsoleManager::run(), which is the CLI loop and lives in
ConsoleManager.cpp.


--------------------------------------------------------------------------------
REQUIREMENTS
--------------------------------------------------------------------------------

  - Windows
  - Visual Studio 2022 (or MSBuild) with the C++ desktop workload
  - Platform toolset v143, Windows SDK 10.0, C++17


--------------------------------------------------------------------------------
HOW TO BUILD AND RUN
--------------------------------------------------------------------------------

Visual Studio:

  1. Open MO1.sln  (this is the only solution file; open this one)
  2. Select the x64 platform, then Build Solution (Ctrl+Shift+B)
  3. Run with Ctrl+F5

Command line:

  msbuild MO1.sln /p:Configuration=Debug /p:Platform=x64
  x64\Debug\MO1.exe

IMPORTANT: run the program from the project root. config.txt is read from the
current working directory, so launching x64\Debug\MO1.exe from inside that
folder will fail to find it. Visual Studio already uses the project root.


--------------------------------------------------------------------------------
CONFIGURATION (config.txt)
--------------------------------------------------------------------------------

config.txt is read by the "initialize" command and is space-separated. All
memory values must be a POWER OF TWO within [64, 65536] bytes.

  num-cpu             Number of CPU cores.                    [1, 128]
  scheduler           "fcfs" or "rr"
  quantum-cycles      Round-robin time slice.                 [1, 2^32]
  batch-process-freq  Ticks between generated processes.      [1, 2^32]
  min-ins / max-ins   Instructions per generated process.     [1, 2^32]
  delay-per-exec      Busy-wait ticks per instruction.        [0, 2^32]
  max-overall-mem     Total physical memory, in bytes.
  mem-per-frame       Bytes per frame (= page size).
  min-mem-per-proc    Lower bound of the per-process roll.
  max-mem-per-proc    Upper bound of the per-process roll.

max-overall-mem must be divisible by mem-per-frame, and min-mem-per-proc must
not exceed max-mem-per-proc. Invalid values are reported and "initialize"
fails, leaving the emulator uninitialized.


--------------------------------------------------------------------------------
COMMANDS
--------------------------------------------------------------------------------

Main menu ("initialize" must be run first; nothing else is recognized before
it):

  initialize                      Read config.txt and start the emulator
  screen -s <name> <mem_size>     Create a process and attach to its screen
  screen -c <name> <mem_size> "<instructions>"
                                  Create a process running 1-50 user-supplied,
                                  semicolon-separated instructions
  screen -r <name>                Re-attach to a process; reports the memory
                                  access violation if it was killed by one
  screen -ls                      List running and finished processes
  scheduler-start                 Begin generating processes in batch
  scheduler-stop                  Stop generating processes
  report-util                     Write the screen -ls report to csopesy-log.txt
  process-smi                     Summary of memory use, nvidia-smi style
  vmstat                          Detailed memory and CPU-tick statistics
  exit                            Quit

Inside an attached process screen:

  process-smi                     Show that process's logs and progress
  exit                            Return to the main menu

Process instructions (usable with screen -c):

  DECLARE <var> <value>           Declare a uint16 variable
  ADD <dest> <a> <b>              dest = a + b   (a, b: variable or literal)
  SUBTRACT <dest> <a> <b>         dest = a - b
  PRINT("text")                   Print a message
  PRINT("text" + <var>)           Print a message with a variable appended
  SLEEP <ticks>                   Sleep for a number of CPU ticks
  FOR([<instructions>], <n>)      Repeat a block n times (may be nested)
  READ <var> <hex_address>        Load a uint16 from memory into var
  WRITE <hex_address> <value>     Store a uint16 to memory

Memory addresses must use the 0x-prefixed hexadecimal form, e.g. 0x500.

Example (type as a single line; the inner quotes are NOT escaped):

  screen -c process2 2048 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT("Result: " + varC)"

This prints "Result: 15". View it with "screen -r process2" followed by
"process-smi" while the process is still running.


--------------------------------------------------------------------------------
NOTES ON BEHAVIOUR
--------------------------------------------------------------------------------

  - Each process owns its own address space. Bytes [0, 64) are its symbol table
    segment, holding up to 32 uint16 variables of 2 bytes each; declarations
    past the 32nd are silently ignored. The rest is user-addressable.

  - Reading or writing outside a process's own memory space is an access
    violation: the process is killed, and screen -r reports the time and the
    offending address.

  - Memory is allocated lazily. A new process's pages all start in the backing
    store marked invalid, and a frame is claimed only when a page fault occurs
    while the process is on a CPU core. When memory is full, a FIFO victim page
    is evicted to the backing store and the faulting instruction is restarted.


--------------------------------------------------------------------------------
FILES GENERATED AT RUNTIME
--------------------------------------------------------------------------------

  csopesy-backing-store.txt   The backing store. Lists every live process (id,
                              name, memory size, page count, command counter)
                              and its currently swapped-out pages. Readable at
                              any time while the emulator is running.
  csopesy-log.txt             Written by report-util.
  log\memory_stamp_NN.txt     Periodic snapshots of the frame table.
