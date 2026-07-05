# cpp_tk

[![CI](https://github.com/okano-tomoyuki/cpp_tk/actions/workflows/ci.yml/badge.svg)](https://github.com/okano-tomoyuki/cpp_tk/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

C++からTk GUIを簡潔に扱える軽量ラッパーライブラリです。
PythonのTkinterに着想を得て、初心者にも扱いやすく、かつC++らしい型安全性と拡張性を両立しています。

## 🎯 目的

- C++でGUIを作りたいが、QtやwxWidgetsは重すぎる
- Python Tkinterのような手軽さをC++でも実現したい
- Tcl/Tkの資産を活かしつつ、C++らしい設計で再利用性を高めたい

### Qt/wxWidgetsとの違い

Qt/wxWidgetsは高機能・高性能な反面、ビルド時間・依存関係・学習コストが大きく、
「ちょっとしたGUI付きツールを手早く書きたい」という用途にはオーバースペックになりがちです。
cpp_tkはその対極として、Tcl/Tkという枯れた基盤の上に薄いラッパーを被せるだけの設計で、
できることの幅は狭い代わりに、依存関係・ビルド・学習コストの軽さを優先しています。
本格的なプロダクト開発にはQt/wxWidgets、社内ツールやプロトタイプ・学習用途にはcpp_tk、
という住み分けを想定しています。

## ✨ 特徴

- Tk/TclをC++から直感的に操作可能
- Pythonicな命名と構造（`Button`/`Entry`/`Label`/`Frame`など、classic/ttk合わせてWidget派生38クラスをカバー）
- `std::function`ベースのイベントバインディング（`bind`/`command`/`trace`等）
- `StringVar`/`IntVar`/`BooleanVar`/`DoubleVar`による変数連携とトレース
- `font`/`colorchooser`/`filedialog`/`messagebox`などのユーティリティ名前空間も完備
- `ScrolledText`/`LabeledScale`/`Calendar`等、頻出パターンをまとめた合成ウィジェットも用意
- Windows 11風のモダンなttkテーマ(`use_sv_ttk_theme()`)を標準搭載
- `cpp_tk::Error`/`ErrorPolicy`による例外ベースのエラーハンドリング（未初期化アクセスや不正なoption名を検知可能）
- doctestベースの自動テスト（24件）とGitHub Actions CI（Windows/Linux、AddressSanitizer/UndefinedBehaviorSanitizerによる検証込み）
- CMake対応（Windows/Linuxで動作確認済み。詳細は[制限事項](#-制限事項)参照）

## 📦 クイックスタート

### 依存関係のインストール

Tcl/Tkの開発用パッケージが必要です。

**Windows (MSYS2 / mingw64)**

```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-tcl mingw-w64-x86_64-tk
```

**Linux (Ubuntu/Debian)**

```bash
sudo apt-get install build-essential cmake ninja-build tcl-dev tk-dev
```

macOSでの動作は未検証です（[制限事項](#-制限事項)参照）。

### ビルド

```bash
git clone https://github.com/okano-tomoyuki/cpp_tk.git
cd cpp_tk
cmake -S . -B build
cmake --build build
```

`example/`配下のサンプルと`test/`配下のテストは、上記コマンドで`build`と同時にまとめてビルドされます
（`example/`はルートの`CMakeLists.txt`から`add_subdirectory`される前提のため、`example/`単体では
ビルドできません）。

### サンプル実行

```bash
./sample1
```

Sun Valley ttk themeの見た目を確認したい場合は`example/theme/`のデモを実行してください
（Toggle themeボタンでdark/lightを切り替えられます）。

```bash
./sv_ttk_demo
```

`Calendar`ウィジェットのデモ:

```bash
./calendar_demo
```

## 🛠 使用例

```cpp
#include "cpp_tk.hpp"

int main()
{
    namespace tk         = cpp_tk;
    namespace ttk        = tk::ttk;
    namespace messagebox = tk::messagebox;

    tk::Tk root;
    tk::Button btn(root);
    btn.text("Click Me").command([]() {
        messagebox::showinfo("Hello", "Button clicked!");
    });
    btn.pack();
    root.mainloop();
}
```

`StringVar`とトレースを使い、入力内容をリアルタイムに反映する例:

```cpp
#include "cpp_tk.hpp"

int main()
{
    namespace tk = cpp_tk;

    tk::Tk root;
    tk::StringVar name;
    tk::Entry entry(root);
    entry.textvariable(name);
    entry.pack();

    tk::Label label(root, {{"text", ""}});
    label.pack();
    name.trace([&label](const std::string& value) {
        label.config("text", "Hello, " + value + "!");
    });

    root.mainloop();
}
```

`custom::use_sv_ttk_theme()`でWindows 11風のモダンなttkテーマを一発で適用する例:

```cpp
#include "cpp_tk.hpp"
#include "custom.hpp"

int main()
{
    namespace tk = cpp_tk;

    tk::Tk root;
    tk::custom::use_sv_ttk_theme();       // dark(既定)。use_sv_ttk_theme(false)でlight

    tk::ttk::Button btn(root, {{"text", "Click Me"}});
    btn.pack();

    root.mainloop();
}
```

`Calendar`で日付を選択する例（`tkcalendar`相当、`custom`名前空間）:

```cpp
#include "cpp_tk.hpp"
#include "custom.hpp"

int main()
{
    namespace tk = cpp_tk;

    tk::Tk root;
    tk::custom::Calendar cal(root); // 引数省略で本日の日付
    cal.pack();
    cal.command([](int year, int month, int day) {
        // 選択された日付が渡される
    });

    root.mainloop();
}
```

## ⚠️ 制限事項

薄いラッパーであることを優先した結果、以下の制約があります。いずれも意図的な設計判断です。

- **Widgetはコピー禁止の値型です**。`Button a(root); Button b = a;`のようなコピーはできません
  （同じ実体を共有する複製が必要な場合は`handle()`を使います）。また、`bind()`/`command()`等の
  コールバックで対象のWidgetを参照する場合は、`[this]`やWidgetへの参照を直接キャプチャせず、
  `auto h = widget.handle();`のように名前付きでコピーキャプチャし、コールバック内で
  `SomeType(h)`のように再構築してから使う必要があります。この規約を守らないと、Widget破棄後に
  コールバックが発火した際、解放済みメモリを参照するSegmentation Faultになり得ます（この種の
  不具合はC++の性質上コンパイル時にもランタイムにも検知できません。CIのAddressSanitizer/
  UndefinedBehaviorSanitizerビルドで機械的に検出することを推奨します）。
- **親を必須の明示引数として渡す構築方式です**。Python Tkinterの暗黙のデフォルトroot省略
  （`master=None`）に相当する機能はありません。
- **親と全く同じ具象型のWidgetを1引数で渡すとコンパイルエラーになります**。例えば
  `ttk::Frame child(container);`で`container`も`ttk::Frame`の場合、`Widget`のコピー禁止に伴い
  暗黙生成される`Frame(const Frame&)`(delete済み)が、C++の仕様上「型が完全一致する候補は
  delete済みでも常に最優先される」ため選ばれてしまい、delete済み関数の呼び出しとしてエラーに
  なります(他のコンストラクタのシグネチャをどう工夫してもこの優先順位は覆せません)。
  `as_parent()`を経由することで回避できます。
  ```cpp
  ttk::Frame child(tk::as_parent(container)); // container自身と同じ型でもコンパイルできる
  ```
- **1スレッドにつき1つのTclインタプリタのみ**をサポートします。同一スレッドで複数の独立した
  `Tk`ルートを同時併存させて使い分けることはできません。
- **C++11をターゲット**にしています。
- **macOSでの動作は未検証**です（実機が無いため。Windows/Linuxは動作確認済み）。

## 🧱 ディレクトリ構成

```
cpp_tk/
├── cpp_tk.hpp / cpp_tk.cpp   # コアライブラリ(Widget/Var/font/ttk等)
├── custom.hpp / custom.cpp   # コアAPIのみで組み上げた合成ウィジェット・ダイアログ
├── thirdparty/              # ベンダリングした第三者製アセット(下記謝辞参照)
├── cmake/                    # Tcl/Tk検出用のFindモジュール
├── CMakeLists.txt            # ビルド設定
├── example/                  # サンプルコード群(theme/にモダンテーマの見た目を確認するデモ)
├── test/                     # doctestベースの自動テスト・コンパイル失敗テスト
├── docs/                     # 設計判断の経緯・既知の制限の記録
└── README.md                 # このファイル
```

## 🧪 テスト

```bash
cd build
ctest --output-on-failure
```

AddressSanitizer/UndefinedBehaviorSanitizerを有効にしたビルドで検証する場合:

```bash
cmake -S . -B build-asan -DCPP_TK_ENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

（Linux推奨。Windows/MSYS2のmingw-w64 GCCはsanitizerランタイムを同梱していません。）

## ⚖️ ライセンス

MIT License

## 🙏 謝辞

- [Sun Valley ttk theme](https://github.com/rdbende/Sun-Valley-ttk-theme) by rdbende (MIT License) — `use_sv_ttk_theme()`が適用するテーマ本体を`thirdparty/sv_ttk/`にベンダリングして利用しています。ライセンス原文は[thirdparty/sv_ttk/LICENSE-sv_ttk.txt](thirdparty/sv_ttk/LICENSE-sv_ttk.txt)に同梱しています。
- [doctest](https://github.com/doctest/doctest) (MIT License) — テストフレームワークとして`test/doctest.h`にベンダリングして利用しています。

## 🤝 貢献

バグ報告・機能提案・プルリク歓迎です！

## 📚 関連リンク

- https://www.tcl.tk/
- https://docs.python.org/ja/3/library/tkinter.html
