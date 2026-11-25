/*
 * Smart Job Scheduler - C backend + embedded HTTP server
 * Portable: Windows (Winsock) and Linux/macOS (POSIX sockets)
 * No third-party C libraries required.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET socket_t;
  #define CLOSESOCKET closesocket
  #define SOCKET_ERR SOCKET_ERROR
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  typedef int socket_t;
  #define CLOSESOCKET close
  #define SOCKET_ERR (-1)
#endif

#define PORT_DEFAULT 8080
#define MAX_JOBS 100
#define MAX_NAME 64
#define MAX_REQUEST 131072
#define MAX_JSON 4194304
#define MAX_SEGMENTS 30000
#define DATA_FILE "data/jobs.csv"

typedef struct {
    int id;
    char name[MAX_NAME];
    int arrival;
    int burst;
    int priority; /* 1 is the highest priority */
    int deadline;
} Job;

typedef struct {
    int job_id; /* 0 means CPU idle */
    char name[MAX_NAME];
    int start;
    int end;
} Segment;

typedef struct {
    Job job;
    int remaining;
    int first_start;
    int completion;
} SimJob;

static Job g_jobs[MAX_JOBS];
static int g_job_count = 0;
static int g_next_id = 1;
static const Job SEED_JOBS[] = {
    {1, "API Gateway",       0, 7, 2, 16},
    {2, "Payment Worker",    2, 4, 1, 12},
    {3, "Notification Hub",  4, 3, 3, 14},
    {4, "Analytics Batch",   5, 8, 4, 28},
    {5, "Cache Refresh",     6, 2, 2, 11},
    {6, "Report Builder",    8, 5, 3, 22},
    {7, "Backup Agent",     10, 4, 5, 30}
};

static void appendf(char *dst, size_t cap, size_t *used, const char *fmt, ...) {
    if (*used >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(dst + *used, cap - *used, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= cap - *used) *used = cap - 1;
    else *used += (size_t)n;
}

static void json_escape(const char *src, char *dst, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < cap; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = (char)c; }
        else if (c == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
        else if (c == '\r') { dst[j++] = '\\'; dst[j++] = 'r'; }
        else if (c == '\t') { dst[j++] = '\\'; dst[j++] = 't'; }
        else if (c >= 32) dst[j++] = (char)c;
    }
    dst[j] = '\0';
}

static void sanitize_name(char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start])) start++;
    if (start) memmove(s, s + start, strlen(s + start) + 1);
    for (size_t i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == ',' || c == '|' || c == '\n' || c == '\r' || c < 32) s[i] = ' ';
    }
}

static void seed_jobs(void) {
    g_job_count = (int)(sizeof(SEED_JOBS) / sizeof(SEED_JOBS[0]));
    for (int i = 0; i < g_job_count; ++i) g_jobs[i] = SEED_JOBS[i];
    g_next_id = 1;
    for (int i = 0; i < g_job_count; ++i) if (g_jobs[i].id >= g_next_id) g_next_id = g_jobs[i].id + 1;
}

static void save_jobs(void) {
    FILE *f = fopen(DATA_FILE, "w");
    if (!f) return;
    fprintf(f, "id,name,arrival,burst,priority,deadline\n");
    for (int i = 0; i < g_job_count; ++i) {
        fprintf(f, "%d,%s,%d,%d,%d,%d\n", g_jobs[i].id, g_jobs[i].name,
                g_jobs[i].arrival, g_jobs[i].burst, g_jobs[i].priority, g_jobs[i].deadline);
    }
    fclose(f);
}

static void load_jobs(void) {
    FILE *f = fopen(DATA_FILE, "r");
    if (!f) {
        seed_jobs();
        save_jobs();
        return;
    }
    char line[256];
    g_job_count = 0;
    g_next_id = 1;
    (void)fgets(line, sizeof(line), f); /* header */
    while (fgets(line, sizeof(line), f) && g_job_count < MAX_JOBS) {
        Job j;
        memset(&j, 0, sizeof(j));
        if (sscanf(line, "%d,%63[^,],%d,%d,%d,%d", &j.id, j.name, &j.arrival,
                   &j.burst, &j.priority, &j.deadline) == 6) {
            g_jobs[g_job_count++] = j;
            if (j.id >= g_next_id) g_next_id = j.id + 1;
        }
    }
    fclose(f);
}

