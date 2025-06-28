# Group 3: MO1-Process-Multiplexer-and-CLI

Members:
-------------

- Roman, Isaac Nathan
- Campos, Annika
- Borlaza, Clarence 
- Arcega, Alexis

Description:
-------------
This project is an operating system emulator that simulates process scheduling and management using a process multiplexer. It features a custom command-line interface (CLI) for user interaction and control. Users can create, manage, and monitor simulated processes, supporting multiple scheduling algorithms (FCFS, RR), and view real-time process information.

How to Run:
-----------
1. Make sure you have a C++ compiler installed (e.g., g++, MSVC).
2. Place a valid `config.txt` file in the project directory with your desired configuration.
3. Open a terminal or command prompt in this directory.
4. Compile the program:
   - Using g++:
     ```
     g++ main.cpp -o csopesy
     ```
  - Or simply run/debug from your IDE
5. Run the program:
   ```
   ./csopesy
   ```

Entry Point:
------------
The entry class file containing the `main` function is:
> main.cpp

Commands:
---------
- `initialize` : Initialize the system using `config.txt`
- `screen -s <name>` : Create a new process screen.
- `screen -r <name>` : Attach to an existing process screen
- `screen -ls` : List all process screens
- `scheduler-start` : Start process generation
- `scheduler-stop` : Stop process generation
- `report-util` : Generate a utilization report in `csopesy-log.txt`
- `clear` : Clear the screen
- `exit` : Exit the program

