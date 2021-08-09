--
-- Uxn core unroller script
--
-- This script updates src/uxn-fast.c when Uxn's opcode set changes, so that
-- updates in the human-readable src/uxn.c core can be easily converted into
-- high-performance code.
--
-- To run, you need Lua or LuaJIT, and just run etc/mkuxn-fast.lua from the top
-- directory of Uxn's git repository:
--
--     lua etc/mkuxn-fast.lua
--
-- This file is written in MoonScript, which is a language that compiles to
-- Lua, the same way as e.g. CoffeeScript compiles to JavaScript. Since
-- installing MoonScript has more dependencies than Lua, the compiled
-- etc/mkuxn-fast.lua is kept in Uxn's repository and will be kept updated as
-- this file changes.
--

replacements =
	op_and16: '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, d & b); push8(u->src, c & a); }'
	op_ora16: '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, d | b); push8(u->src, c | a); }'
	op_eor16: '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, d ^ b); push8(u->src, c ^ a); }'
	op_lit16: '{ push8(u->src, mempeek8(u->ram.dat, u->ram.ptr++)); push8(u->src, mempeek8(u->ram.dat, u->ram.ptr++)); }'
	op_swp16: '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, b); push8(u->src, a); push8(u->src, d); push8(u->src, c); }'
	op_ovr16: '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src); push8(u->src, d); push8(u->src, c); push8(u->src, b); push8(u->src, a); push8(u->src, d); push8(u->src, c); }'
	op_dup16: '{ Uint8 a = pop8(u->src), b = pop8(u->src); push8(u->src, b); push8(u->src, a); push8(u->src, b); push8(u->src, a); }'
	op_rot16: '{ Uint8 a = pop8(u->src), b = pop8(u->src), c = pop8(u->src), d = pop8(u->src), e = pop8(u->src), f = pop8(u->src); push8(u->src, d); push8(u->src, c); push8(u->src, b); push8(u->src, a); push8(u->src, f); push8(u->src, e); }'
	op_sth16: '{ Uint8 a = pop8(u->src), b = pop8(u->src); push8(u->dst, b); push8(u->dst, a); }'

local top, bottom, pushtop

offset = (n, s = '') ->
	if n < 0
		' -%s %d'\format s, -n
	elseif n > 0
		' +%s %d'\format s, n
	elseif s != ''
		' +%s 0'\format s
	else
		''

pop_push = (k, n, s) ->
	switch k
		when 'pop'
			s = s\match '^%((%S+)%)$'
			assert s == 'src'
			switch n
				when '8'
					top[s] -= 1
					if bottom[s] > top[s]
						bottom[s] = top[s]
					'%s.dat[%s.ptr%s]'\format s, s, offset(top[s])
				when '16'
					top[s] -= 2
					if bottom[s] > top[s]
						bottom[s] = top[s]
					'(%s.dat[%s.ptr%s] | (%s.dat[%s.ptr%s] << 8))'\format s, s, offset(top[s] + 1), s, s, offset(top[s])
		when 'push'
			s, v = s\match '^%((%S+), (.*)%)$'
			assert s == 'src' or s == 'dst', s
			switch n
				when '8'
					pushtop[s] += 1
					'%s.dat[%s.ptr%s] = %s'\format s, s, offset(pushtop[s] - 1), v
				when '16'
					if v\match'%+%+' or v\match'%-%-'
						error 'push16 has side effects: ' .. v
					peek, args = v\match '^([md]e[mv]peek)16(%b())$'
					if peek
						args = args\sub 2, -2
						return pop_push('push', '8', '(%s, %s8(%s))'\format s, peek, args) .. ';\n' .. pop_push('push', '8', '(%s, %s8(%s + 1))'\format s, peek, args)
					pushtop[s] += 2
					if v\match ' '
						v = '(' .. v .. ')'
					'%s.dat[%s.ptr%s] = %s >> 8;\n%s.dat[%s.ptr%s] = %s & 0xff'\format s, s, offset(pushtop[s] - 2), v, s, s, offset(pushtop[s] - 1), v
		else
			nil

indented_block = (s) ->
	s = s\gsub('^%{ *', '{\n')\gsub('\n', '\n\t')\gsub('\t%} *$', '}\n')
	s = s\gsub('\n[^\n]+%.error = [^\n]+', '%0\n#ifndef NO_STACK_CHECKS\n\tgoto error;\n#endif')
	s

process = (body) ->
	out_body = body\gsub('^%{ *', '')\gsub(' *%}$', '')\gsub('; ', ';\n')\gsub('%b{} *', indented_block)\gsub '(%a+)(%d+)(%b())', pop_push
	in_ifdef = false
	for k in *{'src', 'dst'}
		if bottom[k] != 0
			if not in_ifdef
				out_body ..= '\n#ifndef NO_STACK_CHECKS'
				in_ifdef = true
			out_body ..= '\nif(__builtin_expect(%s.ptr < %d, 0)) {\n\t%s.error = 1;\n\tgoto error;\n}'\format k, -bottom[k], k
		if pushtop[k] != 0
			if pushtop[k] > 0
				if not in_ifdef
					out_body ..= '\n#ifndef NO_STACK_CHECKS'
					in_ifdef = true
				out_body ..= '\nif(__builtin_expect(%s.ptr > %d, 0)) {\n\t%s.error = 2;\n\tgoto error;\n}'\format k, 255 - pushtop[k], k
			if in_ifdef
				out_body ..= '\n#endif'
				in_ifdef = false
			out_body ..= '\n%s.ptr %s= %d;'\format k, pushtop[k] < 0 and '-' or '+', math.abs pushtop[k]
	if in_ifdef
		out_body ..= '\n#endif'
		in_ifdef = false
	t = {}
	out_body\gsub '[^%w_]([a-f]) = (src%.dat%[[^]]+%])[,;]', (v, k) -> t[k] = v
	out_body = out_body\gsub '(src%.dat%[[^]]+%]) = ([a-f]);\n', (k, v) ->
		if t[k] and t[k] == v
			return ''
		return nil
	out_body

