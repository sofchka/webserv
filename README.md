*This project has been created as part of the 42 curriculum by argharag, apetoyan, szakarya.*

# Webserv

## Description

Webserv is a custom HTTP/1.1 web server written in C++98 as part of the 42 curriculum.

The goal of this project is to understand how web servers work internally by implementing the network layer, HTTP request parsing, response generation, configuration handling, client management, file serving, uploads, and CGI execution without relying on an existing web server.

The server supports multiple listening sockets and configurable virtual servers and locations.

### Main features

* HTTP/1.1 request handling
* Non-blocking socket I/O
* `select()` for monitoring server and client sockets
* Multiple listening ports
* Configuration file parsing
* Static website hosting
* `GET`, `POST`, and `DELETE` methods
* HTTP status codes and error pages
* File uploads
* Location-based configuration
* Directory auto-indexing
* HTTP redirects
* CGI execution
* Configurable maximum request body size

## Instructions

### Requirements

* Linux
* C++ compiler with C++98 support
* `make`
* CGI interpreter if CGI is enabled in the configuration

### Compilation

Clone the repository and enter the project directory:

```bash
git clone <repository-url>
cd webserv
```

Compile the project:

```bash
make
```

This creates the `webserv` executable.

### Running the server

Run the server with the default configuration:

```bash
./webserv
```

Or provide a configuration file:

```bash
./webserv conf/default.conf
```

The configuration file defines listening addresses and ports, server settings, locations, allowed methods, document roots, error pages, uploads, redirects, autoindexing, and CGI handlers.

### Testing

For a simple GET request:

```bash
curl http://localhost:8080/
```

To inspect HTTP headers:

```bash
curl -i http://localhost:8080/
```

To test a specific path:

```bash
curl -i http://localhost:8080/index.html
```

The server can also be tested using a standard web browser.

## Configuration

A configuration file is passed to the server as a command-line argument.

Example structure:

```text
server {
    listen 0.0.0.0:8080;

    client_max_body_size 1000000;

    location / {
        root web;
        index index.html;
        methods GET POST DELETE;
    }

    location /upload {
        root web/uploads;
        methods POST;
        upload on;
        upload_store web/uploads;
    }
}
```

The exact supported directives depend on the configuration parser implemented in the project.

## HTTP Methods

### GET

Used to retrieve static files and resources.

```bash
curl -i http://localhost:8080/
```

### POST

Used for sending data to the server, including upload operations where configured.

```bash
curl -X POST -d "hello world" http://localhost:8080/upload
```

### DELETE

Used to remove resources where the method is enabled by the corresponding location.

```bash
curl -X DELETE http://localhost:8080/file.txt
```

## CGI

CGI (Common Gateway Interface) allows the server to execute external programs to generate dynamic HTTP content.

The server can associate file extensions with CGI executables through the configuration.

For example:

```text
cgi .py /usr/bin/python3;
```

When a request targets a configured CGI resource, the server creates a child process and executes the configured CGI interpreter using `execve()`.

## Technical Choices

### C++98

The project is implemented using C++98, following the requirements of the 42 curriculum.

### Non-blocking I/O

Server and client sockets are configured as non-blocking.

The server uses `select()` to monitor socket readiness instead of blocking on individual clients.

This allows multiple clients to be handled by a single server process.

### Client buffers

Each client has request and response buffering so that HTTP data can be received and sent over multiple socket operations.

### Configuration

The configuration parser separates server-level and location-level settings and allows multiple server configurations to listen on different ports.

## Project Structure

```text
.
├── conf/
│   └── default.conf
├── includes/
│   ├── Config.hpp
│   └── Server.hpp
├── srcs/
│   ├── main.cpp
│   ├── Server.cpp
│   ├── parse_f.cpp
│   ├── make_response.cpp
│   ├── get_type.cpp
│   ├── utils.cpp
│   └── config/
├── web/
│   ├── index.html
│   └── ...
├── Makefile
└── README.md
```

## Resources

### HTTP

* RFC 9110 — HTTP Semantics
* RFC 9112 — HTTP/1.1
* MDN Web Docs — HTTP overview

### Linux networking

* `socket(2)`
* `bind(2)`
* `listen(2)`
* `accept(2)`
* `recv(2)`
* `send(2)`
* `select(2)`
* `fcntl(2)`

### CGI

* CGI specification and documentation
* Linux `execve(2)` documentation
* Linux `fork(2)` documentation
* Linux `pipe(2)` documentation

### C++

* C++98 standard library documentation
* `std::vector`
* `std::map`
* `std::string`
* `std::ifstream`

## AI Usage

AI tools were used as a development and learning aid during the project.

AI was used for:

* explaining HTTP and TCP concepts;
* understanding socket APIs such as `socket`, `bind`, `listen`, `accept`, `recv`, `send`, and `select`;
* debugging compiler and linker errors;
* reviewing server architecture;
* explaining CGI, `fork()`, `execve()`, and pipes;
* helping identify edge cases in HTTP request parsing;
* discussing non-blocking I/O and client buffering;
* improving documentation and README structure.

The project code, architecture, configuration parser, HTTP handling, and integration were developed and tested by the project authors. AI assistance was used for explanations, debugging guidance, and development support rather than as a replacement for understanding or testing the implementation.

