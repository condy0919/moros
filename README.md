|CI          |Host OS    |Build Status|
|------------|-----------|------------|
|**CircleCi**|Debian 9   |[![CircleCI](https://circleci.com/gh/condy0919/moros.svg?style=svg)](https://circleci.com/gh/condy0919/moros)|

# moros

moros is a modern HTTP benchmark tool capable of generating significant load even when using single thread.

An optional plugin(using `shared library`) can perform HTTP request generating, response processing and more on.

## Basic

```bash
moros http://localhost/a.zip
```

This runs  a benchmark with the following default setting:

```
threads:        1
connections:    10
duration:       10s
timeout:        2s
```

Output:

```
Running 10s test @ http://localhost/a.zip
  1 thread(s) and 10 connection(s) each
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency      0ms        0ms     5ms     99.9979%
    Req/Sec   46.53K      3.23K  51.17K          61%
  473743 requests in 10s, 3.72GB read
Requests/sec: 47374.3
Transfer/sec: 380.56MB
```

## Command Line Options

```
-p, --plugin:       Load plugin
-H, --Header:       HTTP header
-t, --threads:      The number of HTTP benchers
-c, --connections:  The number of HTTP connections per bencher
-d, --duration:     Duration of the benchmark
-T, --timeout:      Mark HTTP request timeouted if HTTP response is not
                    received within this amount of time
-l, --latency:      Print latency distribution
```

## Tips

Make sure file descriptors is enough. Use `ulimit -n unlimited`to handle this.

## Installation

Arch Linux
```bash
yay -S moros-git
```

The other can build from source.

`moros`depends on`boost.program_options`and`openssl`library.

Install these 2 packages first.
```bash
git clone https://github.com/condy0919/moros

cd moros
git submodule update --init --recursive # nodejs/http-parser

cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
The`moros`binary will be placed in `moros/bin/`.

## Plugin Interface
* **setup()**

  _setup_ begins before the target address has been resolved and all benchers uninitialized.

* **init()**

  _init_ begins after each bencher initialized.

* **request(schema, host, port, serivce, query\_string, headers[])**

  _request_ generates a new HTTP request each time, which is expensive.

  Generating lots of HTTP requests one time and amortizing the cost is a good solution.

* **response(status, headers[], body, body\_len)**

  _response_ is called with HTTP response status, headers[], body.

  In default, only status is valid.

  Making `want_response_headers` and `want_response_body` symbols

  visible in plugin to process headers and body.

* **summary()**

  _summary_ can report some data collected in above functions.

## Todo

- [x] statistic stat
- [ ] constant benchmark rate
- [x] https support
- [x] plugin support