static void jobs_json(char *out, size_t cap) {
    size_t used = 0;
    out[0] = '\0';
    appendf(out, cap, &used, "{\"jobs\":[");
    for (int i = 0; i < g_job_count; ++i) {
        char esc[MAX_NAME * 2];
        json_escape(g_jobs[i].name, esc, sizeof(esc));
        appendf(out, cap, &used,
                "%s{\"id\":%d,\"name\":\"%s\",\"arrival\":%d,\"burst\":%d,\"priority\":%d,\"deadline\":%d}",
                i ? "," : "", g_jobs[i].id, esc, g_jobs[i].arrival, g_jobs[i].burst,
                g_jobs[i].priority, g_jobs[i].deadline);
    }
    appendf(out, cap, &used, "],\"count\":%d}", g_job_count);
}

static const char *json_value(const char *body, const char *key) {
    static char needle[96];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(body, needle);
    if (!p) return NULL;
    p = strchr(p + strlen(needle), ':');
    if (!p) return NULL;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int json_get_int(const char *body, const char *key, int fallback) {
    const char *p = json_value(body, key);
    if (!p || *p == 'n' || *p == '"') return fallback;
    char *end = NULL;
    long value = strtol(p, &end, 10);
    if (end == p || value < -1000000 || value > 1000000) return fallback;
    return (int)value;
}

static int json_get_string(const char *body, const char *key, char *dst, size_t cap) {
    const char *p = json_value(body, key);
    if (!p || *p != '"' || cap == 0) return 0;
    p++;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') dst[n++] = ' ';
            else if (*p == 't' || *p == 'r') dst[n++] = ' ';
            else dst[n++] = *p;
            p++;
        } else dst[n++] = *p++;
    }
    dst[n] = '\0';
    return (*p == '"');
}

static void add_segment(Segment *segments, int *count, int job_id, const char *name, int start, int end) {
    if (end <= start) return;
    if (*count > 0 && segments[*count - 1].job_id == job_id && segments[*count - 1].end == start) {
        segments[*count - 1].end = end;
        return;
    }
    if (*count >= MAX_SEGMENTS) return;
    Segment *s = &segments[(*count)++];
    s->job_id = job_id;
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->start = start;
    s->end = end;
}

static int choose_available(SimJob *sim, int n, const char *algorithm, int time) {
    int chosen = -1;
    for (int i = 0; i < n; ++i) {
        if (sim[i].completion >= 0 || sim[i].job.arrival > time) continue;
        if (chosen < 0) { chosen = i; continue; }
        Job *a = &sim[i].job, *b = &sim[chosen].job;
        int better = 0;
        if (strcmp(algorithm, "SJF") == 0) {
            better = (a->burst < b->burst) || (a->burst == b->burst && (a->arrival < b->arrival || (a->arrival == b->arrival && a->id < b->id)));
        } else if (strcmp(algorithm, "PRIORITY") == 0) {
            better = (a->priority < b->priority) || (a->priority == b->priority && (a->arrival < b->arrival || (a->arrival == b->arrival && a->id < b->id)));
        } else if (strcmp(algorithm, "EDF") == 0) {
            better = (a->deadline < b->deadline) || (a->deadline == b->deadline && (a->arrival < b->arrival || (a->arrival == b->arrival && a->id < b->id)));
        } else { /* FCFS */
            better = (a->arrival < b->arrival) || (a->arrival == b->arrival && a->id < b->id);
        }
        if (better) chosen = i;
    }
    return chosen;
}

static void schedule_non_preemptive(SimJob *sim, int n, const char *algorithm,
                                    Segment *segments, int *segment_count, int *makespan) {
    int time = 0, finished = 0;
    while (finished < n) {
        int pick = choose_available(sim, n, algorithm, time);
        if (pick < 0) {
            int next_arrival = 2147483647;
            for (int i = 0; i < n; ++i) if (sim[i].completion < 0 && sim[i].job.arrival < next_arrival) next_arrival = sim[i].job.arrival;
            if (next_arrival == 2147483647) break;
            if (next_arrival > time) {
                add_segment(segments, segment_count, 0, "CPU Idle", time, next_arrival);
                time = next_arrival;
            }
            continue;
        }
        if (sim[pick].first_start < 0) sim[pick].first_start = time;
        int start = time;
        time += sim[pick].job.burst;
        sim[pick].completion = time;
        sim[pick].remaining = 0;
        add_segment(segments, segment_count, sim[pick].job.id, sim[pick].job.name, start, time);
        finished++;
    }
    *makespan = time;
}

