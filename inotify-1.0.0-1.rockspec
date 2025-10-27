rockspec_format = "3.0"
package = "inotify"
version = "1.0.0-1"
source = {
  url = 'git://github.com/kayibea/linotify.git',
}
description = {
  summary  = "Inotify bindings for Lua.",
  detailed = "A fast and minimal Lua module that wraps the Linux `inotify` API.",
  homepage = "https://github.com/kayibea/linotify",
  license  = "MIT"
}

supported_platforms = {
  "linux"
}

dependencies = {
  "lua >= 5.1"
}
test_dependencies = {
  "busted >= 2.2.0-1",
  "penlight >= 1.14.0-3"
}

build = {
  type = "builtin",
  modules = {
    inotify = {
      sources = "linotify.c",
    }
  }
}
