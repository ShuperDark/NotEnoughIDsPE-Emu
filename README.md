# NotEnoughIDsPE-Emu

脱獄端末用の Tweak「[NotEnoughIDsPE](https://github.com/ShuperDark/NotEnoughIDsPE)」を、**iOS MCPE (v0.16.2) Emulator** の Mod として動くように移植したものです。
ブロックIDとアイテムIDの上限を広げ、ほかの Mod から使える API（`NEID_` で始まる関数）を提供します。

| | バニラ | NotEnoughIDsPE |
|---|---|---|
| ブロックID | 0〜255 | **0〜65535** |
| アイテムID | 0〜511 | **0〜65535** |

機能・API・各段階の仕組みは元の Tweak と同じです。詳しくは移植元の README を参照してください。

> 移植元: **NotEnoughIDsPE** — https://github.com/ShuperDark/NotEnoughIDsPE

この Mod 単体ではゲームに何も追加されません。ブロックやアイテムを追加するのは、API を使う別の Mod です。

## ビルド（Visual Studio）

1. Visual Studio の「ローカル フォルダーを開く」でこのフォルダを開きます
2. 構成「MCPE mod (arm64, Release)」を選んで「すべてビルド」
3. できた `NotEnoughIDsPE.dylib` は自動で `%LOCALAPPDATA%\iOS-MCPE-0.16.2\mods\` にコピーされます。エミュレータを起動すると読み込まれます

必要なものはエミュレータの Mod と同じです（Python と `py -m pip install ziglang`）。
エミュレータを `--data <フォルダ>` で起動する場合は、CMake の `MCPE_DATA_DIR` をそのフォルダにしてください。

## ファイル

| ファイル | 内容 |
|---|---|
| `src/NotEnoughIDsPE.cpp` | **Tweak.xm から自動生成**した本体（手で編集しない） |
| `src/neid_port.h` / `src/neid_port.cpp` | 移植レイヤー（脱獄端末とエミュレータの違いを吸収する部分） |
| `tools/port_tweak.py` | `Tweak.xm` → `src/NotEnoughIDsPE.cpp` の変換スクリプト |
| `include/NotEnoughIDsPE.h` | ほかの Mod 用の API ヘッダー（`dlsym` で関数を取得する `neid::load()` 付き） |
| `include/emu_mod.h` など, `cmake/` | エミュレータの `mod-template` と同じもの |

### Tweak を更新したとき

Tweak.xm を直したら、変換し直してからビルドします。移植元のリポジトリを、このフォルダと同じ階層に
`NotEnoughIDsPE` として clone しておくと、引数なしで変換できます。

```bat
git clone https://github.com/ShuperDark/NotEnoughIDsPE ..\NotEnoughIDsPE
py tools\port_tweak.py
```

引数で別の場所の Tweak.xm も指定できます（`py tools\port_tweak.py <Tweak.xm のパス>`）。変換できない書き方（想定外の Objective-C など）があると、理由を出して止まります。

## 移植で変えたところ

Tweak のパッチ箇所・照合・API の処理は**すべて元のまま**で、次の iOS 固有の部分だけを `neid_port.h` に置き換えています。

| 脱獄端末 | エミュレータ |
|---|---|
| `%ctor`（Logos） | `__attribute__((constructor))`。エミュレータは Mod のコンストラクタをゲームの初期化より前に実行するので、タイミングは同じです |
| `MSHookFunction` | `emu_hook`（同じ 16 バイトの `LDR X16 / BR X16` の書き換え。エミュレータ自身や他の Mod のフックとも連鎖します） |
| `MSHookMemory` / `vm_protect` + `sys_icache_invalidate` | `memcpy`。ゲストのメモリはすべて書き込み可能で、書き換えはゲームのコードが JIT で変換される前に終わるため、キャッシュの無効化は不要です |
| 中継コード（cave）の領域を Tweak 自身の `__TEXT` に持つ | ゲームのイメージの直後（`0x101410000` 付近）に確保。エミュレータは Mod を `0x3xxxxxxxx` に読み込むので、ゲームから `B` / `BL`（±128 MB）が届きません |
| ゲームのコードから `BL` で Tweak の関数を直接呼ぶ | 届かない呼び先は、上の領域に置いた中継（veneer: `LDR X16, #8; BR X16; .quad 関数`）を経由します。`encB` / `encBL` / `inBRange` が自動で使うので、パッチの表は変えていません |
| `NSLog` / `NSString` のログ | `emulator.log`（`[mod] [NotEnoughIDsPE] ...`）と `<データ>\home\Documents\NotEnoughIDsPE.log`（起動のたびに作り直し） |

## 動作確認（2026-11-09、エミュレータの Debug ビルド）

起動時のログは、Tweak の README にある実機の正常なログと同じ数値になりました。

```
wrote 6095 instructions in 17 ranges (0 retried)
self-check: tables 65536, block id 16-bit, 256 blocks, 0 id mismatches, 0 old-table mismatches, chunk +hi array
stage 4a: 18 store sites patched, 16-bit getBlock on; ... stage 4c: 286 caller groups widened, 0 skipped, ...
stage 4e: 54/54 1-byte callers see the fallback block; stage 4f: pistons 16-bit; stage 3d: network 16-bit, server break 16-bit;
stage 7a: 161 groups (124 + 83 xyz calls, 21 fire-odds reads) widened, 0 skipped; setBlock(BlockID): 52 calls in 44 groups widened, 0 skipped;
caves 4386/8192 insns
```

テスト用 Mod（確認後に削除）で確認したこと:

- ブロック 513 / 40000 / 65509〜65535 の登録、アイテム 32768 / 40000 / 65534 / 65535 などの登録（`Item::mItems` の 16 ビットの ID）
- ワールド内での 16 ビットのブロックの設置と読み取り、バニラのブロックで上書きしたときに上位バイトが消えること（45 件すべて成功）
- クリエイティブインベントリへの表示、手持ち・ホットバー・ワールドでの描画（金・鉄・ダイヤのテクスチャ、光るブロック）
- 地形生成（複製の赤い丸太とピンクの葉の大きなオーク）
- セーブして、エミュレータを再起動してもブロックとインベントリの 16 ビットの ID が残ること（`3c: saved / loaded high bytes of chunk`）

マルチプレイ（通信）、ピストン、かまどの複製の点火の切り替えなどは、まだエミュレータ上で試していません。
