#define _GNU_SOURCE
#include <errno.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define INOTIFY_MT "INOTIFY_HANDLE"
#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 256))

#ifndef LUA_OK
#define LUA_OK 0
#endif

typedef struct {
  int fd;
  int cb_table_ref;
} inotify_ctx_t;

static int push_luaerror(lua_State* L) {
  lua_pushnil(L);
  lua_pushstring(L, strerror(errno));
  lua_pushinteger(L, (lua_Integer)errno);
  return 3;
}

static void push_luaevent(lua_State* L, struct inotify_event* ev) {
  lua_newtable(L);

  lua_pushstring(L, "wd");
  lua_pushinteger(L, ev->wd);
  lua_settable(L, -3);

  lua_pushstring(L, "mask");
  lua_pushinteger(L, ev->mask);
  lua_settable(L, -3);

  lua_pushstring(L, "cookie");
  lua_pushinteger(L, ev->cookie);
  lua_settable(L, -3);

  lua_pushstring(L, "name");
  lua_pushstring(L, (ev->len > 0) ? ev->name : "");
  lua_settable(L, -3);
}

static int l_add(lua_State* L) {
  inotify_ctx_t* ctx = luaL_checkudata(L, 1, INOTIFY_MT);
  const char* path = luaL_checkstring(L, 2);
  uint32_t mask = (uint32_t)luaL_checkinteger(L, 3);
  luaL_checktype(L, 4, LUA_TFUNCTION);

  int wd = inotify_add_watch(ctx->fd, path, mask);
  if (wd < 0) return push_luaerror(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->cb_table_ref);
  lua_pushvalue(L, 4);
  lua_rawseti(L, -2, wd);
  lua_pop(L, 1);

  lua_pushinteger(L, wd);
  return 1;
}

static int l_remove(lua_State* L) {
  inotify_ctx_t* ctx = luaL_checkudata(L, 1, INOTIFY_MT);
  int wd = (int)luaL_checkinteger(L, 2);

  if (inotify_rm_watch(ctx->fd, wd) < 0) return push_luaerror(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->cb_table_ref);
  lua_pushnil(L);
  lua_rawseti(L, -2, wd);
  lua_pop(L, 1);

  lua_pushboolean(L, 1);
  return 1;
}

static int l_close(lua_State* L) {
  inotify_ctx_t* ctx = luaL_checkudata(L, 1, INOTIFY_MT);
  int ret = 0;

  if (ctx->fd >= 0) {
    if (ctx->cb_table_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, ctx->cb_table_ref);
      ctx->cb_table_ref = LUA_NOREF;
    }

    if ((ret = close(ctx->fd)) < 0) return push_luaerror(L);

    ctx->fd = -1;
  }

  lua_pushinteger(L, ret);
  return 1;
}

static int l_gc(lua_State* L) {
  inotify_ctx_t* ctx = luaL_checkudata(L, 1, INOTIFY_MT);

  if (ctx->fd >= 0) {
    if (ctx->cb_table_ref != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, ctx->cb_table_ref);
      ctx->cb_table_ref = LUA_NOREF;
    }
    close(ctx->fd);
    ctx->fd = -1;
  }

  return 0;
}

static int l_init(lua_State* L) {
  int fd = inotify_init1(IN_NONBLOCK);
  if (fd < 0) return push_luaerror(L);

  inotify_ctx_t* ctx = lua_newuserdata(L, sizeof(inotify_ctx_t));
  ctx->fd = fd;
  ctx->cb_table_ref = LUA_NOREF;

  luaL_getmetatable(L, INOTIFY_MT);
  lua_setmetatable(L, -2);

  lua_newtable(L);
  ctx->cb_table_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  return 1;
}

static int l_poll(lua_State* L) {
  inotify_ctx_t* ctx = luaL_checkudata(L, 1, INOTIFY_MT);
  int timeout = (int)luaL_optinteger(L, 2, -1);

  struct pollfd pfd = {
      .fd = ctx->fd,
      .events = POLLIN,
  };

  int poll_ret = poll(&pfd, 1, timeout);
  if (poll_ret < 0) return push_luaerror(L);

  if (poll_ret == 0) {
    lua_pushinteger(L, 0);
    return 1;
  }

  char buffer[EVENT_BUF_LEN];
  ssize_t bytes_read = read(ctx->fd, buffer, sizeof(buffer));
  if (bytes_read < 0) return push_luaerror(L);

  if (bytes_read == 0) {
    lua_pushinteger(L, 0);
    return 1;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->cb_table_ref);

  ssize_t offset = 0;
  while (offset < bytes_read) {
    struct inotify_event* ev = (struct inotify_event*)(buffer + offset);

    lua_rawgeti(L, -1, ev->wd);
    if (lua_isfunction(L, -1)) {
      push_luaevent(L, ev);
      if (lua_pcall(L, 1, 0, 0) != LUA_OK) return lua_error(L);
    } else {
      lua_pop(L, 1);
    }

    offset += sizeof(struct inotify_event) + ev->len;
  }

  lua_pop(L, 1);
  lua_pushinteger(L, poll_ret);
  return 1;
}

static const luaL_Reg methods[] = {{"__gc", l_gc},       {"add", l_add},
                                   {"poll", l_poll},     {"close", l_close},
                                   {"remove", l_remove}, {NULL, NULL}};

static const luaL_Reg lib[] = {{"init", l_init}, {NULL, NULL}};

#define ADD_CONST(L, name)  \
  lua_pushinteger(L, name); \
  lua_setfield(L, -2, #name);

int luaopen_inotify(lua_State* L) {
  luaL_newmetatable(L, INOTIFY_MT);

#if LUA_VERSION_NUM > 501
  luaL_setfuncs(L, methods, 0);
#else
  luaL_register(L, NULL, methods);
#endif

  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);

  lua_newtable(L);
#if LUA_VERSION_NUM > 501
  luaL_setfuncs(L, lib, 0);
#else
  luaL_register(L, NULL, lib);
#endif

  ADD_CONST(L, IN_ACCESS);
  ADD_CONST(L, IN_MODIFY);
  ADD_CONST(L, IN_ATTRIB);
  ADD_CONST(L, IN_CLOSE_WRITE);
  ADD_CONST(L, IN_CLOSE_NOWRITE);
  ADD_CONST(L, IN_OPEN);
  ADD_CONST(L, IN_MOVED_FROM);
  ADD_CONST(L, IN_MOVED_TO);
  ADD_CONST(L, IN_CREATE);
  ADD_CONST(L, IN_DELETE);
  ADD_CONST(L, IN_DELETE_SELF);
  ADD_CONST(L, IN_MOVE_SELF);
  ADD_CONST(L, IN_UNMOUNT);
  ADD_CONST(L, IN_Q_OVERFLOW);
  ADD_CONST(L, IN_IGNORED);
  ADD_CONST(L, IN_ONLYDIR);
  ADD_CONST(L, IN_DONT_FOLLOW);
  ADD_CONST(L, IN_EXCL_UNLINK);
  ADD_CONST(L, IN_MASK_ADD);
  ADD_CONST(L, IN_ISDIR);
  ADD_CONST(L, IN_ONESHOT);
  ADD_CONST(L, IN_CLOSE);
  ADD_CONST(L, IN_MOVE);
  ADD_CONST(L, IN_ALL_EVENTS);

  return 1;
}
