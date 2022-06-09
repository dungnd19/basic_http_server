# Basic HTTP/1.1 server

Implementation of a basic HTTP server in C++

## Features

- Can handle multiple concurrent connections, tested up to 10k.
- Support basic HTTP request and response. Provide an extensible framework to implement other HTTP features.
- HTTP/1.1: Persistent connection is enabled by default.

## Quick start

```bash
mkdir build && cd build
cmake ..
make
./basic_http_server      # Start the HTTP server on port 8080
```

- There are two endpoints available at `/` and `/hello.html` which are created for demo purpose.
```bash
http://0.0.0.0:8080/
http://0.0.0.0:8080/hello.html
```
- In order to have multiple concurrent connections, make sure to raise the resource limit (with `ulimit`) before running the server. A non-root user by default can have about 1000 file descriptors opened, which corresponds to 1000 active clients.
```bash
ulimit -n 655350
```
## Design

The server program consists of:

- 1 main thread for user interaction.
- 1 listener thread to accept incoming clients.
- 10 worker threads to process HTTP requests and sends response back to client.
- Utility functions to parse and manipulate HTTP requests and repsonses conveniently.

## Benchmark
Test environment
```bash
OS: Ubuntu 22.04 LTS x86_64
Host: Inspiron 7548 A08
Kernel: 5.15.0-37-generic
CPU: Intel i5-5200U (4) @ 2.700GHz
GPU: Intel HD Graphics 5500
GPU: AMD ATI Radeon R7 M265/M365X/M4
Memory: 2137MiB / 11851MiB
```

Using wrk tool for benchmarking server [https://github.com/wg/wrk]

```bash
wrk -t10 -c10000 -d60s -H --timeout30s  http://0.0.0.0:8080/
Running 1m test @ http://0.0.0.0:8080/
  10 threads and 10000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    22.72ms   13.64ms 260.09ms   69.09%
    Req/Sec     6.34k     5.42k   29.87k    86.19%
  2648023 requests in 1.00m, 196.98MB read
Requests/sec:  44064.90
Transfer/sec:      3.28MB
```

With stronger CPU (Ex: ThinkPad E14 can reach 10000 request/sec)