static void schedule_srtf(SimJob *sim, int n, Segment *segments, int *segment_count, int *makespan) {
    int time = 0, finished = 0;
    while (finished < n) {
        int pick = -1;
        for (int i = 0; i < n; ++i) {
            if (sim[i].completion >= 0 || sim[i].job.arrival > time || sim[i].remaining <= 0) continue;
            if (pick < 0 || sim[i].remaining < sim[pick].remaining ||
                (sim[i].remaining == sim[pick].remaining && (sim[i].job.arrival < sim[pick].job.arrival ||
                 (sim[i].job.arrival == sim[pick].job.arrival && sim[i].job.id < sim[pick].job.id)))) pick = i;
        }
        if (pick < 0) {
            int next_arrival = 2147483647;
            for (int i = 0; i < n; ++i) if (sim[i].completion < 0 && sim[i].job.arrival < next_arrival) next_arrival = sim[i].job.arrival;
            if (next_arrival == 2147483647) break;
            if (next_arrival > time) {
                add_segment(segments, segment_count, 0, "CPU Idle", time, next_arrival);
                time = next_arrival;
            }
            continue;
        }
        if (sim[pick].first_start < 0) sim[pick].first_start = time;
        add_segment(segments, segment_count, sim[pick].job.id, sim[pick].job.name, time, time + 1);
        sim[pick].remaining--;
        time++;
        if (sim[pick].remaining == 0) {
            sim[pick].completion = time;
            finished++;
        }
    }
    *makespan = time;
}

static void schedule_round_robin(SimJob *sim, int n, int quantum, Segment *segments,
                                 int *segment_count, int *makespan) {
    if (quantum < 1) quantum = 1;
    int order[MAX_JOBS], q[MAX_JOBS + 1], head = 0, tail = 0, next_arrival = 0;
    int time = 0, finished = 0;
    for (int i = 0; i < n; ++i) order[i] = i;
    for (int i = 1; i < n; ++i) {
        int v = order[i], j = i - 1;
        while (j >= 0 && (sim[order[j]].job.arrival > sim[v].job.arrival ||
               (sim[order[j]].job.arrival == sim[v].job.arrival && sim[order[j]].job.id > sim[v].job.id))) {
            order[j + 1] = order[j]; j--;
        }
        order[j + 1] = v;
    }
    int enqueued[MAX_JOBS] = {0};
    while (finished < n) {
        while (next_arrival < n && sim[order[next_arrival]].job.arrival <= time) {
            int ix = order[next_arrival++];
            if (!enqueued[ix] && sim[ix].remaining > 0) {
                q[tail] = ix; tail = (tail + 1) % (MAX_JOBS + 1); enqueued[ix] = 1;
            }
        }
        if (head == tail) {
            if (next_arrival < n) {
                int next_time = sim[order[next_arrival]].job.arrival;
                if (next_time > time) add_segment(segments, segment_count, 0, "CPU Idle", time, next_time);
                time = next_time;
                continue;
            }
            break;
        }
        int ix = q[head]; head = (head + 1) % (MAX_JOBS + 1); enqueued[ix] = 0;
        if (sim[ix].first_start < 0) sim[ix].first_start = time;
        int run = sim[ix].remaining < quantum ? sim[ix].remaining : quantum;
        add_segment(segments, segment_count, sim[ix].job.id, sim[ix].job.name, time, time + run);
        time += run;
        sim[ix].remaining -= run;
        /* Jobs that arrived during this time slice join before the preempted job. */
        while (next_arrival < n && sim[order[next_arrival]].job.arrival <= time) {
            int a = order[next_arrival++];
            if (!enqueued[a] && sim[a].remaining > 0) {
                q[tail] = a; tail = (tail + 1) % (MAX_JOBS + 1); enqueued[a] = 1;
            }
        }
        if (sim[ix].remaining == 0) {
            sim[ix].completion = time;
            finished++;
        } else {
            q[tail] = ix; tail = (tail + 1) % (MAX_JOBS + 1); enqueued[ix] = 1;
        }
    }
    *makespan = time;
}

