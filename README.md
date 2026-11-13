# cttp

**cttp** is an experimental HTTP/1.x server written in **C++** for Linux. It explores low-level TCP socket programming, HTTP request parsing, and event-driven connection handling using the Linux `epoll` interface.

The server supports a small set of HTTP routes for echoing text, inspecting the `User-Agent` request header, and reading or writing files in a configured directory. It also contains an early implementation of gzip response compression using zlib.

> **Project status:** Educational prototype / work in progress. This is not a production-ready HTTP server. Several known correctness and robustness issues are documented in [Known Issues](docs/KNOWN_ISSUES.md). The near-term plan is in the [Roadmap](ROADMAP.md).

## Current functionality

| Area | Implementation | Notes |
| --- | --- | --- |
| TCP server | POSIX socket setup, binding, listening, accepting clients | `getaddrinfo`, `socket`, `bind`, `listen`, `accept`; `SO_REUSEADDR` |
| I/O multiplexing | Linux `epoll` event loop | Single-threaded; registered sockets are not configured as nonblocking |
| HTTP request parsing | Request line, headers, and body extraction | Recognizes GET, POST, DELETE; parsing is incomplete and fragile |
| HTTP versions | HTTP/1.0 and HTTP/1.1 strings | Version checks are basic, not full protocol validation |
| Responses | Basic 200, 201 and 404 responses | Other error codes are not consistently handled |
| Text endpoints | Root, echo, user-agent | See routes below |
| File endpoints | File read and write | File I/O is currently **not binary-safe** |
| Compression | zlib-based gzip code path | Present but not reliably encoded or sized |
| Connection control | `Connection: close` handling | Persistent connection support is incomplete |

## Requirements

- Linux (the server uses `<sys/epoll.h>`).
- A compiler supporting C++17, such as a recent `g++`.
- `make`.
- zlib development headers and library (`zlib1g-dev` on Debian/Ubuntu).

On Debian/Ubuntu, the dependencies can be installed with:

```bash
sudo apt update
sudo apt install build-essential zlib1g-dev
```

## Build and run

```bash
git clone https://github.com/Khemio/cttp.git
cd cttp
mkdir -p build data
make
./build/server --directory data
```

The server listens on **port 4221**, which is currently fixed in `src/server.cpp`.

The existing `Makefile` also provides a `run` target:

```bash
mkdir -p build data
make
make run
```

**Important:** Although the entry point prints an option to run without arguments, the current code accesses `argv[2]` unconditionally. Use `--directory <path>` as shown above. The build directory is also not created automatically by the current Makefile.

## HTTP routes

| Method | Route | Intended response | Current behavior |
| --- | --- | --- | --- |
| `GET` | `/` | `200 OK` | Empty response body |
| `GET` | `/echo/{text}` | `200 OK`, `text/plain` | Returns the extracted text |
| `GET` | `/user-agent` | `200 OK`, `text/plain` | Returns the parsed `User-Agent` value; requires that header |
| `GET` | `/files/{filename}` | `200 OK`, `application/octet-stream` | Reads file from the configured directory; `404` if it cannot open it |
| `POST` | `/files/{filename}` | `201 Created` | Writes request body into a file; `404` if it cannot open it |

The request parser accepts the `DELETE` method, but there is **no DELETE request handler** in the event loop. Unknown GET routes return `404`; unknown POST routes do not have a complete fallback response.

### Example requests

In another terminal, while the server is running:

```bash
# Root endpoint
curl --http1.1 -i http://127.0.0.1:4221/

# Echo a text segment
curl --http1.1 -i http://127.0.0.1:4221/echo/hello

# Return a request header
curl --http1.1 -i -H 'User-Agent: cttp-demo' \
  http://127.0.0.1:4221/user-agent

# Upload a text file
printf 'Hello from cttp!\n' > sample.txt
curl --http1.1 -i --data-binary @sample.txt \
  http://127.0.0.1:4221/files/sample.txt

# Download the file
curl --http1.1 -i http://127.0.0.1:4221/files/sample.txt
```

These commands illustrate the intended interfaces; they are **not a verified passing smoke-test suite**. Due to the current receive-buffer and HTTP-parsing issues, requests may behave unexpectedly. Avoid using the implementation for untrusted clients or data.

### Compression

The server checks `Accept-Encoding` and has a zlib-based gzip branch for some GET responses. That branch currently uses a fixed-size output buffer without resizing it to the actual compressed length. **Do not rely on gzip responses being correct in the present version.**

## Project layout

```text
cttp/
├── Makefile                 # Compiler and run commands
├── src/
│   ├── server.cpp           # CLI argument handling and entry point
│   ├── http_server.hpp      # epoll event loop, routing, responses, gzip
│   ├── http_req.hpp         # HTTP request parser and validation
│   ├── socket.hpp           # POSIX socket wrapper and Client structure
│   └── file.hpp             # File read/write wrapper
└── .gitignore
```

The repository also contains a tracked `src/server` binary. The `build/` and `data/` directories are ignored and must be created locally as needed.

## Design overview

At a high level, the program:

1. Opens a TCP listening socket on port 4221.
2. Registers the listening descriptor with an `epoll` instance.
3. Waits for readable socket events, accepting and registering new clients.
4. Receives bytes from a client and attempts to parse an HTTP request.
5. Dispatches GET or POST requests to hard-coded route handlers.
6. Constructs an HTTP response, sends it, and optionally closes the client connection.

This is an **event-loop architecture**, not yet a fully nonblocking network stack. For more detail, see [Architecture](docs/ARCHITECTURE.md).

## Limitations and next steps

The most important remaining work is making the existing foundation correct:

- Make socket receive logic memory-safe and accumulate exactly the bytes received.
- Frame complete HTTP requests using headers and body length instead of assuming one `recv` equals one request.
- Handle partial writes and nonblocking I/O correctly.
- Fix binary-safe file reading/writing and gzip response sizing.
- Strengthen HTTP validation, error responses, routing, and automated tests.
- Improve CLI argument validation and build setup.

See the [Roadmap](ROADMAP.md) for the deliberately limited next steps and [Known Issues](docs/KNOWN_ISSUES.md) for detailed code-level findings.

## Motivation

cttp is a hands-on exploration of how HTTP servers operate below the framework level: socket lifecycle, file descriptors, event notification, text-protocol parsing, response formatting, and system-call failure cases. It is useful as a learning and refactoring project rather than a general-purpose server.

## Documentation

- [Architecture and request lifecycle](docs/ARCHITECTURE.md)
- [Development roadmap](ROADMAP.md)
- [Known issues and technical backlog](docs/KNOWN_ISSUES.md)
