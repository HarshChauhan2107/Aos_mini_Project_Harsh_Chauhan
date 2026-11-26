/*
 * Intelligent Disk I/O Scheduling Analyzer
 * C HTTP server + disk scheduling simulation engine.
 * Algorithms: FCFS, SSTF, SCAN, C-SCAN, LOOK, C-LOOK.
 * Build: make (Linux/macOS) or build.bat (Windows with MinGW-w64).
 */

#ifdef _WIN32
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#define SOCKET_ERROR_CODE WSAGetLastError()
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define CLOSE_SOCKET close
#define SOCKET_ERROR_CODE 0
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 8080
#define MAX_HTTP_REQUEST 65536
#define MAX_BODY 16384
#define MAX_REQUESTS 128
#define MAX_STEPS (MAX_REQUESTS + 8)
#define MAX_RESPONSE 180000
#define MAX_URL_PATH 1024

typedef struct {
    int from;
    int to;
    int distance;
    const char *kind;
} Step;

typedef struct {
    const char *name;
    int service_order[MAX_REQUESTS];
    int service_count;
    int path[MAX_STEPS];
    int path_count;
    Step steps[MAX_STEPS];
    int step_count;
    long total_movement;
    int max_seek;
    int min_seek;
} ScheduleResult;

typedef struct {
    int head;
    int disk_min;
    int disk_max;
    char direction[8];
    char algorithm[16];
    int requests[MAX_REQUESTS];
    int request_count;
} Input;

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

static const char *find_key(const char *json, const char *key) {
    static char pattern[96];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return NULL;
    p = strchr(p + strlen(pattern), ':');
    return p ? p + 1 : NULL;
}

static bool parse_int_field(const char *json, const char *key, int *out) {
    const char *p = find_key(json, key);
    if (!p) return false;
    while (*p && isspace((unsigned char)*p)) ++p;
    errno = 0;
    char *end = NULL;
    long value = strtol(p, &end, 10);
    if (p == end || errno == ERANGE || value < INT_MIN || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool parse_string_field(const char *json, const char *key, char *out, size_t cap) {
    const char *p = find_key(json, key);
    if (!p) return false;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != '"') return false;
    ++p;
    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) ++p;
        if (i + 1 < cap) out[i++] = *p;
        ++p;
    }
    if (*p != '"') return false;
    out[i] = '\0';
    return true;
}

static bool parse_requests(const char *json, Input *in) {
    const char *p = find_key(json, "requests");
    if (!p) return false;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != '[') return false;
    ++p;
    in->request_count = 0;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) ++p;
        if (*p == ']') return in->request_count > 0;
        if (!*p || in->request_count >= MAX_REQUESTS) return false;
        errno = 0;
        char *end = NULL;
        long value = strtol(p, &end, 10);
        if (p == end || errno == ERANGE || value < INT_MIN || value > INT_MAX) return false;
        in->requests[in->request_count++] = (int)value;
        p = end;
        while (*p && isspace((unsigned char)*p)) ++p;
        if (*p != ',' && *p != ']') return false;
    }
    return false;
}

