CSOPESY MO2 - Multitasking OS Emulator
======================================

AUTHORS
-------
Cabato, Diane
Gumapos, Cedric
Foo, James
Julian, Jedidiah


ENTRY CLASS FILE
----------------
main.cpp (project root)

main() constructs a ConsoleManager and calls ConsoleManager::run(), which is
the CLI loop. That loop lives in ConsoleManager.cpp.


REQUIREMENTS
------------
Windows
Visual Studio 2022 (or MSBuild) with the "Desktop development with C++" workload
Platform toolset v143, Windows SDK 10.0, C++17


HOW TO RUN - VISUAL STUDIO
--------------------------
1. Open MO1.sln (the only solution file in the project).
2. Set the toolbar dropdowns to Debug and x64.
3. Build with Ctrl+Shift+B.
4. Run with Ctrl+F5 (Start Without Debugging).

   Use Ctrl+F5, not F5. Ctrl+F5 keeps the console window open when the
   program exits; F5 closes it immediately.

5. At the "root:\>" prompt, type:  initialize
   No other command is recognized until initialize has been run.


HOW TO RUN - COMMAND LINE
-------------------------
    msbuild MO1.sln /p:Configuration=Debug /p:Platform=x64
    x64\Debug\MO1.exe

IMPORTANT: run the .exe from the project root, not from inside x64\Debug.
config.txt is read from the current working directory, so launching it from
its own folder means initialize cannot find the file and fails. Visual
Studio already uses the project root, so this only affects launching the
.exe by hand. If you do want to run it from x64\Debug, copy config.txt there
first.


CONFIGURATION
-------------
initialize reads config.txt from the working directory. It is space-separated,
one parameter per line:

    num-cpu             number of CPU cores, [1, 128]
    scheduler           "fcfs" or "rr"
    quantum-cycles      round-robin time slice
    batch-process-freq  ticks between generated processes
    min-ins / max-ins   instructions per generated process
    delay-per-exec      busy-wait ticks per instruction, 0 = one per cycle
    max-overall-mem     total memory in bytes
    mem-per-frame       bytes per frame (= page size)
    min-mem-per-proc    lower bound of the per-process memory roll
    max-mem-per-proc    upper bound of the per-process memory roll

All four memory values must be a power of two within [64, 65536] bytes,
max-overall-mem must divide evenly by mem-per-frame, and min-mem-per-proc
must not exceed max-mem-per-proc. Invalid values are reported and initialize
fails.

To change parameters, close the program, edit config.txt, save, and run
again. No rebuild is needed - initialize re-reads the file on every run.


COMMANDS
--------
    initialize                      read config.txt and start the emulator
    screen -s <name> <mem_size>     create a process and open its screen
    screen -c <name> [<mem_size>] "<instructions>"
                                    create a process running 1-50
                                    semicolon-separated instructions
    screen -r <name>                open an existing process's screen
    screen -ls                      list running and finished processes
    scheduler-start                 begin generating processes in batch
    scheduler-stop                  stop generating processes
    report-util                     write the screen -ls report to csopesy-log.txt
    process-smi                     memory summary, nvidia-smi style
    vmstat                          detailed memory and CPU-tick statistics
    exit                            quit

Inside a process screen the prompt changes to "[<name>]\>". There, process-smi
refreshes that process's logs and progress, and exit returns to the main menu.

Example of screen -c (all on one line):

    screen -c process2 2048 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT("Result: " + varC)"

This prints "Result: 15". The escaped form, PRINT(\"Result: \" + varC), also
works, so a line copied from the spec can be pasted as-is.


FILES CREATED WHILE RUNNING
---------------------------
These appear in the working directory (the project root when launched from
Visual Studio):

    csopesy-backing-store.txt   page-movement log for the backing store
    proc-logs\proc<id>.txt      per-process execution trace
    log\memory_stamp_NN.txt     frame-table snapshots (round-robin only)
    csopesy-log.txt             written by report-util

initialize clears csopesy-backing-store.txt and proc-logs\ at the start of
each run. The log\ folder is not cleared, so delete it by hand if you need
its contents to belong to a single session.
