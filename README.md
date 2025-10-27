# linotify

A Lua binding for the Linux inotify library

## Building

To build `inotify.so`, simply type `make`.

## Usage

```lua
local inotify = require "inotify"
local signal = require "posix.signal"

local running = true
io.stdout:setvbuf "no"

signal.signal(signal.SIGINT, function()
    print("\nShutting down...")
    running = false
end)

local handle = inotify.init()

local wd, errmsg, errno = handle:add("/tmp", inotify.IN_CREATE | inotify.IN_DELETE, function(ev)
    print(string.format("Event: %s (mask: %d)", ev.name, ev.mask))
end)

if not wd then
    io.stderr:write(string.format("%s (%d)\n", errmsg, errno))
    os.exit(1)
end

while running do
    local count, errmsg, errno = handle:poll()
    if not count then
        io.stderr:write(string.format("%s (%d)\n", errmsg, errno))
    else
        print(string.format("Processed (%d) events", count))
    end
end

handle:close()
```

## 🧪 Testing

Run tests with Busted:

```bash
luarocks test
```
The test suite will create and remove temporary files in `/tmp`.
