# cttp — Development Roadmap

**Status:** Small educational HTTP server; development paused, with a limited next iteration planned. This roadmap is deliberately narrower than the full [technical backlog](docs/KNOWN_ISSUES.md).

## Goal for the next iteration

Make the **existing HTTP/1.1 routes predictable and safe for local testing** on Linux: `GET /`, `GET /echo/{text}`, `GET /user-agent`, `GET /files/{filename}`, and `POST /files/{filename}`. Keep the architecture **single-threaded with `epoll`**, a fixed set of routes, and no external HTTP framework.

**Scope decision:** Initially process **one request per connection** and send `Connection: close`. This avoids taking on persistent connections or HTTP pipelining while still requiring correct buffering, partial writes, and handling of concurrent clients. Defer optional features until the high-priority work is complete. This is not intended for deployment on untrusted networks.

## High priority — next iteration

Do these in roughly the following order. They are correctness work, not new feature development.

| ID | Task | Definition of done |
| --- | --- | --- |
| **H1** | **Fix build and startup** (`Makefile`, `src/server.cpp`) | `make` creates `build/`; one documented invocation starts the server; missing/invalid arguments fail cleanly; `make clean` works. |
| **H2** | **Repair socket receive and parser memory safety** (`socket.hpp`, `http_req.hpp`) | Read into actual byte storage and use the returned byte count; no NUL-termination assumptions; malformed lines/fields cannot trigger unchecked indexing or `pop_back()`. |
| **H3** | **Implement bounded HTTP/1.1 request framing** (`http_server.hpp`, `http_req.hpp`) | Buffer per connection until `\r\n\r\n` and the declared `Content-Length` body arrive; detect missing/invalid lengths where needed; enforce reasonable header/body limits; return `400` on malformed requests. Chunked requests can be explicitly rejected or left unsupported for now. |
| **H4** | **Make `epoll` I/O genuinely nonblocking** (`socket.hpp`, `http_server.hpp`) | Listening and accepted sockets are nonblocking; handle `EINTR` / `EAGAIN`; queue incomplete responses and resume on `EPOLLOUT`; one slow client does not block other clients; close only after bytes are flushed. |
| **H5** | **Make file routes correct and contained** (`file.hpp`, `http_server.hpp`) | Empty files and arbitrary bytes round-trip unchanged; reject unsafe/empty filenames and keep access within the configured directory; GET missing file returns `404`; POST failures have an explicit response. |
| **H6** | **Give every request a predictable result** (`http_server.hpp`) | Existing routes keep their semantics; invalid requests get `400`, unknown paths `404`, unsupported methods `405` (or an explicitly documented alternative); all responses have correct `Content-Length` and `Connection: close`. **Disable the unfinished gzip path** for now. |
| **H7** | **Add a minimal repeatable test suite** | `make test` (or one documented script) covers the five routes, malformed requests, fragmented POST body, concurrent clients, empty/binary files, bad paths, and a large enough response to exercise partial writes. Run with AddressSanitizer/UndefinedBehaviorSanitizer on Linux. |

The safest development approach is to add regression tests **alongside** H2–H6; H7 is the final verification gate, not an excuse to defer tests until the end.

## Medium priority — optional after the above

Choose only one or two for the next visit to the project; the rest can wait.

| ID | Improvement | Why / limit |
| --- | --- | --- |
| **M1** | Configurable `--port` (retain default `4221`) | Small quality-of-life feature; validate the input and update `make run` / README examples. |
| **M2** | Tighten header parsing and response construction | Preserve spaces in header values and repeated fields; centralize simple response serialization rather than add a general-purpose routing abstraction. |
| **M3** | Add `HEAD` for existing GET resources | Useful small HTTP feature; no additional content routes. Send the same metadata as GET without a response body. |
| **M4** | Refactor headers into `.hpp`/`.cpp`, introduce straightforward RAII, and run tests in CI | Improves maintainability once the core behavior is proven; not a rewrite. |

## Explicitly deferred

- HTTP/1.1 keep-alive, pipelining, and idle connection timers.
- gzip until compression round trips and `Content-Length` are tested (disable the existing path rather than advertise broken output).
- TLS/HTTPS, HTTP/2, HTTP/3, WebSockets, reverse proxying, and thread pools.
- Dynamic route registration, middleware, templating, database integrations, and benchmarks.

These may be reasonable **future projects**, but they are not required to complete the next iteration.

## Definition of done (small milestone)

The near-term milestone is complete when a clean Linux checkout can build and start with one command, the **five existing endpoints** pass automated integration tests, malformed requests return an error instead of crashing, uploads/downloads preserve bytes, and several simultaneous clients are served without one blocking the event loop. A sanitizer run on the covered paths should report no errors.

If all high-priority items are complete, **stop there**. This leaves a compact but sound foundation for later expansion.

## Further reading

- [Architecture and request lifecycle](docs/ARCHITECTURE.md)
- [Detailed known issues](docs/KNOWN_ISSUES.md)
- [Repository README](README.md)