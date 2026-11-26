# DiskScope — Intelligent Disk I/O Scheduling Analyzer

**Advanced Operating Systems Mini Project**

DiskScope is an interactive browser-based disk scheduling simulator. Its scheduling engine and HTTP backend are written in **C**. A lightweight HTML, CSS, and JavaScript frontend communicates with the C backend through a local HTTP API.

The application implements six disk I/O scheduling policies:

1. FCFS — First Come First Serve
2. SSTF — Shortest Seek Time First
3. SCAN — Elevator Algorithm
4. C-SCAN — Circular SCAN
5. LOOK
6. C-LOOK — Circular LOOK

## Features

- Interactive inputs for initial head position, disk range, scan direction, and request queue.
- Compare all six algorithms or inspect one selected algorithm.
- Automatically calculates total head movement and average movement per request.
- Shows the service order, full head path, movement-by-movement breakdown, and a chart.
- Highlights the algorithm with the least total head movement for the current input.
- Includes a sample queue and random queue generator.
- Validates the disk range, request values, direction, and queue length.
- Uses a C socket server; no web framework or third-party C library is required.
- Binds to `127.0.0.1` so the development server is available only on the local machine.

## Project structure

```text
intelligent-disk-io-analyzer/
├── src/
│   └── server.c             # C HTTP server, validation, and scheduling engine
├── web/
│   ├── index.html           # Frontend layout
│   ├── style.css            # Responsive visual design
│   └── app.js               # User interaction, API calls, chart, and result rendering
├── docs/
│   ├── PROJECT_REPORT.md    # Mini-project report source
│   ├── PROJECT_REPORT.docx  # Formatted Word report
│   ├── VIVA_QUESTIONS.md    # Viva questions with answers
│   └── TEST_CASES.md        # Manual test cases and expected behaviour
├── tests/
│   └── test_api.py          # Optional API smoke tests
├── Makefile                 # Linux/macOS build
├── build.bat                # Windows build using MinGW-w64 GCC
├── run.bat                  # Windows run helper
└── README.md
```

The detailed report is included in both editable Markdown and Word formats under `docs/`.

## Requirements

### Windows

- Windows 10/11
- GCC from MinGW-w64 (add the MinGW `bin` directory to `PATH`)
- A modern browser such as Chrome, Edge, or Firefox

### Linux

- GCC or Clang
- `make`
- A modern browser

The project uses only the C standard library and the operating system's socket API. The browser frontend uses native JavaScript and Canvas; there are no npm packages to install.

## Build and run on Windows

1. Extract the project folder.
2. Open the folder in File Explorer.
3. Double-click `build.bat`, or open Command Prompt in the project folder and run:

   ```bat
   build.bat
   ```

4. Run the server:

   ```bat
   run.bat
   ```

5. Open **http://127.0.0.1:8080** in your browser.
6. Keep the console window open while using the application. Press `Ctrl+C` in that window to stop the server.

If port 8080 is busy, run the program with a different port, for example `disk_analyzer.exe 8090`, and open `http://127.0.0.1:8090`.

## Build and run on Linux/macOS

Open a terminal in the project root:

```bash
make
./disk_analyzer
```

Then open **http://127.0.0.1:8080**. To use another port:

```bash
./disk_analyzer 8090
```

To remove the compiled executable:

```bash
make clean
```

## How to use the application

1. Enter the initial disk-head position.
2. Enter the minimum and maximum cylinder numbers. The default disk range is 0–199.
3. Select the initial sweep direction (left or right). It is used by SCAN, C-SCAN, LOOK, and C-LOOK; FCFS and SSTF do not require a direction.
4. Enter the request queue as comma- or space-separated integer cylinder positions.
5. Choose **Compare all 6 algorithms** or a specific algorithm.
6. Press **Run analysis**.
7. Select an algorithm in the comparison table or detail dropdown to inspect its movement chart and step breakdown.

The random generator creates a queue within the selected disk limits. The sample button restores the demonstration queue.

## Scheduling and metric conventions

- The initial head position is included as the first point in the path, but it is not counted as a serviced request.
- A request located exactly at the initial head position is serviced at the beginning for SCAN-family algorithms.
- SSTF chooses the closest pending request. If two requests are equally close, the one that appeared first in the input queue is selected.
- SCAN goes to the physical disk boundary only when there are pending requests on the other side of the head; it then reverses and services those requests.
- C-SCAN moves to the relevant disk boundary and counts the return from one end of the disk to the other as head movement.
- LOOK turns at the last pending request rather than the physical disk boundary.
- C-LOOK jumps from the last request in one direction to the first pending request at the opposite end of the request set. That jump is included in total head movement.
- **Total head movement** is the sum of the absolute cylinder differences for every movement step, including boundary travel and circular wrap movement.
- **Average seek movement** is total head movement divided by the number of disk requests.

These conventions are explicitly used by the C implementation so that the displayed comparison is internally consistent. Different textbooks sometimes present simplified examples with slightly different assumptions for empty sides or circular wrap accounting.

## Backend API

The C server exposes two local endpoints.

### Health check

```http
GET /api/health
```

### Run a simulation

```http
POST /api/analyze
Content-Type: application/json
```

Example request:

```json
{
  "head": 53,
  "disk_min": 0,
  "disk_max": 199,
  "direction": "right",
  "algorithm": "ALL",
  "requests": [98, 183, 37, 122, 14, 124, 65, 67]
}
```

Set `algorithm` to `ALL`, `FCFS`, `SSTF`, `SCAN`, `C-SCAN`, `LOOK`, or `C-LOOK`. The response contains `service_order`, `path`, `steps`, `total_movement`, `average_seek`, `max_seek`, and `min_seek` for each selected algorithm. Each step is tagged as `service`, `boundary`, or `wrap`.

## Run the optional API tests

Start the C server in one terminal. In a second terminal, from the project root run:

```bash
python tests/test_api.py
```

The script uses Python only as a test client; all scheduling calculations are performed by the C backend. If you selected a custom port, set the base URL first, for example `DISKSCOPE_URL=http://127.0.0.1:8090 python tests/test_api.py` (PowerShell: `$env:DISKSCOPE_URL="http://127.0.0.1:8090"; python tests/test_api.py`).

## Important note for submission

The C program is the backend and contains all six scheduling algorithms. HTML, CSS, and JavaScript are used only for the browser interface and visualization. This separation makes the project interactive while keeping the operating-system scheduling logic in C.
