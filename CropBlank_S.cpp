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
#include <atomic>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commdlg.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "multi_thread.hpp"
#include "CropBlank_S.hpp"


////////////////////////////////
// 主要情報源の変数アドレス．
////////////////////////////////
void ExEdit092::init_pointers(AviUtl::FilterPlugin* efp)
{
	fp = efp;

	auto pick_addr = [exedit_base = reinterpret_cast<int32_t>(efp->dll_hinst), this]
		<class T>(T & target, ptrdiff_t offset) { target = reinterpret_cast<T>(exedit_base + offset); };
	auto pick_val = [exedit_base = reinterpret_cast<int32_t>(efp->dll_hinst), this]
		<class T>(T & target, ptrdiff_t offset) { target = *reinterpret_cast<T*>(exedit_base + offset); };

	pick_addr(memory_ptr,			0x1a5328);
	pick_addr(efpip,				0x1b2b20);

	pick_val(yca_max_w,				0x196748);
	pick_val(yca_max_h,				0x1920e0);

	// make ready the multi-thread function feature.
	constexpr ptrdiff_t ofs_num_threads_address = 0x086384;
	auto aviutl_base = reinterpret_cast<uintptr_t>(fp->hinst_parent);
	multi_thread.init(fp->exfunc->exec_multi_thread_func,
		reinterpret_cast<int32_t*>(aviutl_base + ofs_num_threads_address));
}


////////////////////////////////
// 仕様書．
////////////////////////////////
FILTER_INFO("余白除去σ");

// trackbars.
constexpr const char* track_names[]
	= { "上余白", "下余白", "左余白", "右余白", "αしきい値" };
constexpr int32_t
	track_min[]			= { -4000, -4000, -4000, -4000,    0 },
	track_min_drag[]	= { -1000, -1000, -1000, -1000,    0 },
	track_default[]		= {     0,     0,     0,     0,    0 },
	track_max_drag[]	= {  1000,  1000,  1000,  1000, 4095 },
	track_max[]			= {  4000,  4000,  4000,  4000, 4095 };
namespace idx_track
{
	enum id : int {
		top,
		bottom,
		left,
		right,
		threshold,
	};
	constexpr int count_entries = std::size(track_names);
};
static_assert(
	std::size(track_names) == std::size(track_min) &&
	std::size(track_names) == std::size(track_min_drag) &&
	std::size(track_names) == std::size(track_default) &&
	std::size(track_names) == std::size(track_max_drag) &&
	std::size(track_names) == std::size(track_max));

// checks.
constexpr const char* check_names[]
	= { "中心の位置を変更", "上除去", "下除去", "左除去", "右除去" };
constexpr int32_t
	check_default[] = { 1, 1, 1, 1, 1 };
namespace idx_check
{
	enum id : int {
		move_center,
		top,
		bottom,
		left,
		right,
	};
	constexpr int count_entries = std::size(check_names);
};
static_assert(std::size(check_names) == std::size(check_default));


// callbacks.
static inline BOOL func_init(ExEdit::Filter* efp) { exedit.init(efp->exedit_fp); return TRUE; }
static int32_t func_window_init(HINSTANCE hinstance, HWND hwnd, int y, int base_id, int sw_param, ExEdit::Filter* efp);
static BOOL func_proc(ExEdit::Filter* efp, ExEdit::FilterProcInfo* efpip);

inline constinit ExEdit::Filter filter = {
	.flag = ExEdit::Filter::Flag::Effect,
	.name = const_cast<char*>(filter_name),
	.track_n = std::size(track_names),
	.track_name = const_cast<char**>(track_names),
	.track_default = const_cast<int*>(track_default),
	.track_s = const_cast<int*>(track_min),
	.track_e = const_cast<int*>(track_max),
	.check_n = std::size(check_names),
	.check_name = const_cast<char**>(check_names),
	.check_default = const_cast<int*>(check_default),
	.func_proc = &func_proc,
	.func_init = &func_init,
	.information = const_cast<char*>(info),
	.func_window_init = &func_window_init,
	.track_drag_min = const_cast<int*>(track_min_drag),
	.track_drag_max = const_cast<int*>(track_max_drag),
};


