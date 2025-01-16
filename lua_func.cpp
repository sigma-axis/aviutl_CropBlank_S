/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <algorithm>
#include <tuple>

#include "lua/lua.hpp"
#pragma comment(lib, "lua/lua51")

#include "CropBlank_S.hpp"

static inline int bounding_box(lua_State* L)
{
	/*
	* syntax:
	*	left2, top2, right2, bottom2 = bounding_box([left, top, right, bottom [, threshold]])
	* or returns nil if all pixels are below the threshold.
	*
	* left, top: inclusive lower bounds.
	* right, bottom: exclusive upper bounds.
	*/

	auto const* const efpip = *exedit.efpip;
	if (efpip == nullptr)
		// unexpected case.
		return luaL_error(L, "予期しない状態で CropBlank_S.bounding_box() が呼び出されました!");

	// collect arguments.
	int l1 = luaL_optint(L, 1, 0),
		t1 = luaL_optint(L, 2, 0),
		r1 = luaL_optint(L, 3, efpip->obj_w),
		b1 = luaL_optint(L, 4, efpip->obj_h),
		threshold = luaL_optint(L, 5, 0);

	// check / adjust arguments.
	l1 = std::clamp(l1, 0, efpip->obj_w);
	t1 = std::clamp(t1, 0, efpip->obj_h);
	r1 = std::clamp(r1, 0, efpip->obj_w);
	b1 = std::clamp(b1, 0, efpip->obj_h);
	threshold = std::clamp(threshold, 0, (1 << 12) - 1);
	if (l1 >= r1 || t1 >= b1)
		// the specified rectangle is empty. recognize the bounding box as empty.
		return lua_pushnil(L), 1;

	// perform search.
	auto [l2, t2, r2, b2] = find_bounds(
		&efpip->obj_edit[l1 + t1 * efpip->obj_line], efpip->obj_line, threshold,
		r1 - l1, b1 - t1, true, true, true, true);

	if (l2 < 0)
		// all pixels are below the threshold.
		return lua_pushnil(L), 1;

	// move / adjust the resulting bounds.
	l2 += l1; r2 += l1 + 1;
	t2 += t1; b2 += t1 + 1;

	// then return the four integers.
	lua_pushinteger(L, l2);
	lua_pushinteger(L, t2);
	lua_pushinteger(L, r2);
	lua_pushinteger(L, b2);
	return 4;
}


extern "C" __declspec(dllexport) int luaopen_CropBlank_S(lua_State* L)
{
	if (!exedit.is_initialized())
		// in case this dll is not loaded as an .eef.
		return luaL_error(L, "CropBlank_S.eef が拡張編集フィルタプラグインとして読み込まれていません!");

	//// remove this function from the table `package.preload`.
	//lua_getglobal(L, "package"); lua_getfield(L, -1, "preload"); // loads: `package.preload`.
	//lua_pushnil(L); lua_setfield(L, -2, luaL_checkstring(L, 1)); // `package.preload[modname] = nil`
	//lua_pop(L, 2); // unloads: `package.preload`, `package`.

	constexpr int num_field = 2; // "version" and "bounding_box".

	lua_createtable(L, 0, num_field);
	lua_pushstring(L, PLUGIN_VERSION); lua_setfield(L, -2, "version");

#define reg_func(f)	lua_pushcfunction(L, f); lua_setfield(L, -2, #f)

	reg_func(bounding_box);

#undef reg_func

	return 1;
}

/* load this module via the idiom below:

local modname = "CropBlank_S"
if not package.loaded[modname] then
	package.preload[modname] = package.loadlib(modname..".eef", "luaopen_"..modname)
end
local CropBlank_S = require(modname)

*/
