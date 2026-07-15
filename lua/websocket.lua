-- Полный GPT код
local websocket = require "resty.websocket.server"
local ffi = require "ffi"

local wb, err = websocket:new {
  timeout = 60000,
  max_payload_len = 16 * 1024 * 1024
}

if not wb then
  ngx.log(ngx.ERR, "websocket init failed: ", err)
  return ngx.exit(500)
end


local backend = ngx.socket.tcp()
backend:settimeouts(60000, 60000, 60000)

local ok, err = backend:connect("127.0.0.1", 9000)

if not ok then
  ngx.log(ngx.ERR, "backend connect failed: ", err)
  return ngx.exit(502)
end


local function pack_u64_le(n)
  local buf = ffi.new("uint64_t[1]", n)
  return ffi.string(buf, 8)
end


local function unpack_u64_le(str)
  local buf = ffi.new("uint64_t[1]")
  ffi.copy(buf, str, 8)
  return tonumber(buf[0])
end


-- WS -> Backend
local function ws_reader()
  while true do
    local data, typ, err = wb:recv_frame()

    if not data then
      break
    end

    if typ == "close" then
      break
    end

    if typ == "ping" then
      wb:send_pong(data)
      goto continue
    end

    if typ ~= "text" and typ ~= "binary" then
      goto continue
    end

    if #data < 8 then
      ngx.log(ngx.ERR, "invalid message")
      break
    end


    -- первые 8 байт payload = type
    local msg_type = data:sub(1, 8)

    local packet =
        msg_type ..
        pack_u64_le(#data) ..
        data


    local ok, err = backend:send(packet)

    if not ok then
      ngx.log(ngx.ERR, "backend write failed: ", err)
      break
    end

    ::continue::
  end
end


-- Backend -> WS
local function backend_reader()
  while true do
    -- type
    local type_raw, err = backend:receive(8)

    if not type_raw then
      break
    end


    -- length
    local len_raw, err = backend:receive(8)

    if not len_raw then
      break
    end


    local len = unpack_u64_le(len_raw)


    if len > 16 * 1024 * 1024 then
      ngx.log(ngx.ERR, "message too large")
      break
    end


    local data, err = backend:receive(len)

    if not data then
      break
    end


    -- type можно использовать при необходимости
    local typ = type_raw


    -- отправляем клиенту
    local ok, err = wb:send_binary(data)

    if not ok then
      ngx.log(ngx.ERR, "websocket send failed: ", err)
      break
    end
  end
end


-- параллельно запускаем две стороны
local t1 = ngx.thread.spawn(ws_reader)
local t2 = ngx.thread.spawn(backend_reader)


ngx.thread.wait(t1, t2)


backend:close()
wb:send_close()
