# cttp — Architecture

This document describes the implementation in the current `main` branch of [Khemio/cttp](https://github.com/Khemio/cttp). Descriptions of existing code are distinguished from future improvements.

## Overview

cttp is a single-process, single-threaded HTTP server built around a Linux `epoll` event loop. Request handling, network I/O, parsing, and file operations are implemented in project-specific C++ classes rather than a web framework.

```text
                       TCP client(s)
                             |
                             v
                     +---------------+
                     | Socket         |
                     | listen/accept  |
                     +-------+-------+
                             |
                       file descriptors
                             |
                             v
                     +---------------+
                     | Http_Server    |
                     | epoll_wait     |
                     +-------+-------+
                             |
                       recv() bytes
                             |
                             v
                     +---------------+
                     | Http_Req       |
                     | parse/validate|
                     +-------+-------+
                             |
                    method + URI + headers
                             |
                             v
                     +---------------+
                     | Route handlers |
                     | GET / POST    |
                     +----+-----+----+
                          |     |
             +------------+     +-------------+
             v                                v
       +-----------+                    +-----------+
       | File      |                    | gzip/zlib |
       | read/write|                    | (partial) |
       +-----------+                    +-----------+
             \                                /
              +------------+-----------------+
                           |
                     HTTP response
                           |
                           v
                     Socket::send()
```

**Important distinction:** Using `epoll` does not by itself make socket operations nonblocking. The current implementation does not set `O_NONBLOCK` on listening or accepted sockets, and it does not maintain complete per-connection read/write buffers.

## Source modules

### `src/server.cpp`

Defines `main`, configures unbuffered console output, validates the basic command-line argument shape, selects port `4221`, and creates the `Http_Server` instance. The intended invocation is `./build/server --directory data`.

The no-argument branch is defective: the source still accesses `argv[2]` when only `argv[0]` exists.

### `src/socket.hpp`

Defines `Client` and `Socket`.

- `Socket::init()` resolves a passive address with `getaddrinfo`, creates a TCP socket, enables `SO_REUSEADDR`, binds it, and starts listening. Default backlog: **5**.
- `Socket::accept()` wraps `accept` and populates a client file descriptor.
- `Socket::send()` and `Socket::recv()` wrap the POSIX system calls.
- `Socket::close()` closes the listening descriptor.

This module is a foundation, but the receive implementation has a critical memory-safety bug (described below and in [Known Issues](KNOWN_ISSUES.md)). The send wrapper also performs only one `send` call and does not handle a partial write.

### `src/http_req.hpp`

Defines `Http_Req`, with the fields:

```cpp
std::string method, uri, version, body;
std::unordered_map<std::string, std::vector<std::string>> headers;
```

The parser splits the request line, validates its method/URI/version, lowercases header names, separates comma-delimited header values, and extracts a body after the blank line.

The method validator accepts `GET`, `POST`, and `DELETE`, and the version validator recognizes `HTTP/1.0` and `HTTP/1.1` in a simplified manner. Only GET and POST are dispatched in `Http_Server::serve()`.

**Current parser constraints:** It assumes CRLF-style lines and uses `pop_back` without enough length checks; it removes all whitespace within header values; it does not use `Content-Length` for message framing; and some malformed inputs can produce invalid state or undefined behavior.

### `src/http_server.hpp`

Implements the server lifecycle, `epoll` setup, client registration, event loop, route handlers, HTTP response construction, and an early gzip helper.

Relevant members and methods include:

- `client_map`: maps client file descriptors to `Client` objects.
- `init_epoll()`: creates the `epoll` instance and registers the listening socket for `EPOLLIN`.
- `add_client()`: accepts a client and registers its descriptor for `EPOLLIN`.
- `handle_disconnect()`: removes client state, deregisters the descriptor, and closes it.
- `serve()`: waits for events, receives data, parses a request, and dispatches to GET or POST handlers.
- `handle_get()` / `handle_post()`: hard-coded route implementations.
- `make_msg()` and `send_200()` / `send_201()` / `send_404()`: construct basic HTTP responses.
- `gzip()`: calls zlib's deflate API with a gzip wrapper but does not correctly finalize the output string length.

The `epoll_wait` call uses a maximum batch size of **10 events**. This is a per-wait event-array capacity, not a maximum-client guarantee.

### `src/file.hpp`

Defines a file wrapper around `std::fstream`, with `READ`, `WRITE`, and `READ_WRITE` modes. It exposes `read()` and `write()` and uses a small custom exception class.

Current read/write operations are not binary-safe: reads reconstruct text line by line; writes filter control bytes. Empty files can trigger a `pop_back()` on an empty string.

## Request lifecycle

### Startup

1. `main` creates `Http_Server` with a fixed port and configured file directory.
2. The constructor initializes `Socket`, then creates and registers an `epoll` instance.
3. `serve()` enters an infinite event loop.

### Client connection

1. `epoll_wait()` returns a readable event for the listening descriptor.
2. `add_client()` calls `accept()`.
3. The accepted descriptor is stored in `client_map` and registered for `EPOLLIN`.

### Incoming request

1. An event for a client descriptor leads to one call to `Socket::recv()`.
2. If the call returns zero, the server disconnects the client.
3. Otherwise, `Http_Req::parse()` attempts to interpret the received data as a complete HTTP message.
4. A GET method invokes `handle_get`; a POST method invokes `handle_post`.
5. A response is formatted and sent. If the request asks for `Connection: close`, the handler closes the client descriptor after the send.

This sequence assumes a complete request is available after one receive call. TCP does **not** preserve HTTP request boundaries, so the assumption is invalid for production use.

## Route details

### `GET /`

Responds `HTTP/1.1 200 OK`, `Content-Length: 0`, with no body.

### `GET /echo/{text}`

Uses a prefix comparison and extracts the text after `/echo/`, returns `text/plain`. If an `Accept-Encoding` header contains a `gzip` token, the code takes a compression branch (currently unreliable).

### `GET /user-agent`

Reads the first entry from the parsed `user-agent` header vector and returns it as `text/plain`. There is no guard for a missing header, and header-value whitespace is currently stripped by the parser.

### `GET /files/{filename}`

Extracts the first path segment after `/files/`, opens `directory + filename`, and returns `application/octet-stream` with 200 status. If the file cannot be opened, the handler returns 404. File reading is text-oriented and unsafe for empty/binary files.

### `POST /files/{filename}`

Extracts the filename, opens `directory + filename` for writing, writes the body via `File::write()`, and returns 201 on success. The current write function filters bytes and therefore does not guarantee exact persistence of the submitted payload.

### Unhandled cases

- The request parser accepts `DELETE`, but the dispatch loop has no DELETE branch.
- Unknown GET paths return 404.
- Unknown POST paths may not receive a response.
- Other HTTP errors (400, 405, 413, 500, etc.) are not implemented as a comprehensive response policy.

## Protocol and performance boundaries

| Topic | Current behavior | Consequence |
| --- | --- | --- |
| Socket mode | Blocking sockets | A slow client or partial operation may stall the event loop |
| Receive buffering | Single receive, incorrect temporary storage | Unsafe parsing, truncation, unpredictable behavior |
| Request framing | No complete `Content-Length` state machine | Cannot reliably handle fragmented requests |
| Response writes | Single `send` | Partial sends may truncate output |
| Keep-alive | Multiple requests are possible in concept; no proper state machine | Incomplete HTTP/1.1 persistence and pipelining |
| Compression | zlib output buffer not correctly sized/trimmed | Corrupt or invalid gzip response bodies |
| File access | Text-oriented `fstream` behavior | Binary data and empty files are not safely handled |
| Concurrency | One event loop thread | Good learning model; throughput and isolation not established |

## Suggested direction

The code structure can be retained while the underlying correctness is improved: define a connection state machine with input/output byte buffers, switch to nonblocking sockets, parse messages incrementally, and make the file and response paths binary-safe. See the [Roadmap](../ROADMAP.md) for short-term scope and [Known Issues](KNOWN_ISSUES.md) for detailed technical findings.