////////////////////////////////
// ウィンドウ状態の保守．
////////////////////////////////
constexpr int check_height = 16, check_diff_y = 21, d_check_pos_x = 360, d_check_diff_x = 80;
/*
efp->exfunc->get_hwnd(efp->processing, i, j):
	i = 0:		j 番目のスライダーの中央ボタン．
	i = 1:		j 番目のスライダーの左トラックバー．
	i = 2:		j 番目のスライダーの右トラックバー．
	i = 3:		j 番目のチェック枠のチェックボックス．
	i = 4:		j 番目のチェック枠のボタン．
	i = 5, 7:	j 番目のチェック枠の右にある static (テキスト).
	i = 6:		j 番目のチェック枠のコンボボックス．
	otherwise -> nullptr.
*/
static void update_extendedfilter_wnd(ExEdit::Filter* efp, HWND hwnd_setting_dlg)
{
	constexpr auto get_window_rect = [](HWND hwnd, HWND parent) {
		RECT rc;
		::GetWindowRect(hwnd, &rc);
		::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&rc), 2);
		return std::tuple{ rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top };
	};
	constexpr auto set_window_rect = [](HWND hwnd, int x, int y, int w, int h) {
		::MoveWindow(hwnd, x, y, w, h, TRUE);
	};

	HWND const chk_center = efp->exfunc->get_hwnd(efp->processing, 3, idx_check::move_center),
		chk_T = efp->exfunc->get_hwnd(efp->processing, 3, idx_check::top),
		chk_B = efp->exfunc->get_hwnd(efp->processing, 3, idx_check::bottom),
		chk_L = efp->exfunc->get_hwnd(efp->processing, 3, idx_check::left),
		chk_R = efp->exfunc->get_hwnd(efp->processing, 3, idx_check::right);

	auto const [x0, y0, w0, h0] = get_window_rect(chk_center, hwnd_setting_dlg);
	auto [x, y, w, h] = get_window_rect(chk_T, hwnd_setting_dlg);
	set_window_rect(chk_T, x0 + d_check_pos_x, y0, w, h);

	std::tie(x, y, w, h) = get_window_rect(chk_B, hwnd_setting_dlg);
	set_window_rect(chk_B, x0 + d_check_pos_x, y0 + 2 * check_height, w, h);

	std::tie(x, y, w, h) = get_window_rect(chk_L, hwnd_setting_dlg);
	set_window_rect(chk_L, x0 + d_check_pos_x - d_check_diff_x / 2, y0 + check_height, d_check_diff_x, h);

	std::tie(x, y, w, h) = get_window_rect(chk_R, hwnd_setting_dlg);
	set_window_rect(chk_R, x0 + d_check_pos_x + d_check_diff_x / 2, y0 + check_height, w, h);
}

int32_t func_window_init(HINSTANCE hinstance, HWND hwnd, int y, int base_id, int sw_param, ExEdit::Filter* efp)
{
	if (sw_param != 0) update_extendedfilter_wnd(efp, hwnd);

	// returns difference of the desired height from the given height.
	return 2 * check_height - 4 * check_diff_y;
}


