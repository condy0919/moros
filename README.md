|CI          |Host OS    |Build Status|
|------------|-----------|------------|
|**CircleCi**|Debian 9   |[![CircleCI](https://circleci.com/gh/condy0919/moros.svg?style=svg)](https://circleci.com/gh/condy0919/moros)|

# moros

moros is a modern HTTP benchmark tool capable of generating significant load even when using single thread.

An optional plugin(using `shared library`) can perform HTTP request generating, response processing and more on.

## basic

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

## command line options

```
-H, --Header:       HTTP header
-t, --threads:      The number of HTTP benchers
-c, --connections:  The number of HTTP connections per bencher
-d, --duration:     Duration of the benchmark
-T, --timeout:      Mark HTTP request timeouted if HTTP response is not
                    received within this amount of time
-l, --latency:      Print latency distribution
```

## tips

Make sure file descriptors is enough. Use `ulimit -n unlimited`to handle this.

## installl

Arch Linux
```bash
yay -S moros-git
```

The others can build from source.
`moros`depends on`boost.program_options`and`openssl`library.
Install these two packages first.
```bash
git clone https://github.com/condy0919/moros
git submodule update --init --recursive # nodejs/http-parser

cd moros
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
The moros binary will be placed in `moros/bin/`.

## todo

- [x] statistic stat
- [ ] follow 302 setting
- [ ] constant benchmark rate
- [x] https support
- [ ] plugin support