static bool parse_input(const char *body, Input *in, char *error, size_t error_cap) {
    memset(in, 0, sizeof(*in));
    if (!parse_int_field(body, "head", &in->head)) {
        snprintf(error, error_cap, "Please provide a valid initial head position."); return false;
    }
    if (!parse_int_field(body, "disk_min", &in->disk_min) ||
        !parse_int_field(body, "disk_max", &in->disk_max)) {
        snprintf(error, error_cap, "Please provide valid minimum and maximum cylinder values."); return false;
    }
    if (!parse_string_field(body, "direction", in->direction, sizeof(in->direction))) {
        snprintf(error, error_cap, "Please select a scan direction."); return false;
    }
    if (!parse_string_field(body, "algorithm", in->algorithm, sizeof(in->algorithm))) {
        snprintf(error, error_cap, "Please select an algorithm."); return false;
    }
    if (!parse_requests(body, in)) {
        snprintf(error, error_cap, "Enter between 1 and %d integer disk requests, separated by commas.", MAX_REQUESTS); return false;
    }
    if (in->disk_min < 0 || in->disk_max > 1000000000) {
        snprintf(error, error_cap, "Use non-negative cylinder values up to 1,000,000,000."); return false;
    }
    if (in->disk_min >= in->disk_max) {
        snprintf(error, error_cap, "Disk minimum must be smaller than disk maximum."); return false;
    }
    if (in->head < in->disk_min || in->head > in->disk_max) {
        snprintf(error, error_cap, "Initial head position must be within the disk range."); return false;
    }
    if (strcmp(in->direction, "left") != 0 && strcmp(in->direction, "right") != 0) {
        snprintf(error, error_cap, "Direction must be either left or right."); return false;
    }
    for (int i = 0; i < in->request_count; ++i) {
        if (in->requests[i] < in->disk_min || in->requests[i] > in->disk_max) {
            snprintf(error, error_cap, "Request %d is outside the disk range (%d to %d).", in->requests[i], in->disk_min, in->disk_max);
            return false;
        }
    }
    const char *valid[] = {"ALL", "FCFS", "SSTF", "SCAN", "C-SCAN", "LOOK", "C-LOOK"};
    bool found = false;
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i)
        if (strcmp(in->algorithm, valid[i]) == 0) found = true;
    if (!found) {
        snprintf(error, error_cap, "Unknown algorithm selected."); return false;
    }
    return true;
}

