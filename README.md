libslowmydata
============

An LD_PRELOAD library that slows down all forms of opening files.

The whole library is a dumb form of the venerable libeatmydata, but this one slows down disk access instead of making is faster. This is used at this point for benchmarking stuff on a SSD, so a slower disk would have been useful.

see http://www.flamingspork.com/projects/libeatmydata

Usage
-----

```
LD_PRELOAD=./.libs/libeatmydata.so LIBSLOWMYDATA_ON_OPEN_SLEEP=1.0 LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH="*.*" cat ./conf
```


Explanations:
-----
- LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH filename pattern (`fnmatch()` is used) on which the sleep will occur
- LIBSLOWMYDATA_ON_OPEN_SLEEP : how much to sleep when fopen is called


The project still contains a lot of files from the original libeatmydata project, as my C-fu is extremly rusty and I have very little experience with `make` & friends.