////////////////////////////////
// フィルタ処理．
////////////////////////////////
static inline std::tuple<bool, bool, bool, bool, bool> quick_bound_check(ExEdit::PixelYCA const* data, size_t stride, int16_t threshold,
	int w, int h, bool search_T, bool search_B, bool search_L, bool search_R)
{
	// check at most 8 points to roughly know no search is required on certain edges.
	bool Lq = search_L, Tq = search_T, Rq = search_R, Bq = search_B, mid = false;

	if ((Lq || Tq) && data[0].a > threshold) Lq = Tq = false;
	if ((Rq || Bq) && data[w - 1 + (h - 1) * stride].a > threshold) Rq = Bq = false;
	if ((Rq || Tq) && data[w - 1].a > threshold) Rq = Tq = false;
	if ((Lq || Bq) && data[(h - 1) * stride].a > threshold) Lq = Bq = false;

	if (Tq && data[w >> 1].a > threshold) Tq = false, mid = true;
	if (Lq && data[(h >> 1) * stride].a > threshold) Lq = false;
	if (Rq && data[w - 1 + (h >> 1) * stride].a > threshold) Rq = false;
	if (Bq && data[(w >> 1) + (h - 1) * stride].a > threshold) Bq = false, mid = true;

	return { Tq, Bq, Lq, Rq, mid };
}
std::tuple<int, int, int, int> find_bounds(ExEdit::PixelYCA const* data, size_t stride, int16_t threshold,
	int w, int h, bool search_T, bool search_B, bool search_L, bool search_R)
{
	// firstly, drop the case where there's no need to search at all, like the entire image is opaque.
	auto const [quick_T, quick_B, quick_L, quick_R, mid] = quick_bound_check(data, stride, threshold, w, h,
		// cropping top and bottom may help for searching left and right edges.
		search_T || (search_L && search_R), search_B || (search_L && search_R),
		search_L, search_R);
	if (!(search_T && quick_T) && !(search_B && quick_B) && !quick_L && !quick_R) return { 0, 0, w - 1, h - 1 };

	// prepare the memory that is shared among threads.
	int* const mem = reinterpret_cast<int*>(*exedit.memory_ptr) + 1;

	// found bounds.
	int t = h, b = -1, l = mid ? (w >> 1) : w, r = mid ? (w >> 1) : - 1;
	constexpr auto empty = std::tuple{ -1, -1, -1, -1 };

	// helper class for atomic writes.
	struct atomic_min_max : std::atomic_int {
		// would better use fetch_min / fetch_max functions if C++26 were supported.
		int set_min(int min, int new_min) {
			do {
				if (compare_exchange_weak(min, new_min)) return new_min;
			} while (min > new_min);
			return min;
		}
		int set_max(int max, int new_max) {
			do {
				if (compare_exchange_weak(max, new_max)) return new_max;
			} while (max < new_max);
			return max;
		}
	};

	// search for top.
	if (!quick_T) t = 0;
	else if (search_T || (quick_L && quick_R)) {
		atomic_min_max min{ h };
		multi_thread(w, [&](int thread_id, int thread_num) {
			if (thread_id == 0) mem[-1] = thread_num;
			int limit = min.load(std::memory_order_relaxed);

			int const x0 = w * thread_id / thread_num, x1 = w * (thread_id + 1) / thread_num;
			auto* p = &data[x0]; auto const next_line = stride - (x1 - x0);
			for (int y = 0; y < limit; y++, p += next_line) {
				for (int x = x0; x < x1; x++, p++) {
					if (p->a > threshold) {
						min.set_min(limit, y);
						mem[thread_id] = x;
						return;
					}
				}

				limit = min.load();
			}
			mem[thread_id] = -1;
		});

		t = min;
		if (t >= h) return empty; // entire image is transparent.

		if (quick_L || quick_R) {
			// record x-coordinates as a hint for left / right search.
			auto* m = mem + mem[-1];
			while (--m >= mem) {
				if (int x = *m; x >= 0) {
					l = std::min(l, x);
					r = std::max(r, x);
					break;
				}
			}
			while (--m >= mem) {
				if (int x = *m; x >= 0) l = std::min(l, x);
			}
		}
	}

	// search for bototm.
	if (!quick_B) b = h - 1;
	else if (search_B || (quick_L && quick_R)) {
		atomic_min_max max{ t >= h ? -1 : t };
		multi_thread(w, [&](int thread_id, int thread_num) {
			if (thread_id == 0) mem[-1] = thread_num;
			int limit = max.load(std::memory_order_relaxed);

			int const x0 = w * thread_id / thread_num, x1 = w * (thread_id + 1) / thread_num;
			auto* p = &data[x1 - 1 + (h - 1) * stride]; auto const next_line = stride - (x1 - x0);
			for (int y = h; --y > limit; p -= next_line) {
				for (int x = x1; --x >= x0; p--) {
					if (p->a > threshold) {
						max.set_max(limit, y);
						mem[thread_id] = x;
						return;
					}
				}

				limit = max.load();
			}
			mem[thread_id] = -1;
		});

		b = max;
		if (b < 0) return empty; // entire image is transparent.

		if (quick_L || quick_R) {
			// record x-coordinates as a hint for left / right search.
			auto* m = mem + mem[-1];
			while (--m >= mem) {
				if (int x = *m; x >= 0) {
					l = std::min(l, x);
					r = std::max(r, x);
					break;
				}
			}
			while (--m >= mem) {
				if (int x = *m; x >= 0) l = std::min(l, x);
			}
		}
	}

	// crop top and bottom.
	if (t >= h) t = 0; if (b < 0) b = h - 1;
	int const H = b - t + 1;
	data += t * stride;

	// search for left.
	if (!quick_L) l = 0;
	else if (l > 0) {
		atomic_min_max min{ l };
		multi_thread(H, [&](int thread_id, int thread_num) {
			int limit = min.load(std::memory_order_relaxed);

			int const y0 = H * thread_id / thread_num, y1 = H * (thread_id + 1) / thread_num;
			auto const p1 = &data[y1 * stride];
			for (auto p0 = &data[y0 * stride]; p0 < p1; p0 += stride) {
				auto* p = p0;
				for (int x = 0; x < limit; x++, p++) {
					if (p->a > threshold) {
						limit = min.set_min(limit, x);
						goto next_loop;
					}
				}

				limit = min.load();
			next_loop:;
			}
		});

		l = min;
		if (l >= w) return empty; // entire image is transparent.
	}

	// search for right.
	if (!quick_R) r = w - 1;
	else if (r < w) {
		atomic_min_max max{ l >= w ? r : std::max(l, r) };
		multi_thread(H, [&](int thread_id, int thread_num) {
			int limit = max.load(std::memory_order_relaxed);

			int const y0 = H * thread_id / thread_num, y1 = H * (thread_id + 1) / thread_num;
			auto const p0 = &data[w - 1 + y0 * stride];
			for (auto p1 = &data[w - 1 + y1 * stride]; (p1 -= stride) >= p0; ) {
				auto* p = p1;
				for (int x = w; --x > limit; p--) {
					if (p->a > threshold) {
						limit = max.set_max(limit, x);
						goto next_loop;
					}
				}

				limit = max.load();
			next_loop:;
			}
		});

		r = max;
		if (r < 0) return empty; // entire image is transparent.
	}

	return { search_L ? l : 0, search_T ? t : 0, search_R ? r : w - 1, search_B ? b : h - 1 };
}

