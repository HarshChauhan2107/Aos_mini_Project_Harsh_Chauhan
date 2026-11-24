# MINI PROJECT REPORT

## INTELLIGENT DISK I/O SCHEDULING ANALYZER

**Subject:** Advanced Operating Systems  
**Project type:** Mini Project  
**Implementation:** C (backend and scheduling algorithms), HTML, CSS, JavaScript (frontend)

---

## 1. Abstract

Disk I/O scheduling is an important operating-system activity that determines the order in which pending disk read and write requests are serviced. When several processes request access to different disk cylinders, the order of service affects head movement, seek time, and overall storage performance. This mini project, **Intelligent Disk I/O Scheduling Analyzer**, is an interactive simulator designed to demonstrate and compare commonly used disk scheduling algorithms.

The project implements FCFS, SSTF, SCAN, C-SCAN, LOOK, and C-LOOK in the C programming language. A C-based local HTTP server accepts the disk configuration and request queue from a browser interface, executes the selected scheduling algorithm, and returns the calculated results. The frontend displays the service sequence, total head movement, average seek movement, an algorithm comparison table, and a graphical representation of the disk-head path. Users can enter their own request queues, load a predefined example, or generate random requests.

The project helps learners understand how different scheduling policies behave on the same input and how their movement costs can vary.

## 2. Introduction

A magnetic disk stores data on tracks and sectors. The operating system receives I/O requests from multiple processes, and these requests may refer to different disk locations. The disk scheduling mechanism decides which request should be handled next. A poor service order can cause repeated long movements of the disk head, increasing access delay and reducing performance.

Disk scheduling algorithms attempt to arrange requests in a way that improves service efficiency while maintaining acceptable fairness. There is no single policy that is best for every workload. For example, FCFS is easy to implement and preserves arrival order, whereas SSTF generally favours nearby requests. SCAN-based algorithms move the head in an ordered sweep, which can provide more predictable service than always selecting the nearest request.

This project provides a practical environment in which students can change the initial head position, disk limits, request sequence, and sweep direction and then observe the results immediately.

## 3. Problem Statement

Manually calculating disk-head movement for multiple scheduling algorithms is time-consuming and can lead to calculation errors, particularly when a request queue contains several cylinder positions. Students also need a clear way to compare the service order and understand the movement performed by each algorithm.

The problem addressed by this project is to develop an interactive system that accepts a disk request queue, simulates different disk I/O scheduling algorithms, calculates their head movement, and presents the results in an understandable form.

## 4. Objectives

1. To understand the role of disk scheduling in an operating system.
2. To implement six disk scheduling algorithms using C.
3. To accept a user-defined initial head position and disk request queue.
4. To support the disk range and initial scan direction as configurable inputs.
5. To calculate total head movement and average movement per request.
6. To compare the performance of different scheduling algorithms for the same workload.
7. To display the request service order and the complete head movement path.
8. To develop an interactive graphical interface for experimentation and learning.
9. To validate input and handle invalid cylinder positions and configuration values.

## 5. Scope of the Project

The simulator focuses on the head-movement behaviour of six classical disk scheduling policies. It is designed for educational use in an Advanced Operating Systems laboratory. The user can configure the disk geometry and request queue and can observe the effect of changing the initial head position or scan direction.

The project calculates movement-based metrics; it does not access a physical disk, issue real operating-system block I/O commands, measure hardware latency, model rotational latency, or emulate a complete kernel I/O subsystem. The displayed movement values are simulation results based on cylinder distances.

## 6. System Requirements

### 6.1 Hardware requirements

- Desktop or laptop computer.
- Minimum 2 GB RAM recommended.
- Keyboard, mouse, and display.

### 6.2 Software requirements

- Windows 10/11 or Linux/macOS.
- GCC compiler (MinGW-w64 on Windows).
- Modern web browser.
- Terminal or command prompt.

### 6.3 Technologies used

| Component | Technology | Purpose |
|---|---|---|
| Scheduling engine | C | Implements all six algorithms and calculates movement |
| Backend server | C sockets | Receives HTTP requests and returns JSON results |
| Frontend structure | HTML5 | Provides forms, result panels, and tables |
| Frontend styling | CSS3 | Provides responsive layout and visual design |
| Frontend interaction | JavaScript | Sends API requests, updates results, and draws charts |
| Visualization | HTML Canvas | Plots head position against movement steps |

## 7. System Architecture

The project follows a simple client-server architecture.

```text
+-------------------------------+
|          USER                 |
| Enters disk and queue details |
+---------------+---------------+
                |
                v
+-------------------------------+
|      FRONTEND (Browser)       |
| HTML + CSS + JavaScript       |
| Forms, controls, chart, table |
+---------------+---------------+
                | HTTP POST /api/analyze
                | JSON input
                v
+-------------------------------+
|        C HTTP SERVER          |
| Socket handling and routing  |
| Input parsing and validation  |
+---------------+---------------+
                |
                v
+-------------------------------+
|    C SCHEDULING ENGINE        |
| FCFS / SSTF / SCAN / C-SCAN   |
| LOOK / C-LOOK                 |
+---------------+---------------+
                |
                v
+-------------------------------+
|       METRIC CALCULATION      |
| Service order, path, steps,   |
| total movement, average seek  |
+---------------+---------------+
                | JSON response
                v
+-------------------------------+
|      FRONTEND RESULTS         |
| Comparison table and chart    |
+-------------------------------+
```

