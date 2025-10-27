local dir = require "pl.dir"
local path = require "pl.path"
local inotify = require "inotify"

local tmpdir = "/tmp/watchdog_test"

describe("watchdog C module", function()
  ---@type inotify_watcher
  local handle

  setup(function()
    if path.exists(tmpdir) then
      dir.rmtree(tmpdir)
    end
    dir.makepath(tmpdir)
    handle = inotify.init()
  end)

  teardown(function()
    handle:close()
    dir.rmtree(tmpdir)
  end)

  it("detects file creation", function()
    local triggered = false

    handle:add(tmpdir, inotify.IN_CREATE, function(ev)
      if ev.name == "newfile.txt" then
        triggered = true
      end
    end)

    local target = path.join(tmpdir, "newfile.txt")
    io.open(target, "w"):close()

    for _ = 1, 10 do
      handle:poll(100)
      if triggered then break end
    end

    assert.is_true(triggered)
  end)

  it("detects file deletion", function()
    local fname = "deleteme.txt"
    local fullpath = path.join(tmpdir, fname)
    local triggered = false

    io.open(fullpath, "w"):close()

    handle:add(tmpdir, inotify.IN_DELETE, function(ev)
      if ev.name == fname then
        triggered = true
      end
    end)

    os.remove(fullpath)

    for _ = 1, 10 do
      handle:poll(100)
      if triggered then break end
    end

    assert.is_true(triggered)
  end)

  it("detects file modification", function()
    local fname = "modme.txt"
    local fullpath = path.join(tmpdir, fname)
    local triggered = false

    local f = io.open(fullpath, "w")
    f:write("initial")
    f:close()

    handle:add(tmpdir, inotify.IN_MODIFY, function(ev)
      if ev.name == fname then
        triggered = true
      end
    end)

    local f2 = io.open(fullpath, "a")
    f2:write("more\n")
    f2:close()

    for _ = 1, 10 do
      handle:poll(100)
      if triggered then break end
    end

    assert.is_true(triggered)
  end)

  it("detects IN_DELETE_SELF when watching a file", function()
    local fname = "selfdelete.txt"
    local fullpath = path.join(tmpdir, fname)
    local triggered = false

    local f = io.open(fullpath, "w")
    f:write("bye")
    f:close()

    handle:add(fullpath, inotify.IN_DELETE_SELF, function(ev)
      triggered = true
    end)

    os.remove(fullpath)

    for _ = 1, 10 do
      handle:poll(100)
      if triggered then break end
    end

    assert.is_true(triggered)
  end)

  it("detects attribute change (IN_ATTRIB)", function()
    local fname = "attrib.txt"
    local fullpath = path.join(tmpdir, fname)
    local triggered = false

    io.open(fullpath, "w"):close()
    handle:add(tmpdir, inotify.IN_ATTRIB, function(ev)
      if ev.name == fname then triggered = true end
    end)

    os.execute(("touch %s"):format(fullpath))

    for _ = 1, 10 do
      handle:poll(100); if triggered then break end
    end
    assert.is_true(triggered)
  end)


  it("detects move self (IN_MOVE_SELF)", function()
    local dname = "moveself"
    local fullpath = path.join(tmpdir, dname)
    local moved = path.join(tmpdir, dname .. "_new")
    local triggered = false

    dir.makepath(fullpath)
    handle:add(fullpath, inotify.IN_MOVE_SELF, function(ev)
      triggered = true
    end)

    os.rename(fullpath, moved)

    for _ = 1, 10 do
      handle:poll(100); if triggered then break end
    end
    assert.is_true(triggered)
  end)


  it("detects rename (IN_MOVED_FROM / IN_MOVED_TO)", function()
    local from = "from.txt"
    local to = "to.txt"
    local from_path = path.join(tmpdir, from)
    local to_path = path.join(tmpdir, to)
    local moved_from, moved_to = false, false

    io.open(from_path, "w"):close()

    handle:add(tmpdir, inotify.IN_MOVED_FROM + inotify.IN_MOVED_TO, function(ev)
      if ev.name == from and (ev.mask & inotify.IN_MOVED_FROM) ~= 0 then
        moved_from = true
      elseif ev.name == to and (ev.mask & inotify.IN_MOVED_TO) ~= 0 then
        moved_to = true
      end
    end)


    os.rename(from_path, to_path)

    for _ = 1, 10 do
      handle:poll(100); if moved_from and moved_to then break end
    end

    assert.is_true(moved_from)
    assert.is_true(moved_to)
  end)


  it("detects open and close_write", function()
    local fname = "openclose.txt"
    local fullpath = path.join(tmpdir, fname)
    local opened, closed = false, false

    io.open(fullpath, "w"):close()
    handle:add(tmpdir, inotify.IN_OPEN | inotify.IN_CLOSE_WRITE, function(ev)
      if ev.name == fname and (ev.mask & inotify.IN_OPEN) ~= 0 then
        opened = true
      end
      if ev.name == fname and (ev.mask & inotify.IN_CLOSE_WRITE) ~= 0 then
        closed = true
      end
    end)


    local f = io.open(fullpath, "a")
    f:write("x")
    f:close()

    for _ = 1, 10 do
      handle:poll(100); if opened and closed then break end
    end

    assert.is_true(opened)
    assert.is_true(closed)
  end)

  it("can remove a watch", function()
    local fname = "removewatch.txt"
    local fullpath = path.join(tmpdir, fname)
    local triggered = false

    io.open(fullpath, "w"):close()

    local watch_fd = handle:add(tmpdir, inotify.IN_DELETE, function(ev)
      triggered = true
    end)

    handle:remove(watch_fd)
    os.remove(fullpath)

    for _ = 1, 10 do handle:poll(100); end

    assert.is_false(triggered)
  end)
end)
