# Fuzzy Sync

This is a standalone version of the Linux Test Project's Fuzzy Sync C
library. The original was developed to reliably and quickly reproduce
data races within the Linux kernel from an unprivileged user land test
process.

This version was manually extracted from the LTP. The original can be
found at the following link. The original is stable now, but still
receives updates
https://github.com/linux-test-project/ltp/blob/master/include/tst_fuzzy_sync.h

## Usage

The entire library is contained within a single header, either copy it
into your project or fork this project and extend it with more
executables.

To build this project and run the example test do.

```
$ mkdir build && cd build
$ cmake .. [-DCMAKE_BUILD_TYPE=Debug]
$ cmake --build .
$ ctest -V					# or make test
```

You may then continue by copying `test/a_rare_data_race.c` and the
relevant part of `CMakeLists.txt` to add new tests. E.g

```
$ cp test/a_rare_data_race.c test/a_new_test.c
$ echo "fzsync_test(a_new_test)" >> CMakeLists.txt
```

## Further info

The test is derived from the example given in the following article
https://richiejp.com/a-rare-data-race