### 7.1 Frontend module

The frontend is the part of the application with which the user interacts. It contains input fields for the initial head position, minimum and maximum cylinder values, scan direction, request queue, and algorithm selection. It also provides sample-data, random-data, and clear-input controls.

After the user selects **Run analysis**, JavaScript validates the basic form values and sends them to the C backend using an HTTP POST request. It then displays the returned results in the performance overview, comparison table, sequence panel, and chart.

### 7.2 Backend module

The backend is written in C. It creates a local TCP socket server, accepts browser requests, identifies the requested endpoint, reads the JSON body, and validates the input. The backend then calls the scheduling engine and constructs a JSON response containing the simulation results.

The server listens on the loopback address `127.0.0.1`, which means it is intended to run locally on the student's computer.

### 7.3 Scheduling engine module

The scheduling engine contains separate functions for FCFS, SSTF, and the SCAN-family algorithms. Each function builds a service order and records every movement made by the head. A common movement function calculates the absolute distance between the current cylinder and the next cylinder and updates the total movement.

### 7.4 Result and visualization module

The frontend uses the returned data to display the service sequence, head path, total movement, average seek movement, maximum single-step movement, and minimum single-step movement. A Canvas chart plots the cylinder position at each step. Boundary and wrap movements are distinguished from normal request-service movements.

## 8. Disk Scheduling Algorithms

Let the initial head position be `H`, and let the pending request queue contain `n` cylinder requests. The movement from cylinder `a` to cylinder `b` is calculated as:

**Movement = |b − a|**

The total head movement is the sum of the distances travelled during all recorded steps:

**Total Head Movement = Σ |current position − next position|**

The average seek movement used in this project is:

**Average Seek Movement = Total Head Movement / Number of Requests**

### 8.1 FCFS — First Come First Serve

FCFS services disk requests in the same order in which they are received. The head moves from its current position to the first request, then to the second request, and continues until all requests have been serviced.

**Advantages:** It is simple, easy to implement, and fair with respect to request arrival order.  
**Disadvantages:** It may cause large and unnecessary head movements because it does not consider the physical location of requests.  
**Time complexity:** O(n).

### 8.2 SSTF — Shortest Seek Time First

SSTF selects the pending request that is closest to the current head position. After servicing that request, it repeats the selection using the new head position. If two requests are at the same distance, this implementation selects the one that occurred first in the input queue.

**Advantages:** It often reduces total head movement compared with FCFS.  
**Disadvantages:** Requests located far from frequently selected regions may wait for a long time, resulting in possible starvation.  
**Time complexity:** O(n²) in this implementation because each selection scans the remaining requests.

### 8.3 SCAN — Elevator Algorithm

In SCAN, the disk head travels in the selected direction and services requests in cylinder order. When there are pending requests on the opposite side of the starting head position, the head continues to the corresponding physical disk boundary, reverses direction, and services the remaining requests.

**Advantages:** It provides orderly scanning and generally offers more predictable waiting behaviour than SSTF.  
**Disadvantages:** Requests may wait for the head to complete part of a sweep.  
**Time complexity:** O(n log n), because the requests are sorted before service.

### 8.4 C-SCAN — Circular SCAN

C-SCAN services requests in one direction. After reaching the relevant disk boundary, the head returns to the opposite boundary and continues servicing requests in the original direction. The return movement is recorded as a circular wrap step in this simulator.

**Advantages:** It provides a more uniform service pattern across cylinder locations in many workloads.  
**Disadvantages:** The return to the opposite end adds movement that does not service requests.  
**Time complexity:** O(n log n).

### 8.5 LOOK

LOOK is similar to SCAN, but the head does not travel all the way to the physical disk boundary if no request is present there. It reverses direction at the last pending request in the current direction and then services requests on the other side.

**Advantages:** It avoids unnecessary travel to an unused disk boundary.  
**Disadvantages:** Its service delay still depends on the position of the head and the direction of the sweep.  
**Time complexity:** O(n log n).

### 8.6 C-LOOK — Circular LOOK

C-LOOK is similar to C-SCAN, but it moves only as far as the last pending request in the current direction. It then jumps to the first pending request at the opposite end of the request set and continues in the same direction. The jump is included in the movement calculation.

**Advantages:** It avoids unnecessary travel to the physical disk boundaries while retaining circular service order.  
**Disadvantages:** The circular jump still contributes to total movement.  
**Time complexity:** O(n log n).

## 9. Algorithm Workflow