ops = {}

for l in assert io.lines 'src/uxn.c'
	name, body = l\match 'void (op_%S*)%(Uxn %*u%) (%b{})'
	if not name
		continue
	if replacements[name]
		body = replacements[name]
	body = body\gsub 'u%-%>src%-%>', 'src.'
	body = body\gsub 'u%-%>dst%-%>', 'dst.'
	body = body\gsub 'u%-%>src', 'src'
	body = body\gsub 'u%-%>dst', 'dst'
	top = { src: 0, dst: 0 }
	bottom = { src: 0, dst: 0 }
	pushtop = top
	ops[name] = process body
	top = { src: 0, dst: 0 }
	bottom = { src: 0, dst: 0 }
	pushtop = { src: 0, dst: 0 }
	ops['keep_' .. name] = process body

dump = (s, src, dst) ->
	ret = '\t\t\t{\n'
	for l in s\gmatch '[^\n]+'
		if not l\match '^%#'
			ret ..= '\t\t\t\t'
		ret ..= '%s\n'\format l
	ret ..= '\t\t\t}\n\t\t\tbreak;\n'
	(ret\gsub('src', src)\gsub('dst', dst))

i = 0
allops = {}
wanted = false
for l in assert io.lines 'src/uxn.c'
	if l == 'static void (*ops[])(Uxn *u) = {'
		wanted = true
	elseif l == '};'
		wanted = false
	elseif wanted
		l = l\gsub '%/%b**%/', ''
		for op in l\gmatch '[%w_]+'
			if not ops[op]
				error 'missing ' .. op
			allops[i + 0x00 + 1] = { n: { i + 0x00 }, body: dump ops[op], 'u->wst', 'u->rst' }
			allops[i + 0x40 + 1] = { n: { i + 0x40 }, body: dump ops[op], 'u->rst', 'u->wst' }
			allops[i + 0x80 + 1] = { n: { i + 0x80 }, body: dump ops['keep_' .. op], 'u->wst', 'u->rst' }
			allops[i + 0xc0 + 1] = { n: { i + 0xc0 }, body: dump ops['keep_' .. op], 'u->rst', 'u->wst' }
			i += 1

i = 0
wanted = false
for l in assert io.lines 'src/uxnasm.c'
	if l == 'static char ops[][4] = {'
		wanted = true
	elseif l == '};'
		wanted = false
	elseif wanted
		for op in l\gmatch '"(...)"'
			i += 1
			allops[i + 0x00].name = op
			allops[i + 0x20].name = op .. '2'
			allops[i + 0x40].name = op .. 'r'
			allops[i + 0x60].name = op .. '2r'
			allops[i + 0x80].name = op .. 'k'
			allops[i + 0xa0].name = op .. '2k'
			allops[i + 0xc0].name = op .. 'kr'
			allops[i + 0xe0].name = op .. '2kr'

for i = 1, 256
	if not allops[i]
		continue
	for j = i + 1, 256
		if allops[i].body == allops[j].body
			table.insert allops[i].n, (table.remove allops[j].n)
			allops[j].body = nil

with assert io.open 'src/uxn-fast.c', 'w'
	f = assert io.open 'src/uxn.c'
	while true
		l = f\read '*l'
		\write '%s\n'\format l
		if l == '*/'
			break
	\write '\n'
	\write [[
/*
 ^
/!\ THIS FILE IS AUTOMATICALLY GENERATED
---

Its contents can get overwritten with the processed contents of src/uxn.c.
See etc/mkuxn-fast.moon for instructions.

*/
]]
	wanted = true
	while true
		l = f\read '*l'
		if l\match' push' or l\match'[ *]pop' or l\match'devpeek16'
			continue
		if l == '/* Stack */'
			wanted = false
		if wanted
			\write '%s\n'\format l
		if l == '}'
			\write '\n'
			break
	\write [[
/* clang-format on */

#pragma mark - Core

int
uxn_eval(Uxn *u, Uint16 vec)
{
	Uint8 instr;
	if(u->dev[0].dat[0xf]) 
		return 0;
	u->ram.ptr = vec;
	if(u->wst.ptr > 0xf8) u->wst.ptr = 0xf8;
	while(u->ram.ptr) {
		instr = u->ram.dat[u->ram.ptr++];
		switch(instr) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-variable"
]]
	for i = 1, 256
		if not allops[i].body
			continue
		for n in *allops[i].n
			\write '\t\tcase 0x%02x: /* %s */\n'\format n, allops[n + 1].name
		\write '\t\t\t__asm__("evaluxn_%02x_%s:");\n'\format allops[i].n[1], allops[i].name
		\write allops[i].body
	\write [[
#pragma GCC diagnostic pop
		}
	}
	return 1;
#ifndef NO_STACK_CHECKS
error:
	if(u->wst.error)
		return uxn_halt(u, u->wst.error, "Working-stack", instr);
	else
		return uxn_halt(u, u->rst.error, "Return-stack", instr);
#endif
}

int
]]
	wanted = false
	while true
		l = f\read '*l'
		if not l
			break
		if l\match '^uxn_boot'
			wanted = true
		if wanted
			\write '%s\n'\format l
	f\close!
	\close!

