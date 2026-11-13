# cttp — Known Issues and Technical Backlog

This is a code-informed technical backlog for the current [Khemio/cttp](https://github.com/Khemio/cttp) `main` branch. Items describe observed source-code issues, not claims that they were reproduced by a test suite. Priorities reflect potential correctness and impact. This is a comprehensive technical backlog, **not** a commitment to implement everything immediately; the limited near-term scope is in the [Roadmap](../ROADMAP.md).

## Priority 0 — Correctness and safety

### 1. Fix the socket receive buffer

**Location:** `src/socket.hpp`, both `Socket::recv()` overloads.

The local variable is declared as `char* temp[size]`—an array of **pointers**, not a byte buffer. The `recv` call writes into that memory, and then `buf = (char*)temp` treats the result as a NUL-terminated C string. Network data is not guaranteed to be NUL-terminated, and its actual byte count is ignored.

**Fix:** Receive into a real byte buffer (for example, `std::array<char, N>` or `std::vector<char>`) and append exactly `n` bytes when `recv` returns a positive count. Handle `n == 0`, `n < 0`, and transient errors explicitly. Preserve partially received data per connection.

### 2. Implement HTTP message framing

**Locations:** `src/http_server.hpp` (`serve`), `src/http_req.hpp` (`parse`).

The server assumes that each `recv` call contains a whole HTTP request. TCP is a byte stream: one request may arrive in several reads, or several requests may arrive together. The parser does not use `Content-Length` to decide when the body is complete.

**Fix:** Define a connection state with an input buffer; read until a complete header section (`\r\n\r\n`) and the declared request body are available; consume exactly one message and retain trailing bytes. Reject oversized headers/bodies with documented limits. Add tests for fragmented and coalesced requests.

### 3. Make file I/O byte-preserving

**Location:** `src/file.hpp`.

`File::read()` rebuilds file contents line by line and calls `pop_back` after reading. Empty files can cause invalid access, and binary bytes are not preserved. `File::write()` removes bytes that are neither printable nor whitespace, changing binary payloads.

**Fix:** Open files with `std::ios::binary`; use byte-preserving stream reads/writes; handle empty files; make errors explicit; verify round trips with random binary data.

### 4. Make request parsing safe on malformed inputs

**Location:** `src/http_req.hpp`.

The parser calls `pop_back()` on strings without consistently checking whether they are nonempty, and `validate_uri()` accesses `uri[0]` without checking length. Unexpected line formats can produce undefined behavior. Parser failures are logged in `serve()`, but request dispatch may continue using partially populated fields.

**Fix:** Validate bounds, parse CRLF carefully, return a structured parse result, and stop dispatch when parsing fails. Return an HTTP 400 response for malformed requests.

## Priority 1 — Network and protocol reliability

### 5. Use nonblocking sockets and maintain write state

**Locations:** `src/socket.hpp`, `src/http_server.hpp`.

`epoll` currently watches `EPOLLIN` on sockets that are still blocking. `Socket::send()` calls `send` once, without handling partial writes. A correct event-driven implementation needs to account for `EAGAIN`/`EWOULDBLOCK`, `EINTR`, and queued output.

**Fix:** Set `O_NONBLOCK` on listening and accepted descriptors, read until `EAGAIN`, queue responses, and register for `EPOLLOUT` only while there is pending output. Distinguish clean disconnects from errors.

### 6. Fix gzip encoding and content lengths

**Location:** `src/http_server.hpp` (`gzip`, GET handlers).

The implementation reserves `input_size + 20` bytes, calls `deflate`, but does not resize the `std::string` to `zs.total_out` or check the zlib return codes. For some inputs the output buffer may also be too small. The sent `Content-Length` is therefore not guaranteed to reflect the compressed payload.

**Fix:** Use `deflateBound` or an incremental output buffer; handle zlib errors; resize to the actual compressed byte count; add decompression round-trip tests. Consider removing gzip until those tests pass.

### 7. Complete HTTP header handling

**Location:** `src/http_req.hpp`.

Header parsing splits on commas and removes **all** whitespace from values, including meaningful interior spaces (`User-Agent`, quoted fields, and similar values). It does not fully validate header syntax or support repeated fields reliably.

**Fix:** Separate header-name normalization from header-value parsing, trim only optional leading/trailing whitespace, preserve repeated fields, and implement rules for individual headers where relevant.

### 8. Define predictable routing and error behavior

**Location:** `src/http_server.hpp`.

`check_path()` compares prefixes without checking path-segment boundaries. For example, a path starting with `/echo` but not followed by `/` may be treated as an echo route. The parser allows DELETE but the event loop ignores it, and unrecognized POST paths can go unanswered.

**Fix:** Use exact matches or segment-aware route matching; return 404 for missing paths, 405 for unsupported methods as appropriate, and explicit errors for file failures. Validate extracted filenames and keep them inside the configured directory.

### 9. Strengthen response and connection handling

**Locations:** `src/http_server.hpp`, `src/socket.hpp`.

Responses are built directly as strings, with limited status and header support. Connection close is handled per route rather than centrally. There is no clear handling of keep-alive timeouts, idle connections, socket errors, or incomplete responses.

**Fix:** Centralize response serialization, connection policy, and errors; manage per-client lifecycle and timeouts in one place.

## Priority 2 — Build, maintainability, and testing

### 10. Fix command-line behavior and the build

**Locations:** `src/server.cpp`, `Makefile`.

`main` allows `argc == 1` but then reads `argv[2]`. The Makefile assumes `build/` already exists and does not explicitly specify a language standard or expose a clean target.

**Fix:** Require `--directory` or provide a real default; validate path and port; add `mkdir -p build`, compiler flags, and clean/build/run targets.

### 11. Break implementation code out of headers

**Locations:** `src/socket.hpp`, `src/file.hpp`, `src/http_req.hpp`, `src/http_server.hpp`.

Most non-inline method definitions are in headers. This works in the current one-translation-unit build but will create multiple-definition/linkage issues when included from additional translation units.

**Fix:** Move non-template definitions to `.cpp` files (or explicitly mark appropriate header definitions `inline`), improve const-correctness and RAII, and replace raw `Socket*` ownership with direct ownership or a smart pointer.

### 12. Add automated tests and continuous integration

The checked-in source does not contain an automated test suite.

Start with:

- Request parser unit tests: valid inputs, missing fields, CRLF edge cases, repeated headers, fragmented requests, malformed lengths.
- Socket/integration tests: multiple clients, disconnects, partial reads/writes, unknown paths and methods.
- File tests: empty files, binary round trips, missing files, invalid paths.
- Compression tests: valid gzip stream and exact `Content-Length`.
- Memory/runtime tools: AddressSanitizer, UndefinedBehaviorSanitizer, and (where practical) Valgrind.

## Near-term scope

The active, intentionally small development plan is maintained in [ROADMAP.md](../ROADMAP.md). In particular, defer full keep-alive and pipelining, gzip support, substantial refactoring, and new protocol features until the current basic routes pass automated tests.

## Source references

- [Socket wrapper](https://github.com/Khemio/cttp/blob/main/src/socket.hpp)
- [HTTP parser](https://github.com/Khemio/cttp/blob/main/src/http_req.hpp)
- [HTTP server and event loop](https://github.com/Khemio/cttp/blob/main/src/http_server.hpp)
- [File wrapper](https://github.com/Khemio/cttp/blob/main/src/file.hpp)
- [Entry point](https://github.com/Khemio/cttp/blob/main/src/server.cpp)
- [Makefile](https://github.com/Khemio/cttp/blob/main/Makefile)