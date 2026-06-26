Act as a Senior Systems Programmer. I am building a Major Output for an Operating Systems course (CSOPESY). We need to build a Process Scheduler and Command Line Interpreter (CLI) emulator.

Please scaffold this project in C++. The architecture must be strictly modular, separating the UI/CLI from the underlying CPU simulation and scheduling logic. 

Here are the strict specifications to implement:

### 1. Architecture Requirements
- **CLI/View Layer:** Handles parsing user input, navigating between the "Main Menu" and "Process Screens", and rendering ASCII tables.
- **OS Controller/Kernel:** Manages global state, CPU ticks (a background integer counter simulating frame passes), and the active scheduling algorithm.
- **Scheduler Engine:** Implements two algorithms based on a config file: First-Come-First-Serve (`"fcfs"`) and Round-Robin (`"rr"`).
- **Process Control Block (PCB):** Represents a process with attributes like Process Name, ID, State, Lines of Code, Current Instruction Line, attached Core, and a memory map for `uint16` variables.

### 2. Startup & Configuration
- The app launches to a Main Menu with an ASCII art header "CSOPESY", developer names, and last updated date.
- The ONLY command allowed initially is `initialize` (and `exit`).
- `initialize` reads a `config.txt` file in the root directory with space-separated key-value pairs:
  - `num-cpu`: [1, 128]
  - `scheduler`: "fcfs" or "rr"
  - `quantum-cycles`: [1, 2^32]
  - `batch-process-freq`: [1, 2^32]
  - `min-ins` / `max-ins`: [1, 2^32]
  - `delay-per-exec`: [0, 2^32]

### 3. Main Menu Commands
- `screen -s <process_name>`: Creates a new process with randomized dummy instructions (between `min-ins` and `max-ins`) and switches the view to this process's screen.
- `screen -ls`: Prints a summary formatted exactly as:
  - CPU utilization: [X]%
  - Cores used: [X]
  - Cores available: [X]
  - List of Running processes (Name, Timestamp, Core attached, current line / total lines)
  - List of Finished processes (Name, Timestamp, "Finished", total lines / total lines)
- `screen -r <process_name>`: Switches view to an existing process. Prints "Process <process name> not found." if it doesn't exist.
- `scheduler-start`: Starts a background thread/task generating a new process every `batch-process-freq` CPU ticks. Names them `p01`, `p02`, etc.
- `scheduler-stop`: Stops the auto-generation.
- `report-util`: Same output as `screen -ls`, but writes/appends it to `csopesy-log.txt`.

### 4. Process Screen Commands (When attached to a screen)
- `process-smi`: Prints Process Name, ID, current line, total lines, and logs. The logs consist of executed `PRINT` commands with the timestamp and assigned core. If the process is done, print "Finished!".
- `exit`: Detaches and goes back to the main menu.

### 5. Dummy Instructions (Randomly generated for processes)
- Instructions execute based on the `delay-per-exec` parameter.
- `PRINT(msg)`: Appends to process logs. Default msg is "Hello world from <process_name>!".
- `DECLARE(var, value)`: Stores a `uint16` variable for the process. Clamped to max `uint16`.
- `ADD(var1, var2/val, var3/val)` & `SUBTRACT(...)`: Basic math on process variables.
- `SLEEP(X)`: Relinquishes CPU for X ticks.
- `FOR([instructions], repeats)`: Loops instructions. Must support nesting up to 3 times.

### Task:
1. Provide the file/folder structure for this project.
2. Generate the core classes/structs (e.g., `ConfigParser`, `Process`, `Scheduler`, `ConsoleUI`).
3. Provide the main simulation loop that increments the CPU tick and handles process execution in the background while the CLI listens for input.