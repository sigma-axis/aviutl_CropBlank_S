# 余白除去σ 拡張編集フィルタプラグイン

拡張編集オブジェクトの不透明ピクセルを検索して，最小サイズなるよう上下左右端をクリッピングするフィルタ効果です．

Mr-Ojii 様の [AutoClipping_M](https://github.com/Mr-Ojii/AviUtl-AutoClipping_M-Script) の高速化・機能追加版で，他のアニメーション効果やスクリプト制御からも利用できます．

![余白除去のデモ](https://github.com/user-attachments/assets/7200146a-8d17-4a8b-aab9-7be045871662)

[ダウンロードはこちら．](https://github.com/sigma-axis/aviutl_CropBlank_S/releases) [紹介動画．](https://www.nicovideo.jp/watch/sm44546499)

##  動作要件

- AviUtl 1.10 + 拡張編集 0.92

  http://spring-fragrance.mints.ne.jp/aviutl
  - 拡張編集 0.93rc1 等の他バージョンでは動作しません．

- Visual C++ 再頒布可能パッケージ（\[2015/2017/2019/2022\] の x86 対応版が必要）

  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist

- patch.aul の `r43 謎さうなフォーク版58` (`r43_ss_58`) 以降

  https://github.com/nazonoSAUNA/patch.aul/releases/latest

##  導入方法

`aviutl.exe` と同階層にある `plugins` フォルダ内に `CropBlank_S.eef` ファイルをコピーしてください．

##  使い方

正しく導入できているとフィルタ効果に「余白除去σ」が追加されています．オブジェクトの不透明ピクセルを検索して，最小サイズなるよう上下左右端をクリッピングします．

![GUIの表示](https://github.com/user-attachments/assets/0757c613-b515-4289-964f-34eeaebb750e)

### 各種パラメタ

- 上余白 / 下余白 / 左余白 / 右余白

  上下左右それぞれの余白幅をピクセル単位で指定します．負値を指定するとさらにクリッピングします．

  最小値は `-4000`, 最大値は `4000`, 初期値は `0`.

- αしきい値

  各ピクセルを「不透明」と判断する，アルファ値のしきい値です．このしきい値以下のピクセルは「透明」，しきい値を超えるピクセルは「不透明」とみなされます．

  最小値は `0`, 最大値は `4095`, 初期値は `0`.

  - 拡張編集でのアルファ値は完全透明ピクセルは `0`，完全不透明は `4096` です．

- 中心の位置を変更

  拡張編集標準の「クリッピング」の「中心の位置を変更」と同様です．チェックを入れると見た目の回転中心が，フィルタ効果適用前と比べて移動します．

  初期値は ON.

- 上除去 / 下除去 / 左除去 / 右除去

  上下左右それぞれで，透明ピクセルを除去するかどうかを個別に指定します．

  初期値は全て ON.

##  スクリプトからの利用

スクリプト制御やアニメーション効果などで次のように「余白除去σ」の効果を適用できます:

```lua
obj.effect("余白除去σ", "αしきい値", 2048, "中心の位置を変更", 0, "下除去", 0)
```

また，次のように `CropBlank_S.eef` を `require` することで [`bounding_box()` 関数](#bounding_boxleft-top-right-bottom-threshold-関数)が利用できます:

```lua
-- require の 手順．
local modname = "CropBlank_S"
if not package.loaded[modname] then
  package.preload[modname] = package.loadlib(modname..".eef", "luaopen_"..modname)
end
local CropBlank_S = require(modname)

-- bounding_box() 関数を呼び出し．
left2, top2, right2, bottom2 = CropBlank_S.bounding_box(left, top, right, bottom, threshold)
```

### `CropBlank_S` テーブル

`require` 関数の戻り値として得られます．*グローバル変数には登録されないので注意．*

次のフィールドを持ちます:

|名前|型|説明|
|:---:|:---:|:---|
|`bounding_box`|function|[リファレンス](#bounding_boxleft-top-right-bottom-threshold-関数)を参照|
|`version`|string|`"v1.00"` などの文字列|

### `bounding_box([left, top, right, bottom[, threshold]])` 関数

現在オブジェクトの指定矩形内にある，しきい値よりもアルファ値の大きいピクセル全てを囲む最小の矩形を特定する関数．

|位置|名前|型|説明|
|:---|:---:|:---:|:---|
|引数 #1|`left`|integer\|nil|ピクセル検索範囲の左端の X 座標．既定値は `0`.|
|引数 #2|`top`|integer\|nil|ピクセル検索範囲の上端の Y 座標．既定値は `0`.|
|引数 #3|`right`|integer\|nil|ピクセル検索範囲の右端の X 座標．既定値は画像のピクセル幅．|
|引数 #4|`bottom`|integer\|nil|ピクセル検索範囲の下端の Y 座標．既定値は画像のピクセル高さ．|
|引数 #5|`threshold`|integer\|nil|アルファ値のしきい値．これより大きい値のアルファ値が検索対象となる．既定値は `0`.|
|戻り値 #1|`left2`|integer\|nil|存在領域の左端の X 座標．全てのピクセルがしきい値以下のアルファ値の場合は `nil`.|
|戻り値 #2|`top2`|integer|存在領域の上端の Y 座標．|
|戻り値 #3|`right2`|integer|存在領域の右端の X 座標．|
|戻り値 #4|`bottom2`|integer|存在領域の下端の Y 座標．|

- アルファ値は `0` で完全透明，`4096` 完全不透明．
- `left`, `right` の最小値は `0`, 最大値はピクセル幅．
- `top`, `bottom` の最小値は `0`, 最大値はピクセル高さ．
- `threshold` の最小値は `0`, 最大値は `4095`.
- 戻り値の `left2` が `nil` だった場合，`top2`, `right2`, `bottom2` は戻り値として返されない (i.e. 戻り値の個数は `1` 個).
- 座標の範囲は，左/上は inclusive (i.e. 指定値ちょうども範囲内), 右/下は exclusive (i.e. 指定値ちょうどは範囲外), ピクセル単位で左上が原点．

使用例:
- $10 \leq x \lt 90,\quad 20 \leq y \lt 80$ の矩形範囲で，アルファ値が `2048` (50%) を超えるピクセル範囲の最小サイズにクリッピング．
  ```lua
  local left2, top2, right2, bottom2 = CropBlank_S.bounding_box(10, 20, 90, 80, 2048)
  if left2 then -- left2 が nil の場合は全てのピクセルのアルファ値が 50% 以下
    local w, h = obj.getpixel() -- オブジェクトのピクセルサイズ
    obj.effect("クリッピング", "左", left2, "上", top2, "右", w - right2, "下", h - bottom2)
  end
  ```

##  ビルド方法について

このプロジェクトをビルドする際には `lua` フォルダに以下のファイルを配置してください．

- `lua51.lib`
- `lua.h`
- `lualib.h`
- `luaconf.h`
- `lauxlib.h`
- `lua.hpp`

##  謝辞

- Mr-Ojii 様

  この拡張編集フィルタプラグインのアイデア元は Mr-Ojii 様のスクリプト [AutoClipping_M](https://github.com/Mr-Ojii/AviUtl-AutoClipping_M-Script) です．当初は簡単な pull request だけのつもりだったのですが，それがきっかけでピクセル検索の順序で大きく効率が変わることを知ることとなりました．

  また，[Inline Scene S](https://github.com/sigma-axis/aviutl_script_InlineScene_S) の開発過程でも AutoClipping_M と同じ機能が必要となり，その時は Lua スクリプトとして同等の機能を埋め込みましたが，ここから更に高速化できないかと試行錯誤した結果できたプラグインです．

  半ば惰性で作ったプラグインとも言えますが，礎となったスクリプトを開発してくださり，また機能拡張版の公開を快く承諾してくださった Mr-Ojii 様には改めて感謝申し上げます．


##  改版履歴

- **v1.00** (2025-01-16)

  - 初版．

##  ライセンス

このプログラムの利用・改変・再頒布等に関しては MIT ライセンスに従うものとします．

---

The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://mit-license.org

# Credits

##  AutoClipping_M

https://github.com/Mr-Ojii/AviUtl-AutoClipping_M-Script

---

Creative Commons Zero v1.0 Universal


##  aviutl_exedit_sdk

https://github.com/ePi5131/aviutl_exedit_sdk （利用したブランチは[こちら](https://github.com/sigma-axis/aviutl_exedit_sdk/tree/self-use)です．）

---

1条項BSD

Copyright (c) 2022
ePi All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY ePi “AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ePi BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


##  Lua 5.1.5

http://www.lua.org

---

MIT license

Copyright (C) 1994-2012 Lua.org, PUC-Rio.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.


# 連絡・バグ報告

- GitHub: https://github.com/sigma-axis
- Twitter: https://x.com/sigma_axis
- nicovideo: https://www.nicovideo.jp/user/51492481
- Misskey.io: https://misskey.io/@sigma_axis
- Bluesky: https://bsky.app/profile/sigma-axis.bsky.social
