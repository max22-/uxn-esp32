local replacements = {
  op_and16 = '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, d & b); push8(u->src, c & a); }',
  op_ora16 = '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, d | b); push8(u->src, c | a); }',
  op_eor16 = '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, d ^ b); push8(u->src, c ^ a); }',
  op_lit16 = '{ push8(u->src, mempeek8(u->ram.dat, u->ram.ptr++)); push8(u->src, mempeek8(u->ram.dat, u->ram.ptr++)); }',
  op_swp16 = '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, b); push8(u->src, a); push8(u->src, d); push8(u->src, c); }',
  op_ovr16 = '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, d); push8(u->src, c); push8(u->src, b); push8(u->src, a); push8(u->src, d); push8(u->src, c); }',
  op_dup16 = '{ Uint8 a = pop8(u->src), b = pop8(u->src); push8(u->src, b); push8(u->src, a); push8(u->src, b); push8(u->src, a); }',
  op_rot16 = '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src), e = pop8(u->src), f = pop8(u->src); push8(u->src, d); push8(u->src, c); push8(u->src, b); push8(u->src, a); push8(u->src, f); push8(u->src, e); }',
  op_sth16 = '{ Uint8 a = pop8(u->src), b = pop8(u->src); push8(u->dst, b); push8(u->dst, a); }'
}
local top, bottom, pushtop
local offset
offset = function(n, s)
  if s == nil then
    s = ''
  end
  if n < 0 then
    return (' -%s %d'):format(s, -n)
  elseif n > 0 then
    return (' +%s %d'):format(s, n)
  elseif s ~= '' then
    return (' +%s 0'):format(s)
  else
    return ''
  end
end
local pop_push
pop_push = function(k, n, s)
  local _exp_0 = k
  if 'pop' == _exp_0 then
    s = s:match('^%((%S+)%)$')
    assert(s == 'src')
    local _exp_1 = n
    if '8' == _exp_1 then
      top[s] = top[s] - 1
      if bottom[s] > top[s] then
        bottom[s] = top[s]
      end
      return ('%s.dat[%s.ptr%s]'):format(s, s, offset(top[s]))
    elseif '16' == _exp_1 then
      top[s] = top[s] - 2
      if bottom[s] > top[s] then
        bottom[s] = top[s]
      end
      return ('(%s.dat[%s.ptr%s] | (%s.dat[%s.ptr%s] << 8))'):format(s, s, offset(top[s] + 1), s, s, offset(top[s]))
    end
  elseif 'push' == _exp_0 then
    local v
    s, v = s:match('^%((%S+), (.*)%)$')
    assert(s == 'src' or s == 'dst', s)
    local _exp_1 = n
    if '8' == _exp_1 then
      pushtop[s] = pushtop[s] + 1
      return ('%s.dat[%s.ptr%s] = %s'):format(s, s, offset(pushtop[s] - 1), v)
    elseif '16' == _exp_1 then
      if v:match('%+%+') or v:match('%-%-') then
        error('push16 has side effects: ' .. v)
      end
      local peek, args = v:match('^([md]e[mv]peek)16(%b())$')
      if peek then
        args = args:sub(2, -2)
        return pop_push('push', '8', ('(%s, %s8(%s))'):format(s, peek, args)) .. ';\n' .. pop_push('push', '8', ('(%s, %s8(%s + 1))'):format(s, peek, args))
      end
      pushtop[s] = pushtop[s] + 2
      if v:match(' ') then
        v = '(' .. v .. ')'
      end
      return ('%s.dat[%s.ptr%s] = %s >> 8;\n%s.dat[%s.ptr%s] = %s & 0xff'):format(s, s, offset(pushtop[s] - 2), v, s, s, offset(pushtop[s] - 1), v)
    end
  else
    return nil
  end
end
local indented_block
indented_block = function(s)
  s = s:gsub('^%{ *', '{\n'):gsub('\n', '\n\t'):gsub('\t%} *$', '}\n')
  s = s:gsub('\n[^\n]+%.error = [^\n]+', '%0\n#ifndef NO_STACK_CHECKS\n\tgoto error;\n#endif')
  return s
