# Akture

依存を最小限に絞った X11 向けディスプレイマネージャ。仕様は
`SPECIFICATION.md`、画面の基準は案D（`DESIGN_IMG.png`）。

## 構成

| ディレクトリ | 内容 |
| --- | --- |
| `akture/` | マスター `akture(8)`。root で動き、X サーバー、PAM 認証、セッション、フック、電源操作を受け持つ |
| `akture-greeter/` | グリーター `akture-greeter(8)`。`_akture` で動き、描画とキー入力を受け持つ |
| `akture-conv/` | PNG/JPEG を `.akbg` に変換する `akture-conv(1)` |
| `common/` | 両者が共有する通信メッセージ、`.akbg` の検査、設定値の解析 |
| `etc/` | `akture.conf`、PAM 設定、セッション定義、rc.d スクリプト |
| `autumnbsd/` | AutumnBSD が同梱するフック（`50-aprefetch`） |
| `tests/` | atf-c の単体テストと libFuzzer のハーネス |
| `verify/` | Frama-C（WP、Eva）の設定と ACSL スタブ |
| `spec/` | マスター・グリーター・セッション間のやり取りの TLA+ 仕様 |

OS の差は `akture/md_netbsd.c` と `akture/md_freebsd.c` に閉じ込めている。

## ビルドとインストール

BSD make を使う。ライブラリの場所は pkg-config で引く。

```sh
make
make install
```

グリーター用のユーザーを作っておく（NetBSD の例）。

```sh
groupadd _akture
useradd -g _akture -d /var/db/akture -s /sbin/nologin -c "Akture greeter" _akture
```

既定の書体は `/usr/share/akture/fonts/` の `Outfit.ttf` と
`ZenKakuGothicNew-Light.ttf`。別の場所なら `akture.conf` の `font_ui` と
`font_clock` で指定する。

背景は解像度ごとに変換して置く。

```sh
akture-conv -s 1366x768 -d 35 -o /usr/share/akture/backgrounds/autumn-1366x768.akbg autumn.jpg
```

## 検証

```sh
cd tests && make && kyua test       # 単体テスト（atf-c）
cd tests/fuzz && make run           # ファジング（clang の libFuzzer）
cd verify && make                   # Frama-C（WP と Eva をひとつのセッションで）
java -jar tla2tools.jar -config spec/akture.cfg spec/akture.tla
```

`verify/` は次を示す。未確定・不成立の性質が一つでも残れば失敗する。

| 対象 | 道具 | 示すこと |
| --- | --- | --- |
| メッセージの符号化と解析（`akmsg.c`） | WP＋RTE | 何が送られても範囲外アクセスやあふれがない。失敗したら内容を消去する |
| 状態遷移（`state.c`） | WP | 認証に成功せずにセッション中にならない |
| 権限を落とす手順（`ak_privdrop`） | WP | setgroups → setgid → setuid の順で、失敗すれば中止する |
| 秘密の消去（`akmsg_clear`、`ak_explicit_zero`） | WP | 使い終わった領域がすべて 0 になる |
| .akbg のヘッダ（`akbg_check`） | WP＋Eva | 検査を通ったヘッダは大きさが矛盾しない。実行時エラーがない |
| 設定ファイル（`conf_load` から）と引数の分割 | Eva（＋WP で補助関数の契約） | 任意の入力で実行時エラーがない |

`spec/akture.tla` は、グリーターの異常終了や乗っ取り、マスターの異常終了を含めて、
認証なしにセッションが始まらないこと、認証が必ず終わること、グリーターが必ず
起動し直されることを TLC で検査する。

道具のバージョンは `verify/VERSIONS` に固定している。
