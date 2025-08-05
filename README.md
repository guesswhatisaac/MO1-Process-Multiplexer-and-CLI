Group 3: MO2 Multitasking OS


Members:
-----------
- Roman, Isaac Nathan
- Campos, Annika
- Borlaza, Clarence 
- Arcega, Alexis


Description:
-----------
This project is an operating system emulator that simulates process scheduling and management using a process multiplexer. It features a custom command-line interface (CLI) for user interaction and control. Initially built to handle process scheduling (FCFS, RR), it has been extended with a sophisticated virtual memory manager. This new system simulates demand paging, page fault handling, and a backing store, allowing the emulator to run more processes than physical memory would normally allow and providing a deep, interactive look into modern OS memory management.


How to Run:
-----------
1. Make sure you have a C++ compiler installed (e.g., g++, MSVC).

2. Place a valid `config.txt` file in the project directory with your desired configuration.

3. Open a terminal or command prompt in this directory.

4. Compile the program. Note: You must include all four source files.
   
   Using g++ (recommended for Linux/macOS/MinGW):
     g++ main.cpp Scheduler.cpp Process.cpp MemoryManager.cpp -o csopesy_emulator -pthread

   Using MSVC on Windows:
     cl main.cpp Scheduler.cpp Process.cpp MemoryManager.cpp

5. Run the program:
   
   On Windows:
     csopesy_emulator.exe

   On Linux/macOS:
     ./csopesy_emulator


Entry Point:
------------
The entry class file containing the `main` function is:
> main.cpp



Commands:
-----------
- initialize : Initialize the system using `config.txt`. (Must be run first).

- screen -s <name> <size> : Create a new process with a given name and virtual memory size (in bytes). The size must be a power of 2 between 64 and 65536.

- screen -r <name> : Attach to an existing process screen to view its live logs and progress. Also used to view the final status of a finished or terminated process.

- screen -c <name> <size> "<instructions>" : Create a new process with a name, memory size, and a custom, semicolon-separated string of instructions (e.g., "DECLARE varA 10; WRITE 0x100 varA").

- screen -ls : List all currently running and finished process screens, including their core assignment and progress.

- scheduler-start : Start the automatic generation of random processes based on the frequency set in `config.txt`.

- scheduler-stop : Stop the automatic generation of new processes.

- process-smi : (Process Status and Memory Information) Displays a high-level summary of system memory usage and a detailed list of all processes, their PIDs, virtual memory size, and their current status (e.g., Running, Waiting, MEM_FAULT, Finished).

- vmstat : (Virtual Memory Statistics) Shows detailed virtual memory statistics, including total, used, free, and active memory. Also displays CPU tick counts and the accumulated number of pages paged in and out.

- report-util : Generate a utilization report in `csopesy-log.txt` containing a snapshot of running and finished processes.

- clear : Clear the console screen.

- exit :  Shuts down all threads and exit the program.
