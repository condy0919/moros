# moros

moros is a modern HTTP benchmark tool capable of generating significant load even when using single thread.

An optional plugin(using `shared library`) can perform HTTP request generating, response processing and more on.

## basic

```bash
moros http://example.org:8080/a.zip
```

This runs  a benchmark with the following default setting:

```
threads:        1
connections:    10
duration:       10s
timeout:        2s
```

## command line options

```
-H, --Header:       HTTP header
-t, --threads:      The number of HTTP benchers
-c, --connections:  The number of HTTP connections per bencher
-d, --duration:     Duration of the benchmark
-o, --timeout:      Mark HTTP request timeouted if HTTP response is not
                    received within this amount of time
```

## tips

Make sure file descriptors is enough. Use `ulimit -n unlimited`to handle this.

## todo

- [ ] statistic stat
- [ ] https support
- [ ] plugin support


