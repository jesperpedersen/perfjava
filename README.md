libperfjava will generate a `/tmp/perf-<pid>.map` file for Java JITted methods to use with `perf`.

## Requirements

* Linux perf
* OpenJDK 8u60b19 or higher
* [http://github.com/brendangregg/FlameGraph](http://github.com/brendangregg/FlameGraph "FlameGraph")

## Build

```bash
cmake .
make
```

## Usage

libperfjava will record JITed methods during the lifetime of the JVM with

```bash
perf record -F 99 -g java -agentpath:/path/to/libperfjava.so -XX:+PreserveFramePointer ...
```

Then the information can be turned into a flame graph using

```bash
perf script | /path/to/FlameGraph/stackcollapse-perf.pl > /tmp/out.perf-folded
/path/to/FlameGraph/flamegraph.pl --color=java /tmp/out.perf-folded > perfjava.svg
```

The result `perfjava.svg` can be viewed with any SVG viewer, like Firefox.

## Configuration

libperfjava supports the following options

* `flush`: Should a flush happen for each entry written. Default is `0`, i.e. flush will happen upon agent unload.
* `unfold`: Should all frames be unfolded. Default is `0`.

For example

```bash
perf record -F 99 -g java -agentpath:./libperfjava.so=flush=0,unfold=1 -XX:+PreserveFramePointer ...
```

## Thanks to

* [Johannes Rudolph](http://github.com/jrudolph "Johannes Rudolph")
* [Brendan Gregg](http://github.com/brendangregg "Brendan Gregg")

## License

This project is licensed under GPL v2. See the LICENSE file.