static void schedule_response(const char *algorithm_in, int quantum, char *out, size_t cap) {
    char algorithm[24];
    snprintf(algorithm, sizeof(algorithm), "%s", algorithm_in);
    for (size_t i = 0; algorithm[i]; ++i) algorithm[i] = (char)toupper((unsigned char)algorithm[i]);
    if (strcmp(algorithm, "ROUND ROBIN") == 0 || strcmp(algorithm, "ROUND_ROBIN") == 0) strcpy(algorithm, "RR");
    if (strcmp(algorithm, "PRIORITY SCHEDULING") == 0) strcpy(algorithm, "PRIORITY");
    if (strcmp(algorithm, "SRTF") != 0 && strcmp(algorithm, "FCFS") != 0 && strcmp(algorithm, "SJF") != 0 &&
        strcmp(algorithm, "PRIORITY") != 0 && strcmp(algorithm, "EDF") != 0 && strcmp(algorithm, "RR") != 0) strcpy(algorithm, "FCFS");
    if (quantum < 1) quantum = 1;
    if (quantum > 50) quantum = 50;

    SimJob sim[MAX_JOBS];
    Segment *segments = (Segment *)calloc(MAX_SEGMENTS, sizeof(Segment));
    int segment_count = 0, makespan = 0;
    if (!segments) { snprintf(out, cap, "{\"error\":\"Not enough memory to build the schedule.\"}"); return; }
    for (int i = 0; i < g_job_count; ++i) {
        sim[i].job = g_jobs[i];
        sim[i].remaining = g_jobs[i].burst;
        sim[i].first_start = -1;
        sim[i].completion = -1;
    }
    if (strcmp(algorithm, "SRTF") == 0) schedule_srtf(sim, g_job_count, segments, &segment_count, &makespan);
    else if (strcmp(algorithm, "RR") == 0) schedule_round_robin(sim, g_job_count, quantum, segments, &segment_count, &makespan);
    else schedule_non_preemptive(sim, g_job_count, algorithm, segments, &segment_count, &makespan);

    double sum_wait = 0, sum_turn = 0, sum_resp = 0, total_burst = 0;
    for (int i = 0; i < g_job_count; ++i) {
        int turnaround = sim[i].completion >= 0 ? sim[i].completion - sim[i].job.arrival : 0;
        int waiting = turnaround - sim[i].job.burst;
        int response = sim[i].first_start >= 0 ? sim[i].first_start - sim[i].job.arrival : 0;
        if (waiting < 0) waiting = 0;
        sum_wait += waiting; sum_turn += turnaround; sum_resp += response; total_burst += sim[i].job.burst;
    }
    double avg_wait = g_job_count ? sum_wait / g_job_count : 0;
    double avg_turn = g_job_count ? sum_turn / g_job_count : 0;
    double avg_resp = g_job_count ? sum_resp / g_job_count : 0;
    double utilization = makespan > 0 ? (total_burst / makespan) * 100.0 : 0;
    double throughput = makespan > 0 ? (double)g_job_count / makespan : 0;

    size_t used = 0;
    out[0] = '\0';
    appendf(out, cap, &used, "{\"algorithm\":\"%s\",\"quantum\":%d,\"metrics\":{\"avgWaiting\":%.2f,\"avgTurnaround\":%.2f,\"avgResponse\":%.2f,\"cpuUtilization\":%.2f,\"throughput\":%.3f,\"makespan\":%d},\"jobs\":[",
            algorithm, quantum, avg_wait, avg_turn, avg_resp, utilization, throughput, makespan);
    for (int i = 0; i < g_job_count; ++i) {
        char esc[MAX_NAME * 2]; json_escape(sim[i].job.name, esc, sizeof(esc));
        int turnaround = sim[i].completion >= 0 ? sim[i].completion - sim[i].job.arrival : 0;
        int waiting = turnaround - sim[i].job.burst; if (waiting < 0) waiting = 0;
        int response = sim[i].first_start >= 0 ? sim[i].first_start - sim[i].job.arrival : 0;
        appendf(out, cap, &used,
            "%s{\"id\":%d,\"name\":\"%s\",\"arrival\":%d,\"burst\":%d,\"priority\":%d,\"deadline\":%d,\"start\":%d,\"completion\":%d,\"waiting\":%d,\"turnaround\":%d,\"response\":%d}",
            i ? "," : "", sim[i].job.id, esc, sim[i].job.arrival, sim[i].job.burst,
            sim[i].job.priority, sim[i].job.deadline, sim[i].first_start, sim[i].completion,
            waiting, turnaround, response);
    }
    appendf(out, cap, &used, "],\"segments\":[");
    for (int i = 0; i < segment_count; ++i) {
        char esc[MAX_NAME * 2]; json_escape(segments[i].name, esc, sizeof(esc));
        appendf(out, cap, &used, "%s{\"jobId\":%d,\"name\":\"%s\",\"start\":%d,\"end\":%d,\"duration\":%d}",
                i ? "," : "", segments[i].job_id, esc, segments[i].start, segments[i].end, segments[i].end - segments[i].start);
    }
    appendf(out, cap, &used, "]}");
    free(segments);
}