// crops into zero-sized image.
static inline void crop_into_blank(ExEdit::FilterProcInfo* efpip)
{
	efpip->obj_w = efpip->obj_h = 0;
}
// pad or crop only right / bottom sides. no copy is required.
static inline void crop_pad_RB(int R, int B, ExEdit::Exfunc* exfunc, ExEdit::FilterProcInfo* efpip)
{
	efpip->obj_w += R; efpip->obj_h += B;
	if (R > 0) exfunc->fill(efpip->obj_edit, efpip->obj_w - R, 0, R, efpip->obj_h, 0, 0, 0, 0, 2);
	if (B > 0) exfunc->fill(efpip->obj_edit, 0, efpip->obj_h - B, efpip->obj_w - std::max(0, -R), B, 0, 0, 0, 0, 2);
}
// general case for pading / cropping.
static inline void crop_pad(int L, int T, int R, int B, ExEdit::Exfunc* exfunc, ExEdit::FilterProcInfo* efpip)
{
	efpip->obj_w += L + R; efpip->obj_h += T + B;

	if (L <= 0 && R <= 0) {
		if (T > 0) exfunc->fill(efpip->obj_temp, 0, 0, efpip->obj_w, std::min(efpip->obj_h, T), 0, 0, 0, 0, 2);
		if (B > 0) exfunc->fill(efpip->obj_temp, 0, std::max(0, efpip->obj_h - B), efpip->obj_w, std::min(efpip->obj_h, B), 0, 0, 0, 0, 2);
	}
	else if (T <= 0 && B <= 0) {
		if (L > 0) exfunc->fill(efpip->obj_temp, 0, 0, std::min(efpip->obj_w, L), efpip->obj_h, 0, 0, 0, 0, 2);
		if (R > 0) exfunc->fill(efpip->obj_temp, std::max(0, efpip->obj_w - R), 0, std::min(efpip->obj_w, R), efpip->obj_h, 0, 0, 0, 0, 2);
	}
	else exfunc->fill(efpip->obj_temp, 0, 0, efpip->obj_w, efpip->obj_h, 0, 0, 0, 0, 2);

	exfunc->bufcpy(efpip->obj_temp, std::max(0, L), std::max(0, T),
		efpip->obj_edit, std::max(0, -L), std::max(0, -T),
		efpip->obj_w - std::max(0, L) - std::max(0, R),
		efpip->obj_h - std::max(0, T) - std::max(0, B),
		0, 0x13000003);
	std::swap(efpip->obj_edit, efpip->obj_temp);
}
BOOL func_proc(ExEdit::Filter* efp, ExEdit::FilterProcInfo* efpip)
{
	int const src_w = efpip->obj_w, src_h = efpip->obj_h;
	if (src_w <= 0 || src_h <= 0) return TRUE;

	constexpr int
		min_pad_T = track_min[idx_track::top],
		min_pad_B = track_min[idx_track::bottom],
		min_pad_L = track_min[idx_track::left],
		min_pad_R = track_min[idx_track::right],
		min_threshold = track_min[idx_track::threshold],
		max_pad_T = track_max[idx_track::top],
		max_pad_B = track_max[idx_track::bottom],
		max_pad_L = track_max[idx_track::left],
		max_pad_R = track_max[idx_track::right],
		max_threshold = track_max[idx_track::threshold];

	int const threshold = std::clamp(efp->track[idx_track::threshold], min_threshold, max_threshold),
		pad_T = std::clamp(efp->track[idx_track::top],		min_pad_T, max_pad_T),
		pad_B = std::clamp(efp->track[idx_track::bottom],	min_pad_B, max_pad_B),
		pad_L = std::clamp(efp->track[idx_track::left],		min_pad_L, max_pad_L),
		pad_R = std::clamp(efp->track[idx_track::right],	min_pad_R, max_pad_R);
	bool const move_center = efp->check[idx_check::move_center],
		search_T = efp->check[idx_check::top],
		search_B = efp->check[idx_check::bottom],
		search_L = efp->check[idx_check::left],
		search_R = efp->check[idx_check::right];

	if (src_w + pad_L + pad_R <= 0 || src_h + pad_T + pad_B <= 0)
		// cropping already exceeds the size of the original image.
		return crop_into_blank(efpip), TRUE;

	// find the bounding box.
	auto [L, T, R, B] = find_bounds(efpip->obj_edit, efpip->obj_line, threshold,
		src_w, src_h, search_T, search_B, search_L, search_R);
	if (L < 0) return crop_into_blank(efpip), TRUE; // all pixels are below the threshold.
	L *= -1; T *= -1;
	R += 1 - src_w;
	B += 1 - src_h;

	// append / remove margins.
	L += pad_L;
	T += pad_T;
	R += pad_R;
	B += pad_B;

	// check and cap to the maximum size.
	if (int const d = src_w + L + R - exedit.yca_max_w; d > 0) {
		L -= d >> 1;
		R -= (d + 1) >> 1;
	}
	if (int const d = src_h + T + B - exedit.yca_max_h; d > 0) {
		T -= d >> 1;
		B -= (d + 1) >> 1;
	}

	// perform buffer operation.
	if (src_w + L + R <= 0 || src_h + T + B <= 0)
		return crop_into_blank(efpip), TRUE;
	else if (L == 0 && T == 0)
		crop_pad_RB(R, B, efp->exfunc, efpip);
	else crop_pad(L, T, R, B, efp->exfunc, efpip);

	// adjust the center.
	if (!move_center) {
		efpip->obj_data.cx += (L - R) << 11;
		efpip->obj_data.cy += (T - B) << 11;
		// note that `1 << 12` is equivalent to 1 pixel.
	}

	return TRUE;
}


////////////////////////////////
// DLL 初期化．
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hinst);
		break;
	}
	return TRUE;
}


////////////////////////////////
// エントリポイント．
////////////////////////////////
EXTERN_C __declspec(dllexport) ExEdit::Filter* const* __stdcall GetFilterTableList() {
	constexpr static ExEdit::Filter* filter_list[] = {
		&filter,
		nullptr,
	};

	return filter_list;
}

