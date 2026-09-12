# MiniRedis

A lightweight, high-performance in-memory key-value database built from scratch in C++ using a single-threaded, non-blocking event-driven architecture.

---

## Architecture & Workflow

```text
+-------------------------------------------------------------------+
|                        Single-Threaded Loop                       |
|                                                                   |
|   [poll()]  --->  [Non-blocking Read]  --->  [Protocol Parser]    |
|      ^                                              |             |
|      |                                              v             |
|   [POLLOUT] <---  [Optimistic Write]   <---  [Command Engine]     |
|                                              (GET / SET / DEL)    |
+-------------------------------------------------------------------+
```

---

## Key Highlights

- **Event-Driven I/O Multiplexing**: Single-threaded event loop utilizing Linux `poll()` to handle concurrent connections efficiently without thread overhead.
- **Non-Blocking Sockets**: Sockets configured with `O_NONBLOCK` via `fcntl`, managing edge cases like `EAGAIN` / `EWOULDBLOCK` and signal interruptions (`EINTR`).
- **Pipelined Request Processing**: Drains batch requests from dynamic byte buffers and streams large payloads (up to 32MB) across multiple loop iterations.
- **Optimistic Non-Blocking Writes**: Bypasses the polling loop on immediate responses to eliminate redundant syscall roundtrips.
- **Binary-Safe Wire Protocol**: Robust length-prefixed framing format preventing boundary issues and injection attacks.

---

## Project Structure

```text
MiniRedis/
├── include/
│   ├── common.h          # Core definitions, status codes, socket I/O loops
│   ├── buffer.h          # Dynamic byte buffer manipulation (append / consume)
│   └── connection.h      # struct Conn per-connection state & lifecycle
├── src/
│   ├── common.cpp        # Non-blocking configuration & streaming routines
│   ├── buffer.cpp        # Buffer management implementation
│   ├── server.cpp        # Event loop, socket polling, and command dispatcher
│   └── client.cpp        # CLI test client supporting pipelining
├── Makefile              # Strict compiler flags (-Wall -Wextra -Werror -std=c++17)
└── README.md
```

---

## Build & Run

### Prerequisites
- GCC / G++ (C++17 or higher)
- Linux / POSIX environment

### Compilation
```bash
make
```

### Start Server
```bash
./server
```

### Run Client
```bash
./client
```

---

## Verification & Protocol Output

```text
> set mykey hello_redis 
(status) OK
> get mykey 
(string) "hello_redis"
> get non_existing_key 
(nil)
> set mykey updated_value 
(status) OK
> get mykey 
(string) "updated_value"
> del mykey 
(status) OK
> get mykey 
(nil)
> bad_command foo bar 
(error) unknown command or invalid arguments
```

---

## Roadmap

- [x] Socket Lifecycle & POSIX Network Programming
- [x] Length-Prefixed Binary Request-Response Protocol
- [x] Single-Threaded Event Loop with `poll()` & Non-Blocking Sockets
- [x] Pipelined Request Handling & Optimistic Writes
- [x] Core KV Store Engine (`GET`, `SET`, `DEL`)
- [ ] Custom Intrusive Hash Table with Incremental Rehashing
- [ ] TTL Key Expiration using Min-Heaps
- [ ] Sorted Sets with AVL Trees / Skip Lists