static void recommendation(char *out, size_t cap) {
    if (g_job_count == 0) { snprintf(out, cap, "{\"algorithm\":\"FCFS\",\"reason\":\"Add jobs first. FCFS is a simple, fair default for an empty queue.\"}"); return; }
    int min_priority = 9999, max_priority = -9999, tight_deadlines = 0;
    double avg = 0, variance = 0;
    for (int i = 0; i < g_job_count; ++i) {
        if (g_jobs[i].priority < min_priority) min_priority = g_jobs[i].priority;
        if (g_jobs[i].priority > max_priority) max_priority = g_jobs[i].priority;
        if (g_jobs[i].deadline - g_jobs[i].arrival <= g_jobs[i].burst + 2) tight_deadlines++;
        avg += g_jobs[i].burst;
    }
    avg /= g_job_count;
    for (int i = 0; i < g_job_count; ++i) { double d = g_jobs[i].burst - avg; variance += d * d; }
    variance /= g_job_count;
    if (tight_deadlines > 0) {
        snprintf(out, cap, "{\"algorithm\":\"EDF\",\"reason\":\"At least one task has a tight deadline. Earliest Deadline First puts deadline-sensitive work ahead of less urgent tasks.\"}");
    } else if (max_priority - min_priority >= 3) {
        snprintf(out, cap, "{\"algorithm\":\"PRIORITY\",\"reason\":\"The queue has a wide priority spread. Priority scheduling respects the urgency assigned to each job.\"}");
    } else if (g_job_count >= 5 && variance >= 4.0) {
        snprintf(out, cap, "{\"algorithm\":\"SRTF\",\"reason\":\"The queue contains several jobs with varied CPU bursts. SRTF can reduce average waiting time by favoring shorter remaining work.\"}");
    } else {
        snprintf(out, cap, "{\"algorithm\":\"SJF\",\"reason\":\"No exceptionally tight deadline or extreme priority spread was detected. SJF is a strong baseline for reducing average waiting time.\"}");
    }
}

static void send_all(socket_t client, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = send(client, data + sent, (int)(len - sent), 0);
        if (n <= 0) break;
        sent += (size_t)n;
    }
}

static void send_response(socket_t client, int status, const char *content_type, const char *body) {
    char header[512];
    const char *status_text = status == 200 ? "OK" : status == 201 ? "Created" : status == 400 ? "Bad Request" : status == 404 ? "Not Found" : status == 409 ? "Conflict" : "Internal Server Error";
    size_t body_len = strlen(body);
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\n\r\n",
        status, status_text, content_type, body_len);
    send_all(client, header, (size_t)n);
    send_all(client, body, body_len);
}

