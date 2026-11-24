# Viva Questions and Answers

## 1. What is disk scheduling?
Disk scheduling is the method used by an operating system to decide the order in which pending disk I/O requests are serviced.

## 2. Why is disk scheduling required?
Multiple processes may request data from different disk locations. A suitable service order can reduce head movement and improve I/O performance.

## 3. What is seek time?
Seek time is the time required for a disk's read/write head to move to the track or cylinder containing the required data. This simulator uses cylinder movement as a simplified measure; it does not measure real hardware time.

## 4. What is head movement?
Head movement is the distance travelled by the disk head from one cylinder position to another. In this project, movement from `a` to `b` is `|b - a|`.

## 5. Explain FCFS.
First Come First Serve processes the requests in their arrival order. It is simple and fair in arrival order, but it can produce high head movement.

## 6. Explain SSTF.
Shortest Seek Time First selects the pending request closest to the current head position. It often reduces movement but can cause starvation of requests located far away.

## 7. Why can SSTF cause starvation?
If new or nearby requests repeatedly appear around the current head region, a request located far away may keep waiting because closer requests are selected first.

## 8. What is the SCAN algorithm?
SCAN moves the head in one direction, services requests along the way, reaches the disk boundary when requests remain on the opposite side, and then reverses direction. It is also called the elevator algorithm.

## 9. What is C-SCAN?
Circular SCAN services requests in one direction. At the end of the disk, it returns to the opposite end and continues in the same direction.

## 10. What is the difference between SCAN and C-SCAN?
SCAN reverses direction and services requests in both directions. C-SCAN services requests in one direction and wraps to the opposite end to start another sweep.

## 11. What is LOOK?
LOOK is like SCAN, but the head reverses at the last pending request in the current direction instead of travelling to the physical disk boundary.

## 12. What is C-LOOK?
C-LOOK is like C-SCAN, but it moves only between the furthest pending requests. It jumps from one end of the pending request set to the other.

## 13. What is the difference between SCAN and LOOK?
SCAN may travel to the physical disk boundary before reversing. LOOK reverses at the last request that needs service in that direction, avoiding unnecessary boundary travel.

## 14. What is the difference between C-SCAN and C-LOOK?
C-SCAN travels to the disk boundary and wraps to the opposite boundary. C-LOOK wraps between the furthest pending requests and does not need to visit unused disk boundaries.

## 15. How is total head movement calculated in this project?
The C backend adds the absolute difference between the current cylinder and each next cylinder in the generated path. Boundary travel and circular wrap movement are included.

## 16. How is average seek movement calculated?
Average seek movement is calculated as total head movement divided by the number of requests in the queue.

## 17. Which algorithm is best?
There is no single algorithm that is best for every workload. The result depends on the request queue, initial head position, direction, and the chosen performance measure. The project highlights the lowest total movement for the current input.

## 18. What is the time complexity of FCFS?
FCFS takes O(n) time to service n requests because it processes the queue once.

## 19. What is the time complexity of SSTF in this project?
The current implementation takes O(n²) time because it repeatedly scans all unserviced requests to find the closest one.

## 20. Why are sorting algorithms used in SCAN, C-SCAN, LOOK, and C-LOOK?
These algorithms service requests in cylinder order. Sorting the request groups makes it straightforward to visit them in ascending or descending order.

## 21. What data structures are used in the project?
The project uses arrays for request queues, service order, and head paths; a Boolean array for tracking completed SSTF requests; and C structures for recording schedules and individual movement steps.

## 22. What is the role of the C backend?
The backend receives the frontend's input, validates it, runs the selected scheduling algorithms, calculates movement metrics, and returns a JSON response.

## 23. Why is a frontend used in a C project?
The frontend makes the simulator interactive and easier to demonstrate. The actual disk scheduling calculations remain implemented in C; JavaScript is used for form interaction, API communication, and visualization.

## 24. What is an API?
An Application Programming Interface defines how software components communicate. In this project, the browser sends a JSON request to the C server's `/api/analyze` endpoint and receives JSON results.

## 25. Does this project perform real disk I/O?
No. It simulates the order of disk requests and calculates cylinder movement. It does not read from or write to a physical disk device.

## 26. Does the project include rotational latency?
No. It focuses on head movement and uses it as a simplified performance metric. Rotational latency and data-transfer time are not modelled.

## 27. What happens if a request is outside the disk range?
The C backend rejects the input and returns an error message instead of running the simulation.

## 28. Why does the simulator need the initial direction?
SCAN, C-SCAN, LOOK, and C-LOOK need an initial direction to determine which side of the head should be serviced first. FCFS and SSTF do not depend on this setting.

## 29. Why is the initial head included in the path but not in the request count?
The initial head is the starting location, not a pending I/O request. It is needed to calculate the first movement but must not be counted as a serviced request.

## 30. What improvements can be made in the future?
The simulator can be extended with priority-based scheduling, deadline-based scheduling, latency estimation, animation, CSV/PDF export, simulation history, and multi-disk workloads.
