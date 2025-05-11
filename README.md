# Employee Database Server & Client

## Project Overview

This project implements a client-server application for managing a simple employee database. The server is written in C and utilizes TCP sockets for network communication and the `poll()` system call to handle multiple client connections concurrently. It features a custom binary protocol for client-server interaction and stores employee data in a local file.

This project was developed as an exercise in C network programming, custom protocol design, and concurrent server architecture.

## Features

**Server (`dbserver`):**
*   **TCP/IP Networking:** Listens for incoming client connections on a configurable port.
*   **Concurrent Client Handling:** Uses `poll()` to manage multiple connected clients efficiently without threads or forking.
*   **Custom Binary Protocol:** Implements a defined protocol for operations like:
    *   Client Hello / Handshake
    *   Adding new employee records
    *   (Potentially: Querying, Updating, Deleting employees)
*   **File-Based Data Storage:**
    *   Saves and loads employee records from a binary file.
    *   Includes a database header for metadata (e.g., record count, version).
*   **State Machine:** Manages client sessions through different states (e.g., `STATE_HELLO`, `STATE_MSG`).
*   **Basic Error Handling:** Includes checks for network operations and protocol adherence.

**Client (`dbcli` - *jeśli jest częścią repozytorium*):**
*   Connects to the server via TCP.
*   Implements the client-side of the custom binary protocol.
*   Allows users to send requests to the server, such as adding new employees.
*   (Potentially: Provides a command-line interface for various database operations)

## Technical Details

*   **Language:** C (C99/C11 standard)
*   **Networking:** POSIX Sockets (TCP/IP)
*   **Concurrency Model:** Single-threaded, event-driven using `poll()`
*   **Protocol:** Custom binary protocol (details in `common.h` or a separate protocol specification document - *jeśli masz*)
*   **Data Persistence:** Binary file storage.
*   **Build System:** `Makefile` (or specify if different)

## Getting Started

### Prerequisites

*   A C compiler (e.g., GCC, Clang)
*   `make` (or your build tool)
*   Standard POSIX development environment

### Compilation

1.  Clone the repository:
    ```bash
    git clone https://github.com/MateuszSzczukiewicz/Employee-Database.git
    cd Employee-Database
    ```
2.  Build the project:
    ```bash
    make
    ```
    This will produce the server executable (e.g., `bin/dbserver`) and a client executable (e.g., `bin/dbcli`).

### Running the Server

```bash
./bin/dbserver -f <database_file_path> -p <port_number> [-n]
```
**Options:**
*   `-f <database_file_path>`: (Required) Path to the database file.
*   `-p <port_number>`: (Required) Port number for the server to listen on.
*   `-n`: (Optional) Create a new database file. If the file exists and `-n` is specified, an error will occur.
*   `-h`: Display help message.

**Example:**
```bash
./bin/dbserver -f mycompany.db -p 8080 -n
```

### Running the Client
```bash
./bin/dbcli -h <server_host> -p <server_port> [command_options]
```
**Example:**
```bash
./bin/dbcli -h 127.0.0.1 -p 8080 -a "John Doe,123 Main St,40"
```
## Protocol Specification (Brief)

Messages consist of a header (`dbproto_hdr_t`) followed by an optional payload.
*   **Header:**
    *   `type` (e.g., `MSG_HELLO_REQ`, `MSG_EMPLOYEE_ADD_REQ`)
    *   `len` (length of the payload, e.g., number of records or specific meaning)
*   **Payload:** Varies depending on the message type.

**Example Flow (Adding Employee):**
1.  Client -> Server: `MSG_HELLO_REQ` (with protocol version)
2.  Server -> Client: `MSG_HELLO_RESP` (or `MSG_ERROR`)
3.  Client -> Server: `MSG_EMPLOYEE_ADD_REQ` (with employee data)
4.  Server -> Client: `MSG_EMPLOYEE_ADD_RESP` (or `MSG_ERROR`)

## Future Enhancements / TODO

*   Implement full CRUD (Create, Read, Update, Delete) operations for employees.
*   Add more robust error handling and reporting on both client and server.
*   Implement a more detailed client-side command-line interface.
*   Add support for querying/filtering employee records.
*   Improve database file format for more efficient access or updates.
*   Write unit tests.
*   Consider security aspects (e.g., authentication, encryption - though likely out of scope for a basic project).
---