end
local process
process = function(body)
  local out_body = body:gsub('^%{ *', ''):gsub(' *%}$', ''):gsub('; ', ';\n'):gsub('%b{} *', indented_block):gsub('(%a+)(%d+)(%b())', pop_push)
  local in_ifdef = false
  local _list_0 = {
    'src',
    'dst'
  }
  for _index_0 = 1, #_list_0 do
    local k = _list_0[_index_0]
    if bottom[k] ~= 0 then
      if not in_ifdef then
        out_body = out_body .. '\n#ifndef NO_STACK_CHECKS'
        in_ifdef = true
      end
      out_body = out_body .. ('\nif(__builtin_expect(%s.ptr < %d, 0)) {\n\t%s.error = 1;\n\tgoto error;\n}'):format(k, -bottom[k], k)
    end
    if pushtop[k] ~= 0 then
      if pushtop[k] > 0 then
        if not in_ifdef then
          out_body = out_body .. '\n#ifndef NO_STACK_CHECKS'
          in_ifdef = true
        end
        out_body = out_body .. ('\nif(__builtin_expect(%s.ptr > %d, 0)) {\n\t%s.error = 2;\n\tgoto error;\n}'):format(k, 255 - pushtop[k], k)
      end
      if in_ifdef then
        out_body = out_body .. '\n#endif'
        in_ifdef = false
      end
      out_body = out_body .. ('\n%s.ptr %s= %d;'):format(k, pushtop[k] < 0 and '-' or '+', math.abs(pushtop[k]))
    end
  end
  if in_ifdef then
    out_body = out_body .. '\n#endif'
    in_ifdef = false
  end
  local t = { }
  out_body:gsub('[^%w_]([a-f]) = (src%.dat%[[^]]+%])[,;]', function(v, k)
    t[k] = v
  end)
  out_body = out_body:gsub('(src%.dat%[[^]]+%]) = ([a-f]);\n', function(k, v)
    if t[k] and t[k] == v then
      return ''
    end
    return nil
  end)
  return out_body
end
local ops = { }
for l in assert(io.lines('src/uxn.c')) do
  local _continue_0 = false
  repeat
    local name, body = l:match('void (op_%S*)%(Uxn %*u%) (%b{})')
    if not name then
      _continue_0 = true
      break
    end
    if replacements[name] then
      body = replacements[name]
    end
    body = body:gsub('u%-%>src%-%>', 'src.')
    body = body:gsub('u%-%>dst%-%>', 'dst.')
    body = body:gsub('u%-%>src', 'src')
    body = body:gsub('u%-%>dst', 'dst')
    top = {
      src = 0,
      dst = 0
    }
    bottom = {
      src = 0,
      dst = 0
    }
    pushtop = top
    ops[name] = process(body)
    top = {
      src = 0,
      dst = 0
    }
    bottom = {
      src = 0,
      dst = 0
    }
    pushtop = {
      src = 0,
      dst = 0
    }
    ops['keep_' .. name] = process(body)
    _continue_0 = true
  until true
  if not _continue_0 then
    break
  end
end
local dump
dump = function(s, src, dst)
  local ret = '\t\t\t{\n'
  for l in s:gmatch('[^\n]+') do
    if not l:match('^%#') then
      ret = ret .. '\t\t\t\t'
    end
    ret = ret .. ('%s\n'):format(l)
  end
  ret = ret .. '\t\t\t}\n\t\t\tbreak;\n'
  return (ret:gsub('src', src):gsub('dst', dst))
end
local i = 0
local allops = { }
local wanted = false
for l in assert(io.lines('src/uxn.c')) do
  if l == 'void (*ops[])(Uxn *u) = {' then
    wanted = true
  elseif l == '};' then
    wanted = false
  elseif wanted then
    l = l:gsub('%/%b**%/', '')
    for op in l:gmatch('[%w_]+') do
      if not ops[op] then
        error('missing ' .. op)
      end
      allops[i + 0x00 + 1] = {
        n = {
          i + 0x00
        },
        body = dump(ops[op], 'u->wst', 'u->rst')
      }
      allops[i + 0x40 + 1] = {
        n = {
          i + 0x40
        },
        body = dump(ops[op], 'u->rst', 'u->wst')
      }
      allops[i + 0x80 + 1] = {
        n = {
          i + 0x80
        },
        body = dump(ops['keep_' .. op], 'u->wst', 'u->rst')
      }
      allops[i + 0xc0 + 1] = {
        n = {
          i + 0xc0
        },
        body = dump(ops['keep_' .. op], 'u->rst', 'u->wst')
      }
      i = i + 1
    end
  end