1. Start the C server and open the frontend in a browser.
2. Read the disk configuration and request queue from the user.
3. Validate that the disk minimum is smaller than the maximum.
4. Validate that the initial head and every request are within the configured disk range.
5. Separate requests into positions below, equal to, and above the initial head when a directional scan is required.
6. Sort the lower and upper request groups for SCAN, C-SCAN, LOOK, and C-LOOK.
7. Execute the selected algorithm or execute all six algorithms for comparison.
8. Record the service order and every head movement.
9. Calculate total movement and average movement per request.
10. Return the results to the frontend as JSON.
11. Display the comparison and movement chart.

## 10. Data Structures Used

- **Integer arrays:** Store the incoming request queue, sorted lower and upper partitions, service order, and movement path.
- **Boolean array:** In SSTF, records whether a request has already been serviced.
- **Structure (`struct`):** Stores each scheduling result and the details of each movement step.
- **Character arrays:** Store the incoming JSON request, algorithm name, direction, and generated JSON response.

The use of arrays and structures makes the scheduling data easy to manage and allows a common result format for all algorithms.

## 11. Input and Output Description

### 11.1 Input

| Input | Description | Sample value |
|---|---|---:|
| Initial head | Starting disk-head cylinder | 53 |
| Disk minimum | First available cylinder | 0 |
| Disk maximum | Last available cylinder | 199 |
| Direction | Initial direction for scan-based algorithms | Right |
| Request queue | Pending cylinder requests | 98, 183, 37, 122, 14, 124, 65, 67 |
| Algorithm | Selected scheduling method | All algorithms |

### 11.2 Output

The application displays:

- Algorithm name and service order.
- Full head movement path, including the initial position.
- Distance travelled in each movement step.
- Movement type: request service, disk boundary, or circular wrap.
- Total head movement in cylinders.
- Average movement per request.
- Maximum and minimum movement in a single step.
- A comparison of the selected algorithms and the lowest-movement result for that input.

## 12. Sample Demonstration

For the sample queue:

- Initial head position: 53
- Disk range: 0–199
- Direction: Right
- Request queue: 98, 183, 37, 122, 14, 124, 65, 67

The FCFS service order is the same as the input queue:

`98 → 183 → 37 → 122 → 14 → 124 → 65 → 67`

The FCFS head path is:

`53 → 98 → 183 → 37 → 122 → 14 → 124 → 65 → 67`

The resulting total head movement is 640 cylinders, and average head movement is 80.00 cylinders per request.

The application calculates the values for all six algorithms directly from the selected input. The best algorithm can change when the initial head position, direction, or request queue changes; therefore, the comparison is workload-dependent rather than a fixed ranking.

## 13. Testing and Validation

The project includes validation for missing fields, invalid numbers, empty queues, request queues that exceed 128 entries, invalid directions, disk limits, and requests outside the disk range. The API test script checks the health endpoint, a sample calculation, selected-algorithm operation, and invalid request handling.

Detailed manual cases are provided in `TEST_CASES.md`.

## 14. Advantages

1. Supports six major disk scheduling methods in one application.
2. Provides an interactive interface instead of requiring repeated manual calculations.
3. Allows the same queue to be compared across different algorithms.
4. Displays both numerical metrics and the actual order of head movement.
5. Makes the impact of initial direction and disk boundaries easy to observe.
6. Uses C for the core scheduling logic, which is suitable for an Operating Systems project.
7. Separates the frontend from the backend, making the project easier to maintain and extend.

## 15. Limitations

1. The application is a simulator and does not control a real disk device.
2. It calculates cylinder movement but does not include rotational latency, transfer time, controller overhead, or hardware-specific seek curves.
3. The C server handles client connections sequentially, which is sufficient for a local educational demonstration but not intended for production traffic.
4. SSTF can cause starvation for requests that remain far from the current head position.
5. Results depend on the stated movement conventions, especially for boundary and circular wrap handling.

## 16. Future Enhancements

- Add priority-based and deadline-based I/O scheduling.
- Add workload presets for sequential, random, and clustered requests.
- Include estimated seek-time models and configurable rotational latency.
- Add animated head movement and a track/cylinder map.
- Save simulation history and export results to CSV or PDF.
- Add multi-queue and multi-disk simulation.
- Add a threaded server if concurrent multi-user access is required.

## 17. Conclusion

The Intelligent Disk I/O Scheduling Analyzer demonstrates the working of six classical disk scheduling algorithms through an interactive simulation. The C backend performs the scheduling and movement calculations, while the web frontend provides a convenient way to enter requests, compare policies, and understand the service sequence visually. The project shows that different algorithms can produce different movement costs for the same workload and that algorithm performance depends on the request distribution, starting head position, and scan direction.

The project meets its educational objective of making disk I/O scheduling easier to simulate, analyze, and compare as part of the Advanced Operating Systems subject.

## 18. References

1. Abraham Silberschatz, Peter B. Galvin, and Greg Gagne, *Operating System Concepts*, sections on mass-storage structure and disk scheduling.
2. William Stallings, *Operating Systems: Internals and Design Principles*, storage management topics.
3. Andrew S. Tanenbaum and Herbert Bos, *Modern Operating Systems*, I/O management topics.
4. Course notes and classroom material for Advanced Operating Systems.
