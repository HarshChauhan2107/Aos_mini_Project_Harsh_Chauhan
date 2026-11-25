# Smart Job Scheduler - AOS Mini Project

A dynamic CPU scheduling workbench with a **C backend** and an **HTML/CSS/JavaScript dashboard**. It is designed for a Windows-first classroom demonstration and uses only the C standard library plus the operating system's socket API. No database server, Node.js, Python runtime, or third-party C package is needed to run the application.

## What is included

- A C HTTP server that exposes the job queue, scheduling, recommendation, reset, and health APIs.
- A responsive dashboard with a live job queue, job creation and removal, Gantt chart, per-job metrics, and a six-policy benchmark lab.
- Six scheduling algorithms: FCFS, SJF, SRTF, Priority, Round Robin, and Earliest Deadline First (EDF).
- Local persistence in `data/jobs.csv` so queue changes survive an application restart.
- Windows build and run scripts, an alternative port script, a Linux launch script, and an API smoke test.
- A university-style project report with summary, design, formulas, output screenshots, results, test cases, and a GitHub URL placeholder.

## Quick start on Windows

1. Extract the ZIP file into a normal folder, for example `Desktop\SmartJobScheduler`. Do not run it from inside the ZIP preview.
2. Install MSYS2 from [msys2.org](https://www.msys2.org). Open the **MSYS2 UCRT64** terminal from the Start menu and run `pacman -S mingw-w64-ucrt-x86_64-gcc`; confirm the install when prompted.
3. Add the MSYS2 compiler folder (normally `C:\msys64\ucrt64\bin`) to the Windows PATH. Open a NEW **Command Prompt** and run `gcc --version` to verify it.
4. Double-click `run.bat`.
5. The script compiles the C server (first run only), opens `http://localhost:8080`, and keeps the backend running in a black Command Prompt window.
6. Use the dashboard. Keep the Command Prompt window open while the website is in use.
7. To stop the project, return to the Command Prompt window and press `Ctrl+C`.

### If port 8080 is already in use

Double-click `run_8081.bat`; the dashboard will open at `http://localhost:8081`.

### If `run.bat` says GCC was not found

Install MinGW-w64 GCC (for example through MSYS2's UCRT64 environment), add its `bin` directory to the Windows PATH, close all old Command Prompt windows, open a fresh one, and verify `gcc --version`. Then run `run.bat` again. The C source is portable; the Windows build links Winsock with `-lws2_32`. The server binds to 127.0.0.1 (localhost), so it is intended for local use on the same computer.

## Linux / macOS (optional)

A C compiler is required. From a terminal in the extracted project folder, run:

```bash
chmod +x run_linux.sh
./run_linux.sh
```

Then open `http://localhost:8080`. On macOS, use the installed Clang toolchain if `gcc` is not available by changing the compile command in `run_linux.sh` from `gcc` to `clang`.

## Dashboard features

- Add jobs with task name, arrival time, CPU burst, priority (1 is highest), and deadline.
- Remove individual jobs and restore the seven-job demonstration workload.
- Select one of six CPU scheduling policies and execute it.
- Round Robin exposes a configurable time quantum from 1 to 50.
- Inspect the Gantt timeline, completion times, waiting times, turnaround times, and response times.
- Use Smart Recommend to receive a workload-based policy recommendation.
- Run the Benchmark Lab to compare all six policies on the same queue.

## Scheduling definitions and formulas

Let `AT` be arrival time, `BT` be CPU burst time, `ST` be first start time, and `CT` be completion time.

- Turnaround Time (TAT) = `CT - AT`
- Waiting Time (WT) = `TAT - BT`
- Response Time (RT) = `ST - AT`
- CPU Utilization = `(sum of CPU burst times / makespan) x 100`
- Throughput = `number of completed jobs / makespan`

The dashboard labels time values as simulation time units. They are not wall-clock milliseconds.

## API endpoints

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/health` | Backend status and job count |
| GET | `/api/jobs` | Read the current job queue |
| POST | `/api/jobs` | Add one job |
| DELETE | `/api/jobs?id=JOB_ID` | Remove a job |
| POST | `/api/schedule` | Run a scheduling algorithm |
| GET | `/api/recommend` | Return a workload-based recommendation |
| POST | `/api/reset` | Restore the original demonstration queue |

## Project structure

```text
SmartJobScheduler/
├── src/server.c                 C backend, HTTP server, scheduler engine
├── web/index.html               Dashboard structure
├── web/styles.css               Responsive dashboard styling
├── web/app.js                   Frontend interactions and visualizations
├── data/jobs.csv                Persistent job data (seeded demo queue)
├── screenshots/                 Captured dashboard and benchmark outputs
├── docs/                         Project report and source appendix
├── tests/smoke_test.py           API and scheduling checks
├── compile.bat                   Build the Windows executable
├── run.bat                       Compile if needed and start on port 8080
├── run_8081.bat                  Alternative port launcher
└── run_linux.sh                  Linux/macOS launcher
```

## Testing

With the server running, use a second terminal in the project directory:

```bash
python tests/smoke_test.py
```

The test restores the demo queue when it finishes. It checks the health and static routes, all six scheduling policies, basic job validation, add/remove behavior, and reset behavior.

## Notes and limitations

- The backend is a small, single-process, sequential HTTP server intended for local academic demonstrations, not public Internet deployment.
- Up to 100 jobs can be stored. CPU burst and arrival/deadline values are validated by the C backend.
- Priority values range from 1 (highest) to 10 (lowest).
- EDF is implemented as a non-preemptive earliest-deadline-first policy; SRTF and Round Robin are preemptive.
- The data file is intentionally plain CSV for easy inspection and portability. Avoid editing it while the server is running.
- The GitHub repository field in the report is a placeholder. Create your own repository and paste its URL there before submission.

## Academic submission checklist

- [ ] Add student name, roll number, class, college, and teacher details to the report cover page.
- [ ] Run the application on your own Windows computer and capture your own screenshots if your teacher requires machine-specific evidence.
- [ ] Run the smoke test and retain the console output.
- [ ] Create your GitHub repository, upload the project files, and paste the repository URL into the report.
- [ ] Review the report and follow your college's formatting / declaration requirements.