static void serve_static(socket_t client, const char *url_path) {
    char path[512];
    const char *relative = url_path;
    if (strcmp(url_path, "/") == 0) relative = "/index.html";
    if (strstr(relative, "..") || strchr(relative, '\\')) {
        send_response(client, 404, "text/plain; charset=utf-8", "Not found"); return;
    }
    snprintf(path, sizeof(path), "web%.500s", relative);
    FILE *f = fopen(path, "rb");
    if (!f) { send_response(client, 404, "text/plain; charset=utf-8", "File not found"); return; }
    fseek(f, 0, SEEK_END); long len = ftell(f); rewind(f);
    if (len < 0 || len > 5000000) { fclose(f); send_response(client, 500, "text/plain", "File too large"); return; }
    char *data = (char *)malloc((size_t)len + 1);
    if (!data) { fclose(f); send_response(client, 500, "text/plain", "Out of memory"); return; }
    size_t got = fread(data, 1, (size_t)len, f); fclose(f); data[got] = '\0';
    const char *type = "application/octet-stream";
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".html") == 0) type = "text/html; charset=utf-8";
    else if (ext && strcmp(ext, ".css") == 0) type = "text/css; charset=utf-8";
    else if (ext && strcmp(ext, ".js") == 0) type = "application/javascript; charset=utf-8";
    else if (ext && strcmp(ext, ".svg") == 0) type = "image/svg+xml";
    else if (ext && strcmp(ext, ".png") == 0) type = "image/png";
    char header[512];
    int hn = snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-cache\r\n\r\n", type, got);
    send_all(client, header, (size_t)hn); send_all(client, data, got);
    free(data);
}

static int read_request(socket_t client, char *request, size_t cap, size_t *header_len, size_t *body_len) {
    size_t total = 0;
    *header_len = 0; *body_len = 0;
    while (total + 1 < cap) {
        int n = recv(client, request + total, (int)(cap - total - 1), 0);
        if (n <= 0) return 0;
        total += (size_t)n; request[total] = '\0';
        char *end = strstr(request, "\r\n\r\n");
        if (end) {
            *header_len = (size_t)(end - request) + 4;
            int content_length = 0;
            char *line = strstr(request, "\r\n");
            while (line && line < end) {
                line += 2;
                if (line >= end) break;
                if (strncmp(line, "Content-Length:", 15) == 0 || strncmp(line, "content-length:", 15) == 0) {
                    content_length = atoi(line + 15); break;
                }
                line = strstr(line, "\r\n");
            }
            if (content_length < 0 || content_length > 65536) return 0;
            *body_len = (size_t)content_length;
            if (total >= *header_len + *body_len) return 1;
        }
    }
    return 0;
}