static int int_compare(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static void init_result(ScheduleResult *r, const char *name, int head) {
    memset(r, 0, sizeof(*r));
    r->name = name;
    r->path[r->path_count++] = head;
    r->min_seek = INT_MAX;
}

static void move_head(ScheduleResult *r, int destination, const char *kind) {
    if (r->step_count >= MAX_STEPS || r->path_count >= MAX_STEPS) return;
    int from = r->path[r->path_count - 1];
    int distance = abs(destination - from);
    Step *s = &r->steps[r->step_count++];
    s->from = from; s->to = destination; s->distance = distance; s->kind = kind;
    r->total_movement += distance;
    if (distance > r->max_seek) r->max_seek = distance;
    if (distance < r->min_seek) r->min_seek = distance;
    r->path[r->path_count++] = destination;
}

static void record_service(ScheduleResult *r, int request) {
    if (r->service_count < MAX_REQUESTS) r->service_order[r->service_count++] = request;
}

static void service_request(ScheduleResult *r, int request) {
    record_service(r, request);
    move_head(r, request, "service");
}

static void build_fcfs(const Input *in, ScheduleResult *r) {
    init_result(r, "FCFS", in->head);
    for (int i = 0; i < in->request_count; ++i) service_request(r, in->requests[i]);
}

static void build_sstf(const Input *in, ScheduleResult *r) {
    init_result(r, "SSTF", in->head);
    bool done[MAX_REQUESTS] = {false};
    int current = in->head;
    for (int k = 0; k < in->request_count; ++k) {
        int best = -1, best_distance = INT_MAX;
        for (int i = 0; i < in->request_count; ++i) {
            if (done[i]) continue;
            int d = abs(in->requests[i] - current);
            if (d < best_distance) { best_distance = d; best = i; }
        }
        if (best < 0) break;
        done[best] = true;
        service_request(r, in->requests[best]);
        current = in->requests[best];
    }
}

static void split_and_sort(const Input *in, int *lower, int *lower_n, int *upper, int *upper_n, int *same, int *same_n) {
    *lower_n = *upper_n = *same_n = 0;
    for (int i = 0; i < in->request_count; ++i) {
        int v = in->requests[i];
        if (v < in->head) lower[(*lower_n)++] = v;
        else if (v > in->head) upper[(*upper_n)++] = v;
        else same[(*same_n)++] = v;
    }
    qsort(lower, (size_t)*lower_n, sizeof(int), int_compare);
    qsort(upper, (size_t)*upper_n, sizeof(int), int_compare);
}

static void service_same(ScheduleResult *r, const int *same, int n) {
    for (int i = 0; i < n; ++i) service_request(r, same[i]);
}

static void build_scan_family(const Input *in, ScheduleResult *r, const char *name) {
    int lo[MAX_REQUESTS], hi[MAX_REQUESTS], eq[MAX_REQUESTS], ln, hn, en;
    split_and_sort(in, lo, &ln, hi, &hn, eq, &en);
    init_result(r, name, in->head);
    service_same(r, eq, en);
    bool right = strcmp(in->direction, "right") == 0;

    if (strcmp(name, "SCAN") == 0) {
        if (right) {
            for (int i = 0; i < hn; ++i) service_request(r, hi[i]);
            if (ln > 0) {
                move_head(r, in->disk_max, "boundary");
                for (int i = ln - 1; i >= 0; --i) service_request(r, lo[i]);
            }
        } else {
            for (int i = ln - 1; i >= 0; --i) service_request(r, lo[i]);
            if (hn > 0) {
                move_head(r, in->disk_min, "boundary");
                for (int i = 0; i < hn; ++i) service_request(r, hi[i]);
            }
        }
    } else if (strcmp(name, "C-SCAN") == 0) {
        if (right) {
            for (int i = 0; i < hn; ++i) service_request(r, hi[i]);
            if (ln > 0) {
                move_head(r, in->disk_max, "boundary");
                move_head(r, in->disk_min, "wrap");
                for (int i = 0; i < ln; ++i) service_request(r, lo[i]);
            }
        } else {
            for (int i = ln - 1; i >= 0; --i) service_request(r, lo[i]);
            if (hn > 0) {
                move_head(r, in->disk_min, "boundary");
                move_head(r, in->disk_max, "wrap");
                for (int i = hn - 1; i >= 0; --i) service_request(r, hi[i]);
            }
        }
    } else if (strcmp(name, "LOOK") == 0) {
        if (right) {
            for (int i = 0; i < hn; ++i) service_request(r, hi[i]);
            for (int i = ln - 1; i >= 0; --i) service_request(r, lo[i]);
        } else {
            for (int i = ln - 1; i >= 0; --i) service_request(r, lo[i]);
            for (int i = 0; i < hn; ++i) service_request(r, hi[i]);
        }
    } else { /* C-LOOK */
        if (right) {
            for (int i = 0; i < hn; ++i) service_request(r, hi[i]);
            if (ln > 0) {
                move_head(r, lo[0], "wrap");
                record_service(r, lo[0]);
                for (int i = 1; i < ln; ++i) service_request(r, lo[i]);
            }
        } else {
            for (int i = ln - 1; i >= 0; --i) service_request(r, lo[i]);
            if (hn > 0) {
                move_head(r, hi[hn - 1], "wrap");
                record_service(r, hi[hn - 1]);
                for (int i = hn - 2; i >= 0; --i) service_request(r, hi[i]);
            }
        }
    }
}

static void build_schedule(const Input *in, const char *name, ScheduleResult *r) {
    if (strcmp(name, "FCFS") == 0) build_fcfs(in, r);
    else if (strcmp(name, "SSTF") == 0) build_sstf(in, r);
    else build_scan_family(in, r, name);
    if (r->min_seek == INT_MAX) r->min_seek = 0;
}

static void write_result_json(char *out, size_t cap, size_t *used, const ScheduleResult *r, int request_count) {
    appendf(out, cap, used, "{\"algorithm\":\"%s\",\"service_order\":[", r->name);
    for (int i = 0; i < r->service_count; ++i)
        appendf(out, cap, used, "%s%d", i ? "," : "", r->service_order[i]);
    appendf(out, cap, used, "],\"path\":[");
    for (int i = 0; i < r->path_count; ++i)
        appendf(out, cap, used, "%s%d", i ? "," : "", r->path[i]);
    appendf(out, cap, used, "],\"steps\":[");
    for (int i = 0; i < r->step_count; ++i) {
        const Step *s = &r->steps[i];
        appendf(out, cap, used,
                "%s{\"from\":%d,\"to\":%d,\"distance\":%d,\"kind\":\"%s\"}",
                i ? "," : "", s->from, s->to, s->distance, s->kind);
    }
    double average = request_count ? (double)r->total_movement / (double)request_count : 0.0;
    appendf(out, cap, used,
            "],\"total_movement\":%ld,\"average_seek\":%.2f,\"max_seek\":%d,\"min_seek\":%d}",
            r->total_movement, average, r->max_seek, r->min_seek);
}

static void json_error(char *out, size_t cap, const char *message) {
    size_t j = 0;
    appendf(out, cap, &j, "{\"error\":\"");
    for (const char *p = message; *p && j + 8 < cap; ++p) {
        if (*p == '"' || *p == '\\') appendf(out, cap, &j, "\\%c", *p);
        else if (*p == '\n') appendf(out, cap, &j, "\\n");
        else appendf(out, cap, &j, "%c", *p);
    }
    appendf(out, cap, &j, "\"}");
}

static bool analyze_json(const char *body, char *out, size_t cap, int *status) {
    Input in;
    char error[256];
    if (!parse_input(body, &in, error, sizeof(error))) {
        json_error(out, cap, error); *status = 400; return false;
    }
    const char *names[] = {"FCFS", "SSTF", "SCAN", "C-SCAN", "LOOK", "C-LOOK"};
    size_t used = 0;
    appendf(out, cap, &used,
            "{\"head\":%d,\"disk_min\":%d,\"disk_max\":%d,\"direction\":\"%s\",\"request_count\":%d,\"results\":[",
            in.head, in.disk_min, in.disk_max, in.direction, in.request_count);
    int emitted = 0;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(in.algorithm, "ALL") != 0 && strcmp(in.algorithm, names[i]) != 0) continue;
        ScheduleResult r;
        build_schedule(&in, names[i], &r);
        if (emitted++) appendf(out, cap, &used, ",");
        write_result_json(out, cap, &used, &r, in.request_count);
    }
    appendf(out, cap, &used, "]}");
    *status = 200;
    return true;
}

