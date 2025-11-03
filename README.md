# cpp_tk

C++からTk GUIを簡潔に扱える軽量ラッパーライブラリです。
PythonのTkinterに着想を得て、初心者にも扱いやすく、かつC++らしい型安全性と拡張性を両立しています。

## 🎯 目的

- C++でGUIを作りたいが、QtやwxWidgetsは重すぎる
- Python Tkinterのような手軽さをC++でも実現したい
- Tcl/Tkの資産を活かしつつ、C++らしい設計で再利用性を高めたい

## ✨ 特徴

- Tk/TclをC++から直感的に操作可能
- Pythonicな命名と構造（例：`Button`, `Entry`, `Label`, `Frame` など）
- `std::function`ベースのイベントバインディング
- `StringVar`による変数連携とトレース（`trace_var`）
- `filedialog`, `messagebox` などのユーティリティも完備
- CMake対応・クロスプラットフォーム（Windows / macOS / Linux）

## 📦 インストール

```bash
git clone https://github.com/okano-tomoyuki/cpp_tk.git
cd cpp_tk
cmake -S . -B build
cmake --build build
```

## 🧪 サンプル実行

``` bash
cd example
mkdir build && cd build
cmake ..
make
./sample1
```

## 🧱 ディレクトリ構成

```
cpp_tk/
├── cpp_tk.hpp         # ライブラリヘッダー
├── cpp_tk.cpp         # ライブラリ実装
├── CMakeLists.txt     # ビルド設定
├── README.md          # このファイル
└── example/           # サンプルコード群
    ├── sample1.cpp
    ├── sample2.cpp
    └── CMakeLists.txt
```

## 🛠 使用例

``` cpp
#include "cpp_tk.hpp"

int main() 
{
    namespace tk         = cpp_tk;
    namespace ttk        = tk::ttk;
    namespace messagebox = tk::messagebox;

    tk::Tk root;
    tk::Button btn(&root);
    btn.text("Click Me").command([]() {
        messagebox::showinfo("Hello", "Button clicked!");
    });
    btn.pack();
    root.mainloop();
}
```

## ⚖️ ライセンス

MIT License

## 🤝 貢献

バグ報告・機能提案・プルリク歓迎です！

## 📚 関連リンク

- https://www.tcl.tk/
- https://docs.python.org/ja/3/library/tkinter.html