end
i = 0
wanted = false
for l in assert(io.lines('src/uxnasm.c')) do
  if l == 'static char ops[][4] = {' then
    wanted = true
  elseif l == '};' then
    wanted = false
  elseif wanted then
    for op in l:gmatch('"(...)"') do
      i = i + 1
      allops[i + 0x00].name = op
      allops[i + 0x20].name = op .. '2'
      allops[i + 0x40].name = op .. 'r'
      allops[i + 0x60].name = op .. '2r'
      allops[i + 0x80].name = op .. 'k'
      allops[i + 0xa0].name = op .. '2k'
      allops[i + 0xc0].name = op .. 'kr'
      allops[i + 0xe0].name = op .. '2kr'
    end
  end
end
for i = 1, 256 do
  local _continue_0 = false
  repeat
    if not allops[i] then
      _continue_0 = true
      break
    end
    for j = i + 1, 256 do
      if allops[i].body == allops[j].body then
        table.insert(allops[i].n, (table.remove(allops[j].n)))
        allops[j].body = nil
      end
    end
    _continue_0 = true
  until true
  if not _continue_0 then
    break
  end
end
do
  local _with_0 = assert(io.open('src/uxn-fast.c', 'w'))
  local f = assert(io.open('src/uxn.c'))
  while true do
    local l = f:read('*l')
    _with_0:write(('%s\n'):format(l))
    if l == '*/' then
      break
    end
  end
  _with_0:write('\n')
  _with_0:write([[/*
 ^
/!\ THIS FILE IS AUTOMATICALLY GENERATED
---

Its contents can get overwritten with the processed contents of src/uxn.c.
See etc/mkuxn-fast.moon for instructions.

*/
]])
  wanted = true
  while true do
    local _continue_0 = false
    repeat
      local l = f:read('*l')
      if l:match(' push') or l:match('[ *]pop') then
        _continue_0 = true
        break
      end
      if l == '/* Stack */' then
        wanted = false
      end
      if l:match('errors%[%]') then
        _with_0:write('\n#ifndef NO_STACK_CHECKS\n')
        wanted = true
      end
      if wanted then
        _with_0:write(('%s\n'):format(l))
      end
      if l == '}' then
        _with_0:write('#endif\n\n')
        break
      end
      _continue_0 = true
    until true
    if not _continue_0 then
      break
    end
  end
  _with_0:write([[/* clang-format on */

#pragma mark - Core

int
evaluxn(Uxn *u, Uint16 vec)
{
	Uint8 instr;
	u->ram.ptr = vec;
	while(u->ram.ptr) {
		instr = u->ram.dat[u->ram.ptr++];
		switch(instr) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-variable"
]])
  for i = 1, 256 do
    local _continue_0 = false
    repeat
      if not allops[i].body then
        _continue_0 = true
        break
      end
      local _list_0 = allops[i].n
      for _index_0 = 1, #_list_0 do
        local n = _list_0[_index_0]
        _with_0:write(('\t\tcase 0x%02x: /* %s */\n'):format(n, allops[n + 1].name))
      end
      _with_0:write(('\t\t\t__asm__("evaluxn_%02x_%s:");\n'):format(allops[i].n[1], allops[i].name))
      _with_0:write(allops[i].body)
      _continue_0 = true
    until true
    if not _continue_0 then
      break
    end
  end
  _with_0:write([[#pragma GCC diagnostic pop
		}
	}
	return 1;
#ifndef NO_STACK_CHECKS
error:
	if(u->wst.error)
		return haltuxn(u, u->wst.error, "Working-stack", instr);
	else
		return haltuxn(u, u->rst.error, "Return-stack", instr);
#endif
}

int
]])
  wanted = false
  while true do
    local l = f:read('*l')
    if not l then
      break
    end
    if l:match('^bootuxn') then
      wanted = true
    end
    if wanted then
      _with_0:write(('%s\n'):format(l))
    end
  end
  f:close()
  _with_0:close()
  return _with_0
end