static const char *reason_phrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        default: return "Internal Server Error";
    }
}

static bool send_all(socket_t client, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
#ifdef _WIN32
        int n = send(client, data + sent, (int)(length - sent), 0);
#else
        ssize_t n = send(client, data + sent, length - sent, 0);
#endif
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

static void send_response(socket_t client, int status, const char *content_type, const char *body, size_t body_len) {
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
        "Connection: close\r\n\r\n",
        status, reason_phrase(status), content_type, body_len);
    if (n > 0) {
        send_all(client, header, (size_t)n);
        send_all(client, body, body_len);
    }
}

static const char *mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".png") == 0) return "image/png";
    return "application/octet-stream";
}

static void serve_static(socket_t client, const char *url_path) {
    char relative[MAX_URL_PATH];
    if (strcmp(url_path, "/") == 0) snprintf(relative, sizeof(relative), "web/index.html");
    else if (strcmp(url_path, "/index.html") == 0) snprintf(relative, sizeof(relative), "web/index.html");
    else if (strcmp(url_path, "/style.css") == 0) snprintf(relative, sizeof(relative), "web/style.css");
    else if (strcmp(url_path, "/app.js") == 0) snprintf(relative, sizeof(relative), "web/app.js");
    else {
        const char *body = "Not found";
        send_response(client, 404, "text/plain; charset=utf-8", body, strlen(body));
        return;
    }
    FILE *fp = fopen(relative, "rb");
    if (!fp) {
        const char *body = "Frontend files not found. Start the server from the project root directory.";
        send_response(client, 404, "text/plain; charset=utf-8", body, strlen(body));
        return;
    }
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return; }
    long length = ftell(fp);
    if (length < 0 || length > 2 * 1024 * 1024) { fclose(fp); return; }
    rewind(fp);
    char *data = (char *)malloc((size_t)length + 1);
    if (!data) { fclose(fp); return; }
    size_t read_n = fread(data, 1, (size_t)length, fp);
    fclose(fp);
    send_response(client, 200, mime_type(relative), data, read_n);
    free(data);
}

