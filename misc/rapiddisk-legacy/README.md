Description
------------------

The `rapiddisk-legacy.sh` shell script provides users with an interface to use
pre-7.0.0 parameters and functions. It is a wrapper to the updated and new
rapiddisk binary.

Installation
------------------

Copy the shell script anywhere but most preferrably to a directory in your current
working path. It is expected that you have already installed the rapiddisk binary
in a current working path as it directly calls on this binary for execution.

Execution Examples
------------------

Help menu:

```
# sh rapiddisk-legacy.sh --help
```

Attach a device:
```
# sh rapiddisk-legacy.sh --attach 64
```

Detach a device:
```
# sh rapiddisk-legacy.sh --detach rd0
```

Flush a device:
```
# sh rapiddisk-legacy.sh --flush rd0
```

Resize a device:
```
# sh rapiddisk-legacy.sh --resize rd0 128
```

Map a ram drive as a cache to a backend device:
```
# sh rapiddisk-legacy.sh --cache-map rd1 /dev/sdb
```

Map a ram drive as a cache to a backend device with a specific caching policy:
```
# sh rapiddisk-legacy.sh --cache-map rd1 /dev/sdb wt
```

Unmap a ram drive from a backend device:
```
# sh rapiddisk-legacy.sh --cache-map rd1 /dev/sdb wt
```

View caching statistics from a cache mapping:
```
# sh rapiddisk-legacy.sh --stat-cache rc_sdb
```

