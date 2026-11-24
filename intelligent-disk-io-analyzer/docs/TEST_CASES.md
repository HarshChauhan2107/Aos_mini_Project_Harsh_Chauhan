# Test Cases

Use the browser interface or run `python tests/test_api.py` while the C server is active.

| Test | Input / action | Expected behaviour |
|---|---|---|
| 1. Sample comparison | Head 53, range 0–199, direction Right, queue `98, 183, 37, 122, 14, 124, 65, 67`, algorithm ALL | Six algorithm results appear; FCFS total movement is 640 cylinders; each algorithm has a movement path and breakdown. |
| 2. FCFS only | Same sample queue, choose FCFS | Only FCFS appears in results; service order remains the same as the input queue. |
| 3. SSTF | Same sample queue, choose SSTF | Requests are selected by nearest current head position; equal-distance ties preserve input order. |
| 4. Left direction | Use sample queue and choose Left | SCAN-family algorithms service the lower-side requests first. |
| 5. Head on request | Set initial head to 65 and include 65 in the queue | The request at the initial head has zero movement and is serviced at the start by SCAN-family algorithms. |
| 6. Duplicate requests | Queue `40, 40, 60`, head 50 | Duplicate requests are both serviced and both counted. |
| 7. Invalid request | Put a value above the disk maximum in the queue | The API rejects the request and the frontend shows an error. |
| 8. Invalid disk range | Set minimum equal to maximum | The API rejects the disk configuration. |
| 9. Empty queue | Clear the request queue and run | The frontend requests at least one disk request. |
| 10. Random queue | Press Generate random queue and then Run analysis | A queue and head position within the selected disk range are generated and can be simulated. |
| 11. Clear | Press Clear inputs | The input queue and head are cleared and the results panel returns to its initial state. |
| 12. Health check | Open `/api/health` in the browser | A JSON response with status `ok` is returned. |

## Expected sample FCFS calculation

Initial head: 53  
Requests: 98, 183, 37, 122, 14, 124, 65, 67

`|98-53| + |183-98| + |37-183| + |122-37| + |14-122| + |124-14| + |65-124| + |67-65|`

`45 + 85 + 146 + 85 + 108 + 110 + 59 + 2 = 640 cylinders`

Average movement = `640 / 8 = 80.00 cylinders per request`.