static void handle_client(socket_t client) {
    char request[MAX_REQUEST]; size_t header_len = 0, body_len = 0;
    if (!read_request(client, request, sizeof(request), &header_len, &body_len)) {
        send_response(client, 400, "application/json", "{\"error\":\"Invalid or oversized HTTP request.\"}"); return;
    }
    char method[12] = {0}, path[512] = {0};
    if (sscanf(request, "%11s %511s", method, path) != 2) {
        send_response(client, 400, "application/json", "{\"error\":\"Malformed request line.\"}"); return;
    }
    char *body = request + header_len; body[body_len] = '\0';
    if (strcmp(method, "OPTIONS") == 0) { send_response(client, 200, "application/json", "{}"); return; }
    char url_path[512]; snprintf(url_path, sizeof(url_path), "%s", path);
    char *query = strchr(url_path, '?'); if (query) *query = '\0';

    static char response[MAX_JSON];
    if (strcmp(method, "GET") == 0 && strcmp(url_path, "/api/health") == 0) {
        snprintf(response, sizeof(response), "{\"status\":\"ok\",\"app\":\"Smart Job Scheduler\",\"backend\":\"C\",\"jobCount\":%d}", g_job_count);
        send_response(client, 200, "application/json; charset=utf-8", response);
    } else if (strcmp(method, "GET") == 0 && strcmp(url_path, "/api/jobs") == 0) {
        jobs_json(response, sizeof(response)); send_response(client, 200, "application/json; charset=utf-8", response);
    } else if (strcmp(method, "POST") == 0 && strcmp(url_path, "/api/jobs") == 0) {
        if (g_job_count >= MAX_JOBS) { send_response(client, 409, "application/json", "{\"error\":\"The queue is full (100 jobs maximum).\"}"); }
        else {
            Job j; memset(&j, 0, sizeof(j));
            json_get_string(body, "name", j.name, sizeof(j.name)); sanitize_name(j.name);
            j.arrival = json_get_int(body, "arrival", -1);
            j.burst = json_get_int(body, "burst", -1);
            j.priority = json_get_int(body, "priority", 3);
            j.deadline = json_get_int(body, "deadline", -1);
            if (!j.name[0] || j.arrival < 0 || j.arrival > 100000 || j.burst < 1 || j.burst > 10000 || j.priority < 1 || j.priority > 10 || j.deadline < 0 || j.deadline > 200000) {
                send_response(client, 400, "application/json", "{\"error\":\"Enter a name, arrival >= 0, CPU burst 1-10000, priority 1-10, and deadline >= 0.\"}");
            } else {
                j.id = g_next_id++; g_jobs[g_job_count++] = j; save_jobs();
                snprintf(response, sizeof(response), "{\"message\":\"Job added.\",\"id\":%d}", j.id);
                send_response(client, 201, "application/json", response);
            }
        }
    } else if (strcmp(method, "DELETE") == 0 && strcmp(url_path, "/api/jobs") == 0) {
        char *idp = strstr(path, "id="); int id = idp ? atoi(idp + 3) : -1; int found = 0;
        for (int i = 0; i < g_job_count; ++i) if (g_jobs[i].id == id) {
            for (int j = i; j < g_job_count - 1; ++j) g_jobs[j] = g_jobs[j + 1];
            g_job_count--; found = 1; break;
        }
        if (found) { save_jobs(); send_response(client, 200, "application/json", "{\"message\":\"Job removed.\"}"); }
        else send_response(client, 404, "application/json", "{\"error\":\"Job not found.\"}");
    } else if (strcmp(method, "POST") == 0 && strcmp(url_path, "/api/reset") == 0) {
        seed_jobs(); save_jobs(); send_response(client, 200, "application/json", "{\"message\":\"Demo queue restored.\"}");
    } else if (strcmp(method, "POST") == 0 && strcmp(url_path, "/api/schedule") == 0) {
        char algorithm[24] = "FCFS"; json_get_string(body, "algorithm", algorithm, sizeof(algorithm));
        int quantum = json_get_int(body, "quantum", 2);
        schedule_response(algorithm, quantum, response, sizeof(response));
        send_response(client, 200, "application/json; charset=utf-8", response);
    } else if (strcmp(method, "GET") == 0 && strcmp(url_path, "/api/recommend") == 0) {
        recommendation(response, sizeof(response)); send_response(client, 200, "application/json; charset=utf-8", response);
    } else if (strncmp(url_path, "/api/", 5) == 0) {
        send_response(client, 404, "application/json", "{\"error\":\"API endpoint not found.\"}");
    } else if (strcmp(method, "GET") == 0) {
        serve_static(client, url_path);
    } else {
        send_response(client, 404, "application/json", "{\"error\":\"Route not found.\"}");
    }
}

int main(int argc, char **argv) {
    int port = PORT_DEFAULT;
    if (argc > 1) { int requested = atoi(argv[1]); if (requested >= 1024 && requested <= 65535) port = requested; }
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { fprintf(stderr, "Winsock initialization failed.\n"); return 1; }
#endif
    load_jobs();
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == (socket_t)SOCKET_ERR) { fprintf(stderr, "Could not create server socket.\n"); return 1; }
    int opt = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons((unsigned short)port);
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERR) {
        fprintf(stderr, "Could not bind port %d. It may already be in use. Try: SmartJobScheduler.exe 8081\n", port);
        CLOSESOCKET(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    if (listen(server, 16) == SOCKET_ERR) { fprintf(stderr, "Could not start listening socket.\n"); CLOSESOCKET(server); return 1; }
    printf("\n  SMART JOB SCHEDULER - C BACKEND\n");
    printf("  --------------------------------\n");
    printf("  Server running at: http://localhost:%d\n", port);
    printf("  Jobs loaded: %d | Data: %s\n", g_job_count, DATA_FILE);
    printf("  Keep this window open while using the dashboard.\n");
    printf("  Press Ctrl+C to stop the server.\n\n");
    for (;;) {
        struct sockaddr_in client_addr;
#ifdef _WIN32
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif
        socket_t client = accept(server, (struct sockaddr *)&client_addr, &addr_len);
        if (client == (socket_t)SOCKET_ERR) continue;
        handle_client(client);
        CLOSESOCKET(client);
    }
    CLOSESOCKET(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
