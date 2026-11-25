#!/usr/bin/env python3
"""Standard-library smoke tests for the Smart Job Scheduler HTTP API."""
import json
import sys
import urllib.error
import urllib.request

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
BASE = f"http://127.0.0.1:{PORT}"
EXPECTED = {
    "FCFS": (10.29, 15.00, 10.29),
    "SJF": (7.71, 12.43, 7.71),
    "SRTF": (7.14, 11.86, 5.29),
    "PRIORITY": (8.86, 13.57, 8.86),
    "RR": (13.00, 17.71, 4.29),
    "EDF": (8.57, 13.29, 8.57),
}
passed = 0


def request(path, method="GET", payload=None):
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        BASE + path,
        data=data,
        method=method,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=5) as response:
        raw = response.read().decode("utf-8")
        return response.status, json.loads(raw) if raw else {}


def check(condition, label):
    global passed
    if not condition:
        raise AssertionError(label)
    passed += 1
    print(f"[PASS] {label}")


def main():
    # Restore known fixtures before comparing deterministic scheduling outputs.
    request("/api/reset", "POST", {})
    status, health = request("/api/health")
    check(status == 200 and health.get("status") == "ok" and health.get("backend") == "C",
          "C backend health endpoint")

    status, jobs = request("/api/jobs")
    check(status == 200 and jobs.get("count") == 7 and len(jobs["jobs"]) == 7,
          "seven seeded jobs are loaded")

    with urllib.request.urlopen(BASE + "/", timeout=5) as response:
        page = response.read().decode("utf-8")
        static_ok = response.status == 200 and "Smart Job Scheduler" in page
    check(static_ok, "root dashboard route responds with the interface")

    for algorithm, expected in EXPECTED.items():
        status, result = request("/api/schedule", "POST", {"algorithm": algorithm, "quantum": 2})
        metrics = result["metrics"]
        actual = (metrics["avgWaiting"], metrics["avgTurnaround"], metrics["avgResponse"])
        check(status == 200 and result["algorithm"] == algorithm and len(result["jobs"]) == 7,
              f"{algorithm} returns seven per-job results")
        check(all(abs(a - b) < 0.011 for a, b in zip(actual, expected)),
              f"{algorithm} average metrics match reference values")
        check(len(result["segments"]) >= 7 and metrics["makespan"] == 33,
              f"{algorithm} returns a valid Gantt timeline")

    status, recommendation = request("/api/recommend")
    check(status == 200 and recommendation["algorithm"] in EXPECTED,
          "adaptive recommendation returns a supported algorithm")

    status, added = request("/api/jobs", "POST", {
        "name": "Smoke Test Task", "arrival": 12, "burst": 2, "priority": 2, "deadline": 25
    })
    check(status == 201 and added.get("id", 0) > 0, "job can be added")
    status, updated = request("/api/jobs")
    check(updated["count"] == 8, "added job is visible in the persisted queue")
    request(f"/api/jobs?id={added['id']}", "DELETE")
    status, restored = request("/api/jobs")
    check(restored["count"] == 7, "job can be removed")

    try:
        request("/api/jobs", "POST", {"name": "Invalid", "arrival": -1, "burst": 0, "priority": 99, "deadline": -1})
        raise AssertionError("invalid job was not rejected")
    except urllib.error.HTTPError as exc:
        check(exc.code == 400, "invalid job values are rejected with HTTP 400")

    # Exercise the idle-CPU branch with a task that arrives after time zero.
    request("/api/reset", "POST", {})
    _, seeded = request("/api/jobs")
    for item in seeded["jobs"]:
        request(f"/api/jobs?id={item['id']}", "DELETE")
    status, delayed = request("/api/jobs", "POST", {
        "name": "Delayed Arrival Task", "arrival": 5, "burst": 2, "priority": 3, "deadline": 15
    })
    check(status == 201 and delayed.get("id", 0) > 0, "delayed-arrival edge case can be created")
    _, idle_result = request("/api/schedule", "POST", {"algorithm": "FCFS", "quantum": 2})
    check(idle_result["metrics"]["makespan"] == 7 and abs(idle_result["metrics"]["cpuUtilization"] - 28.57) < 0.02,
          "idle gap is included correctly in makespan and CPU utilization")
    check(idle_result["segments"][0]["jobId"] == 0 and idle_result["segments"][0]["start"] == 0 and idle_result["segments"][0]["end"] == 5,
          "Gantt timeline explicitly displays a CPU idle segment")

    request("/api/reset", "POST", {})
    status, final = request("/api/jobs")
    check(status == 200 and final["count"] == 7, "demo data is restored at test completion")
    print(f"\nRESULT: {passed} checks passed. Port: {PORT}.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"\n[FAIL] {exc}")
        print("Confirm the C server is running and the port number is correct.")
        sys.exit(1)
