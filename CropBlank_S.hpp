/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <tuple>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

using byte = uint8_t;
#include <aviutl/FilterPlugin.hpp>
#include <exedit/FilterProcInfo.hpp>


////////////////////////////////
// 仕様書．
////////////////////////////////
#define PLUGIN_VERSION	"v1.00-beta5"
#define PLUGIN_AUTHOR	"sigma-axis"
#define FILTER_INFO_FMT(name, ver, author)	(name##" "##ver##" by "##author)
#define FILTER_INFO(name)	static constexpr char filter_name[] = name, info[] = FILTER_INFO_FMT(name, PLUGIN_VERSION, PLUGIN_AUTHOR)


////////////////////////////////
// 主要情報源の変数アドレス．
////////////////////////////////
inline constinit struct ExEdit092 {
	void init(AviUtl::FilterPlugin* efp) {
		if (!is_initialized()) init_pointers(efp);
	}
	constexpr bool is_initialized() const { return fp != nullptr; }
	AviUtl::FilterPlugin* fp = nullptr;

	void** memory_ptr;					// 0x1a5328 // 少なくとも最大画像サイズと同サイズは保証されるっぽい．
	ExEdit::FilterProcInfo**	efpip;	// 0x1b2b20

	int32_t	yca_max_w;					// 0x196748
	int32_t	yca_max_h;					// 0x1920e0

private:
	void init_pointers(AviUtl::FilterPlugin* efp);

} exedit{};


////////////////////////////////
// フィルタ処理の中核関数．
////////////////////////////////
std::tuple<int, int, int, int> find_bounds(ExEdit::PixelYCA const* data, size_t stride, int16_t threshold,
	int w, int h, bool T, bool B, bool L, bool R);