static void handle_client(socket_t client) {
    char request[MAX_HTTP_REQUEST + 1];
    size_t used = 0;
    size_t header_end = 0;
    while (used < MAX_HTTP_REQUEST) {
#ifdef _WIN32
        int n = recv(client, request + used, (int)(MAX_HTTP_REQUEST - used), 0);
#else
        ssize_t n = recv(client, request + used, MAX_HTTP_REQUEST - used, 0);
#endif
        if (n <= 0) { CLOSE_SOCKET(client); return; }
        used += (size_t)n;
        request[used] = '\0';
        char *marker = strstr(request, "\r\n\r\n");
        if (marker) { header_end = (size_t)(marker - request) + 4; break; }
    }
    if (!header_end) {
        const char *msg = "{\"error\":\"HTTP request headers are too large.\"}";
        send_response(client, 400, "application/json", msg, strlen(msg)); CLOSE_SOCKET(client); return;
    }

    char method[8] = {0}, url[MAX_URL_PATH] = {0};
    if (sscanf(request, "%7s %1023s", method, url) != 2) {
        const char *msg = "{\"error\":\"Malformed HTTP request.\"}";
        send_response(client, 400, "application/json", msg, strlen(msg)); CLOSE_SOCKET(client); return;
    }
    char *header_text = request;
    size_t content_length = 0;
    char *cl = strstr(header_text, "Content-Length:");
    if (!cl) cl = strstr(header_text, "content-length:");
    if (cl) content_length = (size_t)strtoul(strchr(cl, ':') + 1, NULL, 10);

    if (strcmp(method, "GET") == 0) {
        if (strcmp(url, "/api/health") == 0) {
            const char *body = "{\"status\":\"ok\",\"service\":\"Intelligent Disk I/O Scheduling Analyzer\"}";
            send_response(client, 200, "application/json; charset=utf-8", body, strlen(body));
        } else serve_static(client, url);
    } else if (strcmp(method, "POST") == 0 && strcmp(url, "/api/analyze") == 0) {
        if (content_length > MAX_BODY) {
            const char *msg = "{\"error\":\"Request body is too large.\"}";
            send_response(client, 413, "application/json; charset=utf-8", msg, strlen(msg));
        } else {
            size_t body_have = used - header_end;
            while (body_have < content_length && used < MAX_HTTP_REQUEST) {
#ifdef _WIN32
                int n = recv(client, request + used, (int)(MAX_HTTP_REQUEST - used), 0);
#else
                ssize_t n = recv(client, request + used, MAX_HTTP_REQUEST - used, 0);
#endif
                if (n <= 0) break;
                used += (size_t)n; body_have += (size_t)n;
            }
            if (body_have < content_length) {
                const char *msg = "{\"error\":\"Incomplete request body.\"}";
                send_response(client, 400, "application/json; charset=utf-8", msg, strlen(msg));
            } else {
                request[header_end + content_length] = '\0';
                char response[MAX_RESPONSE];
                int status = 200;
                analyze_json(request + header_end, response, sizeof(response), &status);
                send_response(client, status, "application/json; charset=utf-8", response, strlen(response));
            }
        }
    } else {
        const char *msg = "{\"error\":\"Unsupported HTTP method or endpoint.\"}";
        send_response(client, 405, "application/json; charset=utf-8", msg, strlen(msg));
    }
    CLOSE_SOCKET(client);
}

int main(int argc, char **argv) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        char *end = NULL;
        long parsed = strtol(argv[1], &end, 10);
        if (!end || *end || parsed < 1024 || parsed > 65535) {
            fprintf(stderr, "Usage: %s [port 1024-65535]\n", argv[0]); return 1;
        }
        port = (int)parsed;
    }
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "Winsock initialization failed.\n"); return 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        fprintf(stderr, "Could not create server socket.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* local machine only */
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR || listen(server, 16) == SOCKET_ERROR) {
        fprintf(stderr, "Could not bind to 127.0.0.1:%d. Is the port already in use?\n", port);
        CLOSE_SOCKET(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    printf("\n  Intelligent Disk I/O Scheduling Analyzer\n");
    printf("  ----------------------------------------\n");
    printf("  Frontend: http://127.0.0.1:%d\n", port);
    printf("  API:      http://127.0.0.1:%d/api/analyze\n", port);
    printf("  Press Ctrl+C to stop the server.\n\n");
    for (;;) {
        socket_t client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) continue;
        handle_client(client);
    }
    CLOSE_SOCKET(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
