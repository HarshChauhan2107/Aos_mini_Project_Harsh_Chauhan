"""Small smoke-test client for the C API. Start disk_analyzer before running."""
import json
import os
import sys
import urllib.error
import urllib.request

BASE = os.environ.get("DISKSCOPE_URL", "http://127.0.0.1:8080")


def get_json(path):
    with urllib.request.urlopen(BASE + path, timeout=3) as response:
        return json.loads(response.read().decode("utf-8"))


def post_json(path, payload):
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        BASE + path, data=data, headers={"Content-Type": "application/json"}, method="POST"
    )
    try:
        with urllib.request.urlopen(request, timeout=3) as response:
            return response.status, json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        return exc.code, json.loads(exc.read().decode("utf-8"))


def main():
    health = get_json("/api/health")
    assert health.get("status") == "ok", health
    print("PASS: health endpoint")
    for asset in ("/", "/style.css", "/app.js"):
        with urllib.request.urlopen(BASE + asset, timeout=3) as response:
            assert response.status == 200 and len(response.read()) > 100
    print("PASS: frontend assets are served by the C server")

    sample = {
        "head": 53,
        "disk_min": 0,
        "disk_max": 199,
        "direction": "right",
        "algorithm": "ALL",
        "requests": [98, 183, 37, 122, 14, 124, 65, 67],
    }
    status, result = post_json("/api/analyze", sample)
    assert status == 200, result
    by_name = {item["algorithm"]: item for item in result["results"]}
    assert len(by_name) == 6, by_name.keys()
    assert by_name["FCFS"]["total_movement"] == 640, by_name["FCFS"]
    assert by_name["FCFS"]["average_seek"] == 80.0, by_name["FCFS"]
    assert len(by_name["FCFS"]["service_order"]) == 8
    print("PASS: six algorithms and FCFS sample calculation")

    sample["algorithm"] = "SSTF"
    status, result = post_json("/api/analyze", sample)
    assert status == 200 and len(result["results"]) == 1
    assert result["results"][0]["algorithm"] == "SSTF"
    print("PASS: single-algorithm selection")

    sample["algorithm"] = "C-LOOK"
    status, result = post_json("/api/analyze", sample)
    circular_look = result["results"][0]
    assert status == 200 and circular_look["service_order"] == [65, 67, 98, 122, 124, 183, 14, 37]
    assert circular_look["path"][-2:] == [14, 37]
    print("PASS: C-LOOK circular jump and service sequence")

    invalid = dict(sample)
    invalid["requests"] = [200]
    status, result = post_json("/api/analyze", invalid)
    assert status == 400 and "error" in result
    print("PASS: out-of-range request validation")

    print("\nAll API smoke tests passed.")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"TEST FAILED: {exc}", file=sys.stderr)
        sys.exit(1)
