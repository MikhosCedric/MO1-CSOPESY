# MO1-CSOPESY

A C++ console-based process scheduling and management simulator.

## Authors

- Cabato, Diane
- Gumapos, Mikhos
- Foo, James
- Julian, Jedidiah

## Prerequisites

- Visual Studio (or MSBuild)
- C++ compiler with C++17 support

## How to Run

1. Open `MO1.sln` in Visual Studio.
2. Build the solution (**Ctrl+Shift+B**).
3. Run (**F5** for debug, **Ctrl+F5** without debug).

Or via command line:
```
msbuild MO1.vcxproj
```
Then run the generated executable in the output directory.

## Entry Point

The `main()` function is located in **`main.cpp`** at the project root. It creates a `ConsoleManager` instance and calls `run()` to start the console interface.

## Configuration

The program reads `config.txt` from the project root to set initial scheduling parameters.
