# cpp_tk 課題整理（引き継ぎメモ）

作成日: 2026-07-05
目的: 本家Python Tkinterとの差分・cpp_tk固有の設計判断・構造的なユーザ体験上の課題を、別セッション／別担当者に引き継げる形で整理する。**現在「残っている」タスクだけを見たい場合は [todo.md](todo.md) を参照。このファイルは経緯・却下した設計案・発見したバグの詳細な記録であり、完了済みの項目も含めて全て残す（過去の議論を蒸し返さないための参照用）。**

参考: 直近のコミットはダングリングポインタ／セグメンテーション違反対策が中心（`db3a275`, `b7a62ab`, `a5a7586`, `81975a4`）。本ドキュメントのC節はこの流れの延長線上にある未解決課題を含む。

---

## A. 本家Tkinterにあるがcpp_tkに未実装の機能

**2026-07-05 対応済み。** Widget共通機能・classic LabelFrame/OptionMenu・Menu/Listbox/Text/filedialog/messageboxの不足メソッドを追加し、一時サンプル(`example/_smoke_test_a.cpp`、ビルド後削除)で新API一式の呼び出しを確認済み。ビルドも通ることを確認した。

### Widget共通機能（本家のMisc相当）
- [x] `unbind` が無い（[bind](../cpp_tk.hpp#L355) はあるが解除手段がない）→ [Widget::unbind](../cpp_tk.hpp#L358)を追加
- [x] `focus_set` / `focus_get` / `focus_force` → 追加
- [x] `winfo_screenwidth` / `winfo_screenheight` / `winfo_pointerx` / `winfo_pointery` / `winfo_manager` / `winfo_ismapped` → 追加
- [x] `clipboard_get` / `clipboard_append` / `clipboard_clear` → 追加
- [x] `wait_window` / `wait_variable` → 追加（`tkwait window/variable`を薄くラップ）
- [x] `lift` / `lower` が [Tk](../cpp_tk.hpp#L481) と [Toplevel](../cpp_tk.hpp#L555) にしかなく、一般Widgetから呼べない → `Widget::lift`/`Widget::lower`を基底に追加（Tk/Toplevel側の既存メソッドはそのまま残置、名前解決で優先される）

### ウィジェット自体が無い
- [x] classic `tk::OptionMenu` → 追加。`tk_optionMenu`プロシージャではなく本家同様Menubutton+Menuを内部合成する方式で実装し、`command()`で選択時コールバックを設定可能
- [x] classic `tk::LabelFrame` → 追加（[ttk::Labelframe](../cpp_tk.hpp#L1255)とは別に、classic版`labelframe`をラップ）

### 既存ウィジェットのメソッド不足
- [x] [Menu](../cpp_tk.hpp#L784-L806): `add_checkbutton` / `add_radiobutton` / `insert` / `entryconfigure` / `index` を追加
  - 注意: Tkのmenuは既定で`tearoff=1`のためindex 0は自動生成されるtearoff項目が占める。数値indexでの操作は本家同様この扱いに従う（ライブラリの不具合ではない）。動作確認時、この仕様を失念して数値indexでentryconfigureし「unknown option -label」エラーを踏んだため、引き継ぎ先も同じ落とし穴に注意。
- [x] [Listbox](../cpp_tk.hpp#L740-L782): indexが`int`固定で`END`等のシンボリック定数が使えない → `insert`/`erase`/`get`/`see`/`select_set`/`select_clear`/`select_includes`/`activate`に`const std::string&`版オーバーロードを追加し、既存の`int`版と共存
- [x] [Text](../cpp_tk.hpp#L930-L968): `dump` / `compare` / `count` / `image_create` / `window_create` を追加（`image_create`は`-image`オプション必須、`window_create`は既存Widgetを埋め込む）
- [x] [filedialog::askopenfile](../cpp_tk.hpp#L1467): 複数選択の`askopenfilenames`を追加（`-multiple 1`＋`Tcl_SplitList`で正しく要素分解、既存の空白splitの流儀は使っていない）。名前は本家準拠だが返り値の意味（本家はファイルオブジェクト、cpp_tkはパス文字列）が異なる点をヘッダコメントに明記
- [x] [messagebox](../cpp_tk.hpp#L1475-L1492): `askyesnocancel` を追加（cancel/noは両方falseにまとめる簡略化。他のask*系と同じ割り切り）

### 第2回棚卸し（2026-07-05対応済み、テスト基盤導入後にsubagentで実施）
初回はWidget共通機能・classic OptionMenu/LabelFrame・Menu/Listbox/Text/filedialog/messageboxのみが対象で、Canvas/ttkの細部やToplevel周辺は未着手だった。実利用頻度が高い順に対応し、[test/test_a_features_round2.cpp](../test/test_a_features_round2.cpp)で全項目を回帰テスト化した(11 TEST_CASE、59 assertion、全green)。

- [x] Widget共通: `bind_all`/`bind_class`/`unbind_all`(グローバルキーバインド)、`grid_slaves`/`pack_slaves`/`place_slaves`+`grid_info`/`pack_info`/`place_info`(配置情報照会)、`nametowidget`(フルネーム文字列からの逆引き。ただし具体的な派生型は復元できずWidgetとして返る簡略版)、`winfo_reqwidth`/`winfo_reqheight`
- [x] Canvas: `tag_raise`/`tag_lower`(重なり順)、`find_all`/`find_withtag`、`canvasx`/`canvasy`(スクロール座標変換)、`create_bitmap`
- [x] Text: `index()`(index式の解決)、`tag_names`/`tag_ranges`/`tag_lower`/`tag_raise`/`tag_delete`、`edit_undo`/`edit_redo`/`edit_modified`
- [x] Entry(classic/ttk): `select_range`/`selection_clear`/`select_present`
- [x] Scale(classic/ttk): `get()`/`set()`の値アクセサを追加。Spinbox(classic/ttk)は`get()`のみ追加(`set()`はEntryと同じ理由でTclに該当サブコマンドが無いため意図的に非対応、ヘッダコメントに明記)
- [x] ttk::Notebook: `tab()`(タブ設定の読み書き)、`tabs()`、`forget()`/`hide()`、`index()`、`select()`の引数無し(getter)版
- [x] ttk::Treeview: `tag_has()`
- [x] Toplevel/Tk: `overrideredirect()`(getter/setter)

優先度を下げて今回は対象外: `PhotoImage`の`put`/`get`/`zoom`/`subsample`等の画像操作系、`simpledialog`(askstring等)、`ttk::Style`の`layout`/`element_create`等の高度なテーマ機能。

**副産物として発見・修正したバグ**（テスト作成中に判明、本セッションのC節と同種の実害):
- **`Menu::index()`**: パターンに一致しない場合、Tclは`"none"`ではなくエラーを送出する仕様だったため、ErrorPolicy導入後は「見つからなければ-1」のはずが例外を送出していた。`success`を明示的に見るよう修正。
- **`Treeview::selection_set`/`add`/`remove`/`toggle`**: 各iidを別々の位置引数として渡していたが、Tclの`ttk::treeview selection`は「itemsを1個のTclリスト引数」として受け取る仕様。スペースを含むiidを渡すと空白区切りで誤って再分割され、存在しないアイテムとして扱われていた。`ArgValue(vector<string>)`のSTRING_LIST機構(`Tcl_NewListObj`経由)を使うよう修正。

### 第3回棚卸し（2026-07-05対応済み、クラス単位の欠落に絞って実施）
第1・2回はいずれも「既存クラスのメソッド不足」が対象で、「クラスそのものが丸ごと無い」という観点では棚卸ししていなかった。ユーザからの`tk.Image`/`tk.BitmapImage`の指摘をきっかけに、本家tkinter/ttkのクラス一覧とcpp_tkの宣言済みクラス一覧を突き合わせて確認した。[test/test_new_classes.cpp](../test/test_new_classes.cpp)で全項目を回帰テスト化(5 TEST_CASE、14 assertion、全green)。

- [x] **`BitmapImage`**: `PhotoImage`と同じ`image create <type>`系のTclコマンドパターンで独立実装。本家は`PhotoImage`と共に`Image`という共通基底を持つが、cpp_tkの`PhotoImage`は既に`Object`/`InterpreterClient`を直接継承する構造のため、共通基底の導入は見送り同じ構造で独立実装した。
- [x] **`ttk::Menubutton`**: ttk版のMenubutton。`menu()`は引き続きclassicの`Menu`を紐づける(ttk専用のMenuクラスは本家にも無い)。
- [x] **`ttk::PanedWindow`**: ttk版のPanedWindow。`sashpos`/`panes`も追加。
- [x] **`ScrolledText`**: Text+Scrollbar(縦)を合成しスクロール連携済みの便利クラス(Python `tkinter.scrolledtext.ScrolledText`相当)。Text固有の操作は全て委譲せず`text()`アクセサ経由にした(C++の静的型で本家同様に全メソッドを継承的に持たせるのは保守コストが高いため)。内部のyscrollcommand/command連携は`[this]`直接キャプチャではなく`handle()`経由の安全な再構築パターンを使用(docs/tasks.md B節のWidget設計方針を踏襲)。
- [x] **`ttk::LabeledScale`**: Label(値表示)+Scaleを合成したウィジェット(Python `tkinter.ttk.LabeledScale`相当。本家もFrame+Label+Scaleの合成でネイティブTclウィジェットではない)。`DoubleVar::trace`で値変化に追従してlabel表示を更新する部分も同様に`handle()`経由。
- 上記5クラスの実装に伴い、`tk::Scrollbar`(classic)・`ttk::Label`・`ttk::Scale`に`using Widget::Widget;`(`handle()`からの再構築用の継承コンストラクタ)が無かったため追加した。

優先度を下げて今回も対象外: `simpledialog`(askstring等)、`ttk::Style`の`layout`/`element_create`等の高度なテーマ機能、`Image`共通基底の導入。

**副産物として発見した既存の構造的制約（2026-07-05対応済み）**:
- **生成時専用(read-only)オプションを設定する手段が無い**: `ttk::panedwindow`の`-orient`のように「生成時にしか指定できないオプション」を持つウィジェットが存在するが、cpp_tkの`Widget(parent, type, name)`は常に「(1)オプション無しで生成 → (2)`configure`で別途設定」という2段階の実装になっているため、こうしたオプションを設定する手段が現状無い(`ttk::PanedWindow::orient()`を生成後に呼ぶと`attempt to change read-only option`でErrorになる。動作確認用テストで実際に踏んだため、既定値のままテストするよう回避した)。

  **対応方針の検討（2026-07-05）**: 対応前に、この問題が`ttk::PanedWindow`固有か他ウィジェットにも及ぶかを実機で調査した。`ttk::Progressbar`/`ttk::Scale`/`ttk::Separator`/classic`Scale`/`Scrollbar`/`ttk::Scrollbar`の`-orient`は全て生成後の再設定が可能で、read-onlyだったのは`ttk::PanedWindow`のみだった。一方、本家Tkのドキュメント上生成時専用と明記されている`Frame`/`Toplevel`の`-class`/`-container`/`-visual`/`-colormap`/`-screen`/`-use`は、cpp_tkでは**コンストラクタのoptionsマップに含めても構築自体が失敗する**ことを実機確認した(実利用頻度は低いが実在する)。「`ttk::PanedWindow`だけの局所対応」と「`Widget`基底コンストラクタの構造的修正」の2案を提示し、ユーザーは後者(構造的修正)を選択した。

  **対応内容**: `Widget(parent, type, name, options={})`という4引数版を追加し、`options`を生成コマンド自体に埋め込む(`call({type, full_name, "-opt1", val1, ...})`)方式に変更した。生成後に再設定可能な通常のオプションは、生成時に埋め込んでも最終的な状態は変わらないため、既存の全ウィジェットの動作に影響は無い。`config(options)`を呼んでいた約35個の具象ウィジェットクラス(classic/ttk合わせて)のコンストラクタを機械的に「基底コンストラクタへoptionsを渡す」形へ移行した(`Toplevel`のように追加ロジックを持つものは個別対応)。
  - **副産物として発見したバグ**: `ttk::Separator`のコンストラクタが`options`を受け取りながら一度も使っていなかった(`config(options)`の呼び出し自体が無かった)。修正のついでに解消した。
  - [test/test_structural_fixes.cpp](../test/test_structural_fixes.cpp)で`ttk::PanedWindow`の`-orient`、`Frame`の`-class`/`-container`、`ttk::Separator`の`-orient`が構築時に正しく適用されることを回帰テスト化。[test/test_new_classes.cpp](../test/test_new_classes.cpp)の`ttk::PanedWindow`テストも、既定値のままにする回避策から`{{"orient", "horizontal"}}`を実際に指定する形に更新した。

**追記（2026-07-05）: `ScrolledText`/`ttk::LabeledScale`は`cpp_tk::custom`名前空間へ移設**
ユーザから「複合Widget（単一のTclウィジェットとして代替不可能ではなく、単に便利なだけの合成）はコアの純粋性を損なうのでは」という指摘があり、議論の結果、以下の方針で分離した（本家tkinterも`OptionMenu`はcoreモジュール、`ScrolledText`は別モジュール、`ttk.LabeledScale`はttkモジュール内、と一貫していないことを踏まえた判断）。
- **コア(`cpp_tk.hpp`/`cpp_tk.cpp`、`cpp_tk`名前空間)には残すもの**: 単一のTclウィジェットとして代替不可能な合成（`OptionMenu`はTclの`tk_optionMenu`相当を独自合成する以外に手段が無いため残置）
- **新設した`custom.hpp`/`custom.cpp`（`cpp_tk::custom`名前空間）に切り出したもの**: `ScrolledText`、`ttk::LabeledScale`（→`cpp_tk::custom::LabeledScale`、ttk専用ではなくなったため`ttk::`は名前空間から外れる点に注意）。いずれも中身はコア(`cpp_tk.hpp`)の公開APIのみで組み上げられており、Tcl/Tkの内部詳細には一切触れていない。これにより「coreはTcl/Tkウィジェットとの1:1の薄いラッパーのみ」という原則を保ちつつ、便利な合成ウィジェットも提供できる。
- ビルドへの影響: `CMakeLists.txt`の`add_library(cpp_tk ...)`に`custom.cpp`を追加（同一の`cpp_tk`ライブラリターゲットに含める。ヘッダのみ分離しライブラリは分割していない）。回帰テストは[test/test_new_classes.cpp](../test/test_new_classes.cpp)で`cpp_tk::custom::ScrolledText`/`cpp_tk::custom::LabeledScale`に追従済み。

### 第4回棚卸し（2026-07-05対応済み、Tk/Tclのネイティブコマンド単位で欠落を点検）
第1〜3回はいずれもクラス/メソッド単位の欠落を対象としていたが、`bell`/`tkwait visibility`/`tk scaling`のような「特定のウィジェットに属さないネイティブTclコマンド」の観点では棚卸ししていなかった。ユーザの指摘を受け、本家tkinterの同等コマンドと突き合わせ、重要度が高・中のもの6項目を追加した。[test/test_native_misc.cpp](../test/test_native_misc.cpp)で全項目を回帰テスト化(6 TEST_CASE、全green)。

- [x] `Widget::windowingsystem()` — `tk windowingsystem`（`x11`/`win32`/`aqua`のいずれかを返す）
- [x] `Widget::bell()` — `bell`（システムビープ音）
- [x] `Widget::wait_visibility()` — `tkwait visibility`（ウィジェットが実際に画面へマップされるまでブロック）
- [x] `Widget::scaling()` / `Widget::scaling(double)` — `tk scaling`（DPIスケーリング係数のgetter/setter）。Tk内部でピクセル単位に丸められるため、setした値と厳密には一致しない場合がある（テストでは`doctest::Approx(...).epsilon(0.01)`で許容）
- [x] `image_names()` / `image_types()`（フリー関数） — `image names`/`image types`。`PhotoImage::destroy()`/`BitmapImage::destroy()`も併せて追加（`image delete`。cpp_tkは画像の自動破棄をしない設計のため、`Widget::destroy()`と同様に明示呼び出しが必要）
- [x] `Widget::bindtags()` / `Widget::bindtags(vector<string>)` — `bindtags`（バインドタグの参照順序のgetter/setter）

優先度を下げて今回も対象外: 上記以外の低頻度ネイティブコマンド（`tk appname`、`tk fontchooser`等）。

### 第5回棚卸し
第1〜4回はいずれも「ユーザが気づいた穴をきっかけに、その観点で横串を通す」形だったため、`cpp_tk.hpp`全クラス（2036行）を通読し、本家tkinter/ttkの対応APIと1つずつ突き合わせる形で実施した。B/C/D区分は棚卸し直後に対応済み。A区分は「腰を据えて対応する問題」として一旦保留した上で、改めて優先順位付け（1→2→3→4の順）を行い対応した（2026-07-05対応済み）。

**A. 実利用上のインパクトが大きいと判断したもの（既存パターンとの一貫性の欠如＝バグに近い）（2026-07-05対応済み）**
- [x] **`Menu`の各項目に`std::function`のコマンドコールバックを設定する手段が無い**: `add_command`/`add_checkbutton`/`add_radiobutton`/`insert`/`entryconfigure`はいずれも`-command`オプションを`ArgValue`（＝文字列等）としてしか受け付けられず、`Button::command()`/`Scale::command()`のような`std::function<void()>`版が存在しない。ユーザーが独自にコールバックを登録する公開APIも無い（`register_void_callback`はprotected）。実際、`OptionMenu`の内部実装は自分自身が`Widget`のprotectedメンバに直接アクセスできることを利用して`add_command({{"command", 内部で生成した文字列}}})`という回避策を取っており、外部ユーザーはこれができない。**メニュー項目クリックにコールバックを割り当てられないのは本家の最頻出パターンが使えないに等しく、最優先で対応。**
  - 対応内容: 上記5メソッド全てに`std::function<void()> callback = nullptr`引数を追加（既定値ありのため既存呼び出しは無変更で動作する）。callback指定時のみ`register_void_callback`で一意な名前のプロシージャを登録し`-command`として渡す。項目はTclの数値indexでしか識別できずindexは挿入/削除で変動するため、コールバック名はMenuインスタンス単位ではなくプロセス全体で単調増加するカウンタ(`next_menu_entry_callback_name`)から生成し衝突を避けている（既存の`-command`は指した先のプロシージャ自体は変わらないため、後からindexがずれても実害は無い）。
- [x] **`ttk::Treeview::heading()`に列ヘッダクリック時のコールバックを設定する手段が無い**: 同様に`-command`が`ArgValue`のみで、「ヘッダクリックでソート」という本家の定番パターンが組めない。
  - 対応内容: `heading(column, options, callback = nullptr)`を追加。列名(`column`)はindexと異なり挿入/削除で変動しない安定した識別子のため、Menuと違いカウンタを使わず`full_name+column`から決定的にコールバック名を生成している。
- [x] **`Listbox`に`xview`/`yview`が無い**: `yscrollcommand`（Scrollbarの位置表示を更新する向き）は登録できるが、逆方向の「Scrollbarの`command`からListboxへスクロールを指示する」`yview()`メソッドが無いため、`Scrollbar+Listbox`を双方向連携させる本家の定番パターンが片方向しか組めない。`Canvas`/`Text`/`Treeview`は同種のメソッドを持つが`Listbox`だけ欠けていた。
  - 対応内容: `Text::yview`/`Canvas::xscrollcommand`と同型で`yview`/`xview`/`xscrollcommand`を追加(`yscrollcommand`は既存)。
- [x] **`Entry`/`ttk::Entry`に`xview`/`xscrollcommand`が無い**: 長い文字列を入力するEntryの横スクロール連携（本家の`xscrollcommand=scrollbar.set` / `scrollbar.command=entry.xview`）が組めない。
  - 対応内容: classic/ttk両方の`Entry`に`xview`/`xscrollcommand`を追加。

**テストに関する既知の注意点（今回の実装時に判明）**: `xview`/`yview`呼び出し直後に`-xscrollcommand`/`-yscrollcommand`コールバックが同期的に発火するかはウィジェット・プラットフォーム実装依存で不安定だった（同じ手順でclassic Entryは発火する場合があるがttk::Entryでは発火しない、classic EntryとttkEntryでカーソル追従による自動スクロール巻き戻りの挙動も異なる、等）。そのため[test/test_round5_a.cpp](../test/test_round5_a.cpp)ではコールバックの発火有無ではなく、(1)`xview`/`yview`呼び出しが例外にならないこと、(2)`cget("xscrollcommand"/"yscrollcommand")`が正しく設定されること、の2点で検証している(Listboxの`yview`のみ実際の表示位置変化も検証できたため追加で確認済み)。

B/C/D区分は[test/test_round5_bcd.cpp](../test/test_round5_bcd.cpp)(14 TEST_CASE、44 assertion)、A区分は[test/test_round5_a.cpp](../test/test_round5_a.cpp)(6 TEST_CASE、16 assertion)、simpledialogは[test/test_simpledialog_return.cpp](../test/test_simpledialog_return.cpp)/[test/test_simpledialog_escape.cpp](../test/test_simpledialog_escape.cpp)で回帰テスト化した。これで第5回棚卸しのA〜D区分は全て対応済み。

**B. 既知だが規模がある機能不足（第2・3回で優先度を下げた延長）（2026-07-05対応済み）**
- [x] `PhotoImage`/`BitmapImage`の画像操作系: `put`/`get`/`zoom`/`subsample`/`copy`/`copy_from`/`blank`、および`width()`/`height()`アクセサ(Tk"image width/height"は本家のImage共通基底が持つため`BitmapImage`にも追加)。`read`/`write`(ファイルI/O)は今回も対象外(用途が限定的なため見送り)。
- [x] `simpledialog`モジュール丸ごと: `askstring`/`askinteger`/`askfloat`。**ScrolledText等と同じ理由でcore(`cpp_tk`名前空間)ではなく`cpp_tk::custom::simpledialog`に配置した**(本家simpledialogもTclネイティブコマンドではなくToplevel+Label+Entry+Buttonを組み立てるPure Python実装であり、「単一のTclウィジェットとして代替不可能」ではないため)。`askinteger`/`askfloat`は`askstring`の上に構築した簡略版で、数値として解釈できない入力は本家のような再入力プロンプトを出さずキャンセル扱いにする。本家同様`<Return>`→OK確定、`<Escape>`→キャンセルのキーバインドも追加した。
- [x] `ttk::Style`の高度なテーマ機能: `layout`/`element_create`/`theme_create`。いずれもネストしたTclリストやスクリプト文字列の組み立てはユーザー側に委ねる薄いラッパー。

**C. 単発の小粒な欠落（実利用頻度は中程度）（2026-07-05対応済み）**
- [x] `Text::bbox(index)`（指定位置の外接矩形。`Canvas::bbox`と同じパターンで実装）
- [x] `Text`の`xview`/`xscrollcommand`（横スクロール。`yview`/`yscrollcommand`と対称に実装）
- [x] `font`名前空間の静的関数`families()`/`names()`、`Font::copy()`（`copy()`は`font actual`で全属性を読み出し新規フォントとして`font create`し直す実装）
- [x] `Canvas::postscript()`（Canvas内容をPostScriptとして書き出す）

**D. 低頻度・低優先度（本家にはあるが実利用頻度が低い）（2026-07-05対応済み）**
- [x] `Widget::grab_current()`/`grab_status()`
- [x] `Widget::winfo_id()`/`winfo_name()`/`winfo_parent()`/`winfo_depth()`/`winfo_geometry()`/`winfo_containing(x,y)`
- [x] `Widget::option_add()`/`option_get()`（Tkオプションデータベース）
- [x] `Widget::tk_focusNext()`/`tk_focusPrev()`（`nametowidget()`と同じ簡略版でWidgetとして返る）
- [x] `Toplevel`/`Tk`の`wm iconname`

B/C/D区分は[test/test_round5_bcd.cpp](../test/test_round5_bcd.cpp)(14 TEST_CASE、44 assertion)、および[test/test_simpledialog_return.cpp](../test/test_simpledialog_return.cpp)/[test/test_simpledialog_escape.cpp](../test/test_simpledialog_escape.cpp)で回帰テスト化した(A区分の対応・テストは上記の通り別途完了)。

**副産物として発見したバグ・制約（テスト作成中に判明）**:
- **`askstring()`（`grab_set()`を伴うモーダルダイアログ）を同一プロセス内で2回連続実行するとハングする**: `test_simpledialog_return.cpp`と`test_simpledialog_escape.cpp`の2つのテストケースを同一実行ファイル内に置くと、実行順によらず2回目の`wait_window()`が永久に返らなかった(個別に実行すれば両方とも問題無く成功する)。原因はgrab関連のプラットフォーム側状態が単一プロセス内での2回目のTk_Init系サイクルに影響するためと推測される(cpp_tk自体の不具合というより、Windows環境固有のTk grab実装の制約の可能性が高い)。既存のテストプロセス分離方針(test/CMakeLists.txt冒頭のコメント、F節参照)が「未検証のリスク」としていたものが、`grab_set()`を伴うケースに限り実際に顕在化した形。対応として、この2ケースのみ個別の実行ファイル(`test_simpledialog_return.cpp`/`test_simpledialog_escape.cpp`)に分離した。**`grab_set()`を使うテストコードを今後追加する際は、同一プロセス内で複数回`grab_set()`を伴うモーダルフローを実行しないよう注意する必要がある。**
- **doctestの`REQUIRE`/`CHECK`をTcl起動コールバック(`after`/`bind`等)の中で使ってはならない**: これらのマクロは失敗時に例外を投げるが、コールバックは`invoke_guarded`経由で例外を握りつぶす設計のため、アサーション失敗がコールバック内で起きると単に握りつぶされて処理が止まり、`wait_window()`等の待機がハングする形で顕在化する(実際に`test_simpledialog_*`の開発中に踏んだ)。コールバック内では例外を投げない条件分岐のみを使い、判定は呼び出し元(コールバックの外)のCHECK/REQUIREに委ねる必要がある。

---

## B. C++特有の理由で意図的に本家と変えている設計（現状は妥当と判断）

- **Widgetのコピー禁止＋`shared_ptr`の`handle()`方式**（[cpp_tk.hpp:317-419](../cpp_tk.hpp#L317-L419)）
  Pythonの参照カウント付きオブジェクトに相当する仕組みを、直近のダングリングポインタ対策コミット群で確立。コールバックからは`handle()`経由で再構築してキャプチャする方針。
- **親を必須の参照で渡す構築方式**
  Pythonの暗黙デフォルトroot省略（`master=None`）を許さず、グローバル可変状態を持たない設計。ただし呼び出し側は常に明示的にparentを渡す必要があり、Pythonに比べ冗長になる（利便性とのトレードオフとして明文化しておく価値あり）。
- **`std::map<std::string, ArgValue>` によるオプション指定**（`**kwargs`の代替）
- **`ArgValue` タグ付きユニオン**による動的型のエミュレーション

これらはコメントで意図が説明済み。今後変更する場合は「なぜ変えたか」をREADME等に明文化した方が、後任が誤って本家に寄せて壊さないための保険になる。

**2026-07-05対応済み**: README.md全体を刷新し、上記B節の内容（Widgetのコピー禁止＋`handle()`方式とその安全なコールバックキャプチャ規約、親を必須引数で渡す方式）に加え、C節8.（ユーザコールバックの`handle()`規約破りによる検知不能なSegmentation Fault）、現状のInterpreter設計上の制約（1スレッドにつき1つのTclインタプリタのみ）、C++11ターゲット、macOS未検証、を「⚠️ 制限事項」節としてREADME.mdに明文化した。あわせて、実装規模に対して内容が薄かった点（Widget派生38クラス・ttk完全対応・エラーハンドリング・doctestベースのテスト体制とCI等が未紹介だった）、および実態と合っていなかった記述（`example/`単体でのビルド手順、依存関係インストール手順の欠落、macOS対応を謳っていた点)を修正し、依存関係インストール手順(Windows/MSYS2・Linux/apt)・使用例の追加・CIバッジ・sanitizerビルドの案内・Qt/wxWidgetsとの位置づけの違いの説明を追加した。

---

## C. 構造的にユーザ体験を損なっている実装（優先して整理したい課題）

1. **エラーが握りつぶされる（2026-07-05対応済み）**
   [Widget::config](../cpp_tk.cpp#L747-L762) は `call()` の成否を見ずに投げっぱなしで、存在しないオプション名や不正な値を渡しても静かに失敗していた。例外機構が皆無なため、タイポや誤ったoption名を実行時に検知する手段がユーザに提供されていなかった。

2. **未初期化ウィジェットの警告がstderr頼み（2026-07-05対応済み）**
   デフォルト構築のWidget（`interp == nullptr`）への呼び出しは `std::cerr` に出すだけで、戻り値は空文字列/falseになっていた。GUIアプリはコンソールを持たないことが多く、警告がユーザから見えないままバグが埋もれる問題があった。

   **1・2共通の対応内容**: `cpp_tk::Error`（`std::runtime_error`派生の唯一の例外型）と`cpp_tk::ErrorPolicy`（`set_error_policy`/`error_policy`で切替）を追加。判定ロジックは[InterpreterClient::checked_interp/call](../cpp_tk.hpp#L161-L230)の1箇所に集約した。
   - `ErrorPolicy`はビットフラグの`enum class`（`DEFAULT = 0`が既定、`LENIENT_CALL`/`LENIENT_THREAD`の各ビットを`operator|`で組み合わせて指定する。詳細は下記「2026-07-05 追記」を参照）。
   - stderrへのログ出力はポリシーに関わらず必ず行われる(診断性は維持)。
   - 呼び出し側が`bool* success`を渡している既存呼び出し(`cget`/`index()`/`winfo_exists()`等、約30箇所)は、ポリシーに関わらず例外を投げない(`*success = false`を設定して継続する)。挙動は完全に不変。
   - `success`を渡していない呼び出し(`config()`等)は、対応するカテゴリのビットが立っていなければ`Error`を送出し(既定はDEFAULT=送出)、立っていれば従来通りログのみで空文字列/false/no-opとして継続する。
   - Release等で例外を出したくない場合は起動時に`cpp_tk::set_error_policy(cpp_tk::ErrorPolicy::LENIENT_CALL | cpp_tk::ErrorPolicy::LENIENT_THREAD);`を1行呼ぶだけで2026-07-05以前と同じ挙動に戻せる。

   **2026-07-05 追記(ErrorPolicyのビットフラグ化)**: 当初`STRICT`/`LENIENT`(旧名)の2値のみで、クロススレッド検知(下記E節)は「深刻度が違う」という理由で常に例外にする特別扱いにしていた。しかし検討の結果、クロススレッド検知自体は`Tcl_Interp`に一切触れる前に働くガードであり、「例外を投げる」か「ログのみで継続する」かのどちらを選んでも**危険なTcl呼び出しをしない**という安全性は同じ(呼び出し元への通知方法が違うだけ)と判断した。そのため特別扱いをやめ、`ErrorPolicy`を`DEFAULT = 0` / `LENIENT_CALL = 1u << 0`(config失敗・未初期化アクセス) / `LENIENT_THREAD = 1u << 1`(クロススレッドアクセス)というビットフラグに再設計し、カテゴリごとに独立して緩められるようにした(`operator|`/`operator&`/`has_error_policy`を追加)。「typoは厳しく検知したいが、低頻度で発生しうるクロススレッドの取りこぼしは業務継続を優先してログのみにしたい」のような使い分けが1つのグローバル設定で可能になる。一時サンプルで、DEFAULT/LENIENT_CALLのみ/LENIENT_THREADのみ/両方、の4パターン全てが独立して期待通り動作することを確認済み。

   **コールバック境界の防御(1・2の前提条件として同時対応)**: TclはCライブラリであり、`register_void_callback`等5つ＋`trace_var`3つ、計8箇所のコールバックトランポリンは`Tcl_CreateCommand`/`Tcl_TraceVar`でC関数ポインタとしてTcl側に登録され、TclのCイベントループから直接呼ばれる。ここでC++例外を投げっぱなしにすると、Tcl内部のCスタックフレーム(例外テーブルを持たない)を巻き戻ろうとして未定義動作になる。`button.command([&](){ label.config(...); })`のように「コールバックの中からconfig()等を呼ぶ」パターンは非常に一般的なため、`Error`導入(1・2の対応)より前に、あるいは同時にこの境界を塞ぐ必要があった。
   - 全8箇所のトランポリンで、ユーザコールバック本体の呼び出しを`invoke_guarded()`という共通ヘルパー経由にし、内部で発生した例外(cpp_tk::Errorに限らず`std::exception`全般、およびそれ以外の例外も)を必ず捕捉してTcl側には一切伝播させないようにした。
   - 捕捉した例外は`cpp_tk::set_callback_exception_handler(std::function<void(const std::exception&)>)`で差し替え可能なハンドラに渡す(既定はstderr出力)。本家Python tkinterの`Tk.report_callback_exception`と同じ役割。ハンドラ自身が例外を投げても、そこでも握りつぶしてTcl側には伝播しない。
   - `register_bool_callback`(Entry::validate等)がコールバック内で例外を送出した場合は、判定不能とみなしfalse(編集拒否)側にfail closedする。
   - 一時サンプルで、未初期化アクセス・不正option・LENIENT切替・コールバック内例外の4パターン全てが期待通り動作することを確認済み。

3. **`Var*` 生ポインタによる変数バインディング（2026-07-05対応済み、破壊的変更）**
   `Checkbutton::variable` / `Radiobutton::variable` / `Entry::textvariable` / `Combobox::textvariable` / `Spinbox::textvariable`が全て素の`Var*`を受け取り、所有権・生存期間の契約がドキュメント化されていなかった。

   **調査で判明した実害**: 単なる型安全性の問題だけでなく、`tk::Entry`/`ttk::Entry`/`ttk::Combobox`は渡された`Var*`を`text_var_`という内部メンバに**保持**し、後から`get()`/`set()`で使い回していた。これは実際のダングリングポインタ発生源になり得た。他方`Checkbutton`/`Radiobutton`/`Spinbox`は`var->name()`を呼んだ瞬間しか使わず保持していなかったため、実害はなかった。

   **対応内容**:
   - `Entry`(classic/ttk)・`Combobox`から`text_var_`メンバを完全に削除した。Tkの`-textvariable`はウィジェットの表示テキストとTcl変数を双方向で自動同期する仕様のため、`get()`は常にウィジェット自身への`call()`(`entry get`/`combobox get`)で完結し、Var側を後から参照する必要がそもそも無かった。これによりダングリングポインタの発生源が構造的に消えた。
   - `Entry::set()`は**本家Python tkinterにもTclの`entry`ウィジェットコマンドにも存在しない**cpp_tk独自追加のメソッドだったため、削除した(既存の代替手段である`erase()`+`insert()`、またはtextvariable経由の`Var::set()`を使う)。一方`Combobox::set()`/`get()`は本家Python(`ttk.Combobox.set/get`)およびTcl(`ttk::combobox set/get`)の両方に実在するため維持し、`text_var_`ではなく本来の`ttk::combobox set`サブコマンドを直接呼ぶ実装に直した。
   - `Entry::textvariable`/`Combobox::textvariable`/`Spinbox::textvariable`は`StringVar&`(具体型・非const参照)に変更。`Checkbutton::variable`/`Radiobutton::variable`は本家同様BooleanVar/IntVar/StringVar等を柔軟に受け付ける必要があるため`Var&`(基底型のまま・非const参照)に変更した。
   - 非constの参照にしたのは、`const Var&`だと右辺値(名前を持たない一時オブジェクト)にもバインドできてしまい、「C++側から二度と参照できないTcl変数」を意図せず作れてしまうため。非const参照は右辺値を弾き、名前を持った(＝ある程度生存期間のある)オブジェクトを渡すことを型システムで強制する。
   - 副次的に、`ArgValue(Var& var)`という委譲コンストラクタを追加し、`config({{"variable", my_var}})`のように`.name()`を書かずに`Var`を直接渡せるようにした(本家Pythonの`Checkbutton(variable=my_var)`の書き味に近づけた)。これも`variable()`/`textvariable()`と同じ理由で非const参照にしている。`Menu::add_checkbutton`/`add_radiobutton`は専用の`variable()`メソッドを持たず`options`マップ経由でしか変数を紐付けられないため、ここを`const`のままにすると`menu.add_checkbutton({{"variable", tk::BooleanVar(root)}})`のように一時変数を渡すコードが素通りしてしまい、専用メソッド側で塞いだはずの穴が別経路から再侵入する。非constにしたことで同様にコンパイルエラーになることを確認済み。
   - 破壊的変更に伴い[example/sample2.cpp](../example/sample2.cpp)の`entry.set("Hello World")`を`entry.insert("0", "Hello World")`に修正済み。

4. **エスケープハッチが無い（2026-07-05対応済み）**
   `call()` は `protected` で、未ラップのTclサブコマンド/オプションをユーザが直接叩く手段（本家の `self.tk.call(...)` 相当）が無かった。
   **対応内容**: `Widget`/`PhotoImage`/`font::Font`/`ttk::Style`/`Var`の5クラスがそれぞれ個別に持っていた「nullptrガード＋エラーメッセージ＋`Interpreter::call`への委譲」という同型の実装を、共通基底クラス [InterpreterClient](../cpp_tk.hpp#L161-L188) に一本化した。
   - 各クラスは`interp()`という純粋仮想アクセサ(「自分のInterpreterポインタがどこにあるか」を返すだけ)と、診断メッセージ用の`type_name()`をoverrideするだけでよく、`call()`の実処理はInterpreterClientに1箇所しか存在しない。
   - `friend`は一切使わない(標準的な仮想関数オーバーライドのみ)。
   - `Var::get_var`/`set_var`/`trace_var`も同型のガードを持っていたため、`InterpreterClient::checked_interp(operation)`という保護ヘルパーを介して同じ仕組みに乗せた（`call()`以外の操作にも使える形に一般化）。
   - `Widget`の`register_void_callback`等5つの内部メソッドも同型のガードを個別に持っていたため、ついでに`checked_interp`経由に統一した。
   - Widget内部の既存`call(...)`呼び出し（約200箇所）は継承した`call()`をそのまま使うため、**1箇所も変更不要**だった。
   - 設計の経緯: 当初は「Widgetだけpublic化」「4クラスまとめてpublic化(friend使用)」「`Object`への統合」も検討したが、(a) `Widget`は`impl_`が`shared_ptr<Impl>`経由でありPhotoImage/Font/Style/Varの直接`interp_`保持と格納方式が異なるため単純なstorage共有は不可、(b) `friend`はInterpreter側が利用者クラスを列挙する形になり結合が不透明、(c) `Object`は「Tcl側に名前付きオブジェクトを持つか」という別の関心事(`ttk::Style`は意図的に`Object`を継承しない)であり混在させるべきでない、という理由から、仮想関数ベースの独立した`InterpreterClient`基底に決定した。

5. **ID/インデックスの流儀が要素ごとにバラバラ（2026-07-05対応済み、破壊的変更）**
   Listboxが`int`版indexと`string`版indexを二重に持っていた（A節の対応でstring版を追加した際、互換性のため一時的にint版と共存させていた）。
   **対応内容**: 破壊的変更を許容し、Listboxの`insert`/`erase`/`get`/`see`/`select_set`/`select_clear`/`select_includes`/`activate`から`int`版オーバーロードを削除し、`std::string`版（`END`/`ACTIVE`等のシンボリック定数、または`"0"`のような数値の文字列表現を受け付ける）に一本化した。`nearest(int y)`（スクリーン座標）・`size()`（件数）・`curselection()`（`vector<int>`、本家Python同様「位置の集合」を返す）は元々index引数ではないため対象外。
   併せて`ttk::Notebook::select(int index)`も`select(const std::string& tab_id)`に変更した。Tclの`ttk::notebook select`はタブの数値index文字列だけでなく、タブ内容ウィジェットのフルネームや`"current"`も受け付けるため、単なる統一以上に実用上の意味がある変更。一方`ttk::Combobox::current(idx)`は`ttk::combobox current`がシンボリック定数を受け付けず整数専用のため、対象から除外した（文字列化してもtypecast強制になるだけで実益が無いため）。
   破壊的変更に伴い[example/sample2.cpp](../example/sample2.cpp)の`listbox.insert(0, ...)`のような呼び出しを`listbox.insert(tk::END, ...)`に修正済み。

6. **`Var`/`PhotoImage`/`BitmapImage`/`font::Font`/`ttk::Style`の`parent`引数が死んだパラメータになっていた（2026-07-05対応済み、破壊的変更）**
   これら5クラスのコンストラクタは全て`const Widget& parent`を必須引数として取っていたが、実装を確認したところ**一切参照されていなかった**（スレッド安全性対応で`current_interp()`を導入した際に内部実装だけ差し替えられ、引数の整理が漏れていた）。他の自由関数API（`image_names()`/`font::families()`等）は元々parentを取らずcurrent_interp()だけを見ており、実装と体裁が一致していなかった。

   **対応内容**: 5クラス全てから`parent`引数を廃止した。各クラスは単一のコンストラクタで完結する形に統一し、呼び出しスレッドのcurrent interpreter（`current_interp()`）があればそれに束縛して実体を作り、無ければ`Widget()`と同様の未初期化プレースホルダのままになる（Python本家の`StringVar()`等が引数無しで書ける点にも近づいた）。
   - 当初「`static XxxVar::create()`のようなファクトリメソッドを追加する」案を検討したが、本家Python実装にファクトリパターンは無く実装が乖離するため却下した。代わりに**単一コンストラクタが呼び出し時点のcurrent_interp()状態に応じて挙動を変える**設計にした。Tk構築前にメンバとして仮置きしたい場合は、`Widget`と同じ流儀（`var = StringVar();`のような構築し直し＋move代入）で後から実体化できる。
   - `PhotoImage::copy()`/`zoom()`/`subsample()`と`Font::copy()`は内部で「デフォルト引数のコンストラクタで空の複製を作ってから明示的に`image create`/`font create`し直す」実装だったが、単一コンストラクタ化によりデフォルト引数呼び出し自体が既に実体(空画像/既定フォント)を生成するようになったため、二重生成（`PhotoImage`は実害無しだが`Font`は`font create`が同名エラーになる実害あり）を避けるよう、生成済みの実体に対して`copy`/`configure`するだけの実装に修正した。
   - **既知の限界（2026-07-05時点。7.でさらに変更）**: `current_interp()`は「呼び出しスレッドで最後に構築された`tk::Tk`」を指すため、同一スレッドで複数の`tk::Tk`ルートを同時併存させて使い分ける用途は元々サポートされていない（`interp_map`が`thread_id`単位で1エントリしか持てない設計のため）。`parent`廃止によってこの制約が新たに生まれるわけではない。
   - **テストへの影響**: `test_interpreter_client.cpp`の「未初期化オブジェクトへのcall()」テストケースは、同一プロセス内の別のTEST_CASEが既に`tk::Tk`を構築しているとcurrent_interp()が非nullptrになってしまい「未初期化」を検証できなくなったため、`test_uninitialized_objects.cpp`という専用ファイル(一度も`tk::Tk`を構築しない)に分離した。

7. **`tk::Tk`がInterpreter生成の唯一の入口になっていた（2026-07-05対応済み、破壊的変更）**
   6.の対応後も、実際に`new Interpreter()`して`interp_map`に登録するのは`Tk::Tk()`だけであり、`current_interp()`は「あれば返す・無ければnullptr」という受動的なgetterのままだった。この設計だと、同一スレッドで`tk::Tk`を2回構築すると2つ目が`interp_map`のエントリを無条件に上書きし、1つ目のInterpreterは（`interp_map`経由では）迷子になる（1つ目の`Tk`オブジェクト自身は`impl_->interp`に直接ポインタを持っているため動作自体は継続するが、以後`current_interp()`を通る他クラスは2つ目に束縛される）。

   検討の結果、「Interpreterはスレッドごとにちょうど1つだけ存在する遅延生成のシングルトンであればよい」という方針に転換した。
   - `current_interp()`自体を「あれば返す・無ければその場で`new Interpreter()`して登録する」getter-or-createに変更した。`Tk::Tk()`は`impl_->interp = current_interp();`に簡略化され、上書きは構造的に発生しなくなった（2回目の`tk::Tk`構築は同じInterpreter・同じ"."ウィンドウを再利用するだけになる）。
   - 副作用として、`Var`/`PhotoImage`/`BitmapImage`/`font::Font`/`ttk::Style`の「`tk::Tk`が無ければ未初期化プレースホルダのままになる」という6.の挙動は無くなった。`current_interp()`が呼ばれた時点でInterpreterが無ければ生成されるため、これらのクラスは`tk::Tk`構築前に単独で構築しても即座に実体化される。意図的な仕様変更であり、「メンバとして明示的な初期化無しに持てた方がユーザ体験が良い」という判断による。
   - `image_names()`/`image_types()`/`font::families()`/`font::names()`も、従来は`interp_map[...]`を直接参照し「無ければ空を返す」問い合わせ専用の実装だったが、同様に`current_interp()`経由（自動生成あり）に統一した。「呼び出したのに空を返す（実際にはフォント/画像が存在するのに『無い』と偽の回答をする）方が、隠れたウィンドウが生成される驚きより実害が大きい」という判断による。
   - **テストへの影響（同一プロセス内の状態共有）**: `tk::Tk`が使い回されるようになったことで、1つのテスト実行ファイル（doctestは全TEST_CASEを同一プロセスで順に実行する）内の複数TEST_CASEが同じ"."ウィンドウ・同じInterpreterを共有するようになった。これにより、それまで「`tk::Tk`は毎回まっさらな新しいInterpreterを作る」という旧実装の副作用に暗黙で依存していた一部のテストが顕在化して壊れた。
     - `wait_visibility()`（Tclの`tkwait visibility`）は状態の**変化**を待つ命令であり、先行するTEST_CASEが既にmapped状態にしていると`deiconify()`だけでは新たな変化が起きず永久にブロックする（`test_native_misc.cpp`のwait_visibilityテスト自身、`test_round5_bcd.cpp`のgrab/winfo系2箇所がタイムアウトしていた）。対応として、各該当TEST_CASEの冒頭で`root.withdraw(); root.deiconify();`という順で明示的に一度unmapしてから戻すことで、どの状態から始まっても確実に変化を起こすようにした。
     - `test_widget_basics.cpp`の`bind/unbind`テストは、直前の`grid_rowconfigure`テストが`pack()`した`Frame`を`destroy()`せず放置していたため、極小サイズ(`1x1`)のroot内でpack領域を奪い合い、新しいFrameの実効サイズが0になってイベントが配送されなかった。放置していたテスト側で使用後に`f.destroy()`するよう修正した。
   - `test_uninitialized_objects.cpp`は、Var/PhotoImage/Font/Styleの「未初期化」を検証する場ではなくなったため、「`tk::Tk`を一度も構築していないプロセスでも、Widget派生（要`parent`）はプレースホルダのままだが、Var/PhotoImage/Font/Styleは自動的に実体化される」ことを検証する内容に書き換えた。

8. **【最重要・未対応】ユーザコールバックが`handle()`規約を破ると、検知不能なSegmentation Faultになり得る**
   `bind()`/`command()`/`after()`/`trace()`等に渡すコールバックは、`Widget::handle()`(`shared_ptr<Impl>`)経由でキャプチャする規約になっている（B節、[Widget::handle()](../cpp_tk.hpp#L763-L774)のコメント参照）。ライブラリ内部のコールバック登録箇所（`custom.cpp`のScrolledText/LabeledScale、`Tk`/`Toplevel`のWM_DELETE_WINDOWハンドラ等）はこの規約を徹底しているが、**これはあくまで人間が守っている規約であり、型システムやAPIの形で強制されていない**。

   **具体的な危険**: コールバックの実体（`std::function`）は、登録元の`Widget`ではなく`Interpreter`(意図的にリークされプロセス終了までほぼ確実に生き続ける)が持つ`void_callback_map_`等のマップに格納される（[Interpreter::register_void_callback等](../cpp_tk.cpp)）。そのため、コールバックのクロージャの寿命は登録元`Widget`オブジェクトの寿命と完全に独立している。もしユーザがコールバックで`[this]`や`Widget&`を`handle()`を介さず直接キャプチャした場合、`Widget`破棄後もそのコールバックは(`unbind()`されない限り)マップに残り続け、対応するTclイベントが発火するたびに**解放済み/再利用済みのメモリを参照する**。これは以下の理由で最悪クラスの不具合になる:
   - ダングリングポインタの参照はC++例外を投げない。既存の`invoke_guarded()`（`try`/`catch`によるコールバック境界の防御、C-4.参照）はC++例外しか捕捉できないため、**この種の未定義動作に対しては一切機能しない**。
   - SIGSEGVで即座にプロセスが落ちるか、運悪くまだ無効化されていないメモリを指していて**エラーも出さず静かに誤動作する**かのどちらかであり、`ErrorPolicy`/`Error`の仕組みの外側で発生する。
   - 発生タイミングは「対象ウィジェット破棄後、次にそのイベントが発火した時点」であり、破棄した箇所とは時間的・コード的に離れているため、クラッシュ時のスタックトレースから真因（いつ・どこで対象が破棄されたか）に辿り着くのが非常に困難。再現性もメモリ再利用状況次第で低い。

   **現状ライブラリ側に検知手段が無いことを確認済み**: `handle()`経由で正しくキャプチャしていれば安全（Tcl側の実体が既に無い場合も、`checked_interp()`経由の通常のTclエラーとして安全に失敗する、C-4.参照）。しかし規約破りをコンパイル時にもランタイムにも検出する仕組みは現状存在しない。

   **検討した対処候補**: C++にGCが無い以上、ユーザが何を意図的にキャプチャするかを型システムだけで完全に強制するのは原理的に不可能。そのため「根絶」ではなく「軽減」を目的に以下を比較検討した。
   - (採用) **AddressSanitizer/UndefinedBehaviorSanitizerのCI導入**: 発生を未然に防ぐのではなく、テスト実行時に確実に(発生箇所・確保/解放箇所付きで)顕在化させる。詳細は下記「2026-07-05追記(sanitizer)」参照。
   - (試作したが撤回) **`keep_alive()`ヘルパー**: 複数Widgetの`handle()`を`std::tuple`にまとめて保持するヘルパー`WidgetGroup<Ts...>`/`keep_alive()`を一度実装したが、撤回した。理由は下記「2026-07-05追記(keep_alive()を撤回した理由)」参照。
   - (却下) **Tcl側実体の寿命をshared_ptr参照カウントに連動させる(Python参照セマンティクスに寄せる)案**: 「Tcl側実体をいつ自動破棄するか」という別の関心事への対処であり、ユーザが生ポインタ/参照をキャプチャする誤りそのものは防げないため、根本対策にならないと判断し見送った。加えて「明示的destroy()のみ、暗黙のRAII cleanupはしない」という既存方針(B節)の見直しを伴う。
   - (却下) **コールバックシグネチャを変更し、`handle()`から再構築したWidgetを引数として自動的に渡す案**（例: `button.command([](Widget self){...})`）: 自分自身を参照する最頻出パターンには効くが、ScrolledTextの例のような兄弟ウィジェット参照には効かず、`command`/`bind`/`trace`等十数箇所のシグネチャ変更を伴う大きな破壊的変更になる上、本家tkinterのコールバックシグネチャからさらに乖離するため見送った。

   **2026-07-05追記(`keep_alive()`を撤回した理由)**: 複数のWidgetを1つのコールバックでまとめて安全に参照するためのヘルパー`keep_alive()`/`WidgetGroup<Ts...>`を一度実装したが、`std::tuple`に詰めるとインデックスに名前が付かず可読性を損なうと判断し撤回した。
   ```cpp
   // 撤回前の書き方(可読性に問題があった)
   auto handles = keep_alive(scrollbar_, text_);
   scrollbar_.command([handles](const std::string& args) {
       auto widgets = handles();
       std::get<1>(widgets).yview(args); // text_ ← インデックスだけでは何のWidgetか分からずコメントで補うしかない
   });
   ```
   - `std::get<N>`は索引に名前が無くコメントに頼らざるを得ない上、`keep_alive()`への引数順を後で入れ替えると対応する`std::get<N>`側も追随させる必要がある。**しかも同じ型のWidgetが複数あると、入れ替えても型エラーにすらならず、意味的に誤ったWidgetを参照するサイレントなバグになり得る**。この保守性の低下は、Widget数が多い場合に`handle()`を1つずつ書く手間(1個あたり1行)を上回るコストだと判断した。
   - 代わりに、`custom.cpp`に元々あった「1つずつ`handle()`をキャプチャして`Type(handle)`で再構築する」という素直な名前付きコピーキャプチャのイディオムを、そのまま踏襲することにした。
   ```cpp
   auto scrollbar_handle = scrollbar_.handle();
   auto text_handle      = text_.handle();
   text_.yscrollcommand([scrollbar_handle](const std::string& args) {
       Scrollbar(scrollbar_handle).set(args);
   });
   ```
   - **撤回後も残した副産物**: `WidgetGroup`が任意の`Widget`派生型`Ts`を`Ts(shared_ptr<Impl>)`で再構築できる必要があったため、これまで一部のクラス(Tk/Toplevel/Canvas/Scrollbar/Text/ttk::Frame/ttk::Label/ttk::Scale/ttk::Scrollbarの9クラスのみ)にしか無かった`using Widget::Widget;`を、**Widget派生38クラス全てに追加した**。これは`keep_alive()`を撤回した後も独立した価値がある: 上記の名前付きコピーキャプチャのイディオム自体(`Type(handle)`による再構築)がこれまで9クラスでしか使えなかったのを、全クラスで使えるようにしたため。コメントも実装した`share<T>()`という(実在しない)表現から、実態に合わせて更新した。
   - **副作用として発覚した既存バグ(修正済み、`using Widget::Widget;`を残したため影響は継続する)**: `using Widget::Widget;`を`Menu`に追加した結果、`custom.cpp`(実装は`cpp_tk.cpp`内)の`OptionMenu`コンストラクタにあった`menu_ = Menu(*this, {});`という呼び出しが、`Menu::Menu(const Widget&, const map&)`と継承された`Widget::Widget(const Widget&, const string& type, ...)`の間で**曖昧**になりコンパイルエラーになった(空の`{}`が`std::string`型の`type`引数にも`std::map`型の`options`引数にも解釈できてしまうため)。`Menu(*this)`(optionsを省略しデフォルト引数に任せる)に修正して解消した。全コードベースを検索した結果、この`(parent, {})`という書き方をしていた箇所はここ1箇所のみだった。**今後の注意点として**、`using Widget::Widget;`を持つクラスに対して`SomeWidget(parent, {})`のように空の`{}`を明示的に渡すコードを新たに書くと同様に曖昧になり得るため、省略するか具体的な値を持つmapを渡すこと。
   - `keep_alive()`本体・`test/test_keep_alive.cpp`は削除済み。名前付きコピーキャプチャのイディオムをREADME等に明文化するタスクは`docs/todo.md`のB節タスクと合流させた。

   **2026-07-05追記(sanitizer)**: `CMakeLists.txt`に`CPP_TK_ENABLE_SANITIZERS`オプション(既定OFF、`-fsanitize=address,undefined -fno-omit-frame-pointer -g`をCXXフラグ・リンカフラグ双方に追加)を導入した。
   - **重要な制約**: このプロジェクトの開発環境であるWindows/MSYS2のmingw-w64 GCCは、sanitizerランタイム(`libasan`/`libubsan`)を同梱していない(pacmanリポジトリを確認したが該当パッケージ自体が存在しない)。clang+`mingw-w64-x86_64-compiler-rt`の組み合わせでは動作する可能性があるが、`compiler-rt`が未インストールであり、今回はローカル環境への新規パッケージインストールを伴うため見送った。そのため**ローカルのWindows開発環境ではこのオプションは現状機能しない**(リンクエラーになる)。
   - 一方、`.github/workflows/ci.yml`には元々検証目的のLinux(Ubuntu)ジョブが存在し、Ubuntu標準の`build-essential`(GCC)は最初からsanitizerランタイムを同梱している。そこで**新規ジョブ`linux-sanitizers`をCIに追加**し、`-DCPP_TK_ENABLE_SANITIZERS=ON`でLinux上のみ有効化した。ローカル環境の制約を受けずに、PRのたびにこの種のダングリングポインタ参照を機械的に検出できる。
   - **副次的に発覚した問題**: `CMakeLists.txt`の`CMAKE_LIBRARY_OUTPUT_DIRECTORY`/`CMAKE_ARCHIVE_OUTPUT_DIRECTORY`/`CMAKE_RUNTIME_OUTPUT_DIRECTORY`が全て`${PROJECT_SOURCE_DIR}`直下の固定パスになっているため、`build`と`build-asan`のような複数のビルドディレクトリを同一ソースツリーに対して併存させると、後からビルドした方が`lib/libcpp_tk.a`等の成果物を無条件に上書きし、互いを汚染する(sanitizer有効/無効を混在させるとリンクエラーになる)。CI環境ではチェックアウトのたびにクリーンな状態から始まるため実害は無いが、ローカルで複数ビルド構成を試す場合は要注意(この問題自体は今回の変更が原因ではなく既存の設計)。

---

## 次の一歩（未決定・要相談）

- **C-8.（ユーザコールバックのhandle()規約破りによる検知不能なSegmentation Fault）は根絶不可能な問題と判断し、2026-07-05時点で以下まで対応済み**: CIのsanitizerジョブ追加、`using Widget::Widget;`の全38クラス展開、README.mdの「制限事項」節での明文化(Before/Afterのコード例付き)。`enable_shared_from_this`/Python参照セマンティクス/コールバックシグネチャ変更は、いずれもC++11の制約下では根本解決にならない、または本家tkinterとの乖離が大きいと判断し却下済み。これ以上の踏み込んだ対策は無いという結論で線引きした。
- Aは2026-07-05時点で対応完了。
- Cは1〜7全て2026-07-05に対応完了(1・2はcpp_tk::Error/ErrorPolicy導入とコールバック境界の防御で対応)。8は上記の通り軽減策止まりで対応完了とした。
- Bは2026-07-05にREADME.mdへの明文化まで完了。

---

## D. 付随対応・発見事項

### Tclリスト結果のトークン化をTcl_SplitListに統一（2026-07-05対応済み）
`winfo children`や`curselection`等、Tclがスペース区切りのリスト形式で返す結果を、従来は`std::istringstream`による単純な空白splitでパースしていた（要素がbrace/quoteで囲まれるケースを正しく扱えない暫定処理だった）。`Interpreter`に`Tcl_SplitList`ベースの`split_list()`を追加し、該当する以下17箇所を置き換えて統一した。
- 結果パース系: `Widget::winfo_children`、`Canvas::bbox`/`find_overlapping`/`find_closest`/`gettags`、`Listbox::curselection`、`ttk::Style::theme_names`、`ttk::Treeview::selection`/`get_children`/`bbox`
- 呼び出し引数の構築系（ユーザ入力文字列をTclコマンドの単語列に展開する箇所）: `Canvas::xview`/`yview`、`Scrollbar::set`（classic/ttk両方）、`Text::yview`、`ttk::Treeview::xview`/`yview`
- 未使用になった`<sstream>`のincludeを削除。

### 発見した既存バグ（2026-07-05対応済み）
- **Treeview::bbox**: `column`引数が空文字列（デフォルト値）のとき、`{full_name, "bbox", iid, column}`のように空文字列をそのままTclコマンドの引数として渡してしまい、`Invalid column index`エラーになる（動作確認用スモークテストで実際に踏んだ）。`column`が空なら引数自体を省略する（`{full_name, "bbox", iid}`のみ呼ぶ）よう修正し、失敗時に例外を投げず空配列を返す`bool* success`パターンに統一した。[test/test_structural_fixes.cpp](../test/test_structural_fixes.cpp)で回帰テスト化。

---

## E. スレッド安全性

### 前提
Tclの`Tcl_Interp*`は生成したスレッド以外から触ってはいけない制約を持つ（スレッド間で共有するAPIではなく、スレッド間通信には`Tcl_ThreadQueueEvent`等Tcl自身が提供する専用機構を使うのが流儀）。`interp_map`（[cpp_tk.cpp:94](../cpp_tk.cpp#L94)、`std::thread::id -> Interpreter*`）は「1スレッド1Interpreter」を前提にした設計だが、これはあくまで`Tk`生成時に自スレッドのIDで登録するだけで、**別スレッドから既存のWidget/PhotoImage/Font/Style/Varを誤って呼び出すことを妨げていなかった**。`filedialog`/`messagebox`等の自由関数は呼び出しの都度`interp_map`を引き直すため見知らぬスレッドからは偶然nullptrになり無害化されるが、`Widget`等は生成時に取得した`Interpreter*`を`impl_`/`interp_`に保持するだけなので、そのハンドルを別スレッドで使うと検知されずに`Tcl_EvalObjv`が誤ったスレッドから呼ばれてしまう。

本家Python tkinterもこの問題自体は「解決」しておらず、ウィジェットへのアクセスは常にメインスレッドから行うことという運用ルールを敷き、実現手段(`root.after(0, callback)`や`queue.Queue`によるワーカースレッドとの受け渡し)はユーザ側の作法に委ねている。QThread/wxThreadのような専用スレッドラッパークラスはTkinterには無い。

### 対応方針（3段階に整理し、段階1・2を2026-07-05に対応）
1. **クロススレッドアクセスの検知（対応済み）**: `Interpreter`に生成時のスレッドID(`owner_thread_`)を持たせ、[InterpreterClient::checked_interp](../cpp_tk.cpp#L593-L614)で`std::this_thread::get_id()`と比較する。不一致なら`ErrorPolicy::LENIENT_THREAD`に従って`cpp_tk::Error`を送出するか、ログのみで継続する(この検知自体は`Tcl_Interp`に一切触れる前に働くため、どちらを選んでも危険なTcl呼び出しはしない。詳細はC-1/2節の「2026-07-05追記」を参照)。一時サンプルで、`DEFAULT`/`LENIENT_CALL`のみ/`LENIENT_THREAD`のみ/両方の4パターンが独立して期待通り動作し、`LENIENT_THREAD`時は別スレッドからの呼び出しが例外を投げずメインスレッドに戻れば引き続き正常に使え続けることを確認済み。

2. **安全なスレッド間受け渡し(`InterpreterClient::post()`、対応済み)**: Tcl自身が提供するスレッドセーフなAPI`Tcl_ThreadQueueEvent`+`Tcl_ThreadAlert`(Tcl自身の`Thread`拡張が内部で使うのと同じ機構)を使い、`widget.post(std::function<void()> job)`(全`InterpreterClient`派生クラスが持つ)でワーカースレッドからUIスレッドへ安全に処理を依頼できるようにした。本家の`.after(0, callback)`+`queue.Queue`パターンに相当する。
   - `Interpreter`に生成時の`Tcl_ThreadId`(`owner_tcl_thread_`、`Tcl_GetCurrentThread()`で取得。段階1の`owner_thread_`(`std::thread::id`、比較用)とは別物で、`Tcl_ThreadQueueEvent`が要求する型が異なるため両方保持している)を追加。
   - `post()`はあえて`checked_interp()`を通さない(段階1のスレッド一致チェックは「同一スレッドからのみ呼んでよい」という契約であり、post()は逆に「異なるスレッドから呼ばれること」を前提とする真逆の契約のため)。`interp()`が指すInterpreter自体は構築後不変な値のみ読むので、クロススレッドで読んでもデータ競合はない。
   - 実処理は`Interpreter::post()`が担い、`Tcl_Event`を先頭に持つ独自イベント構造体(`PostedJobEvent`)を`Tcl_Alloc`で確保して`std::function<void()>`へのポインタを持たせ、`Tcl_ThreadQueueEvent`で対象スレッドのイベントキューへ注入、`Tcl_ThreadAlert`で(`vwait`等でブロック中の)対象スレッドを起こす。ジョブの実行自体もTclのCコールスタックから直接呼ばれる境界のため、他のコールバックと同様`invoke_guarded`経由で例外を握りつぶす。
   - 一時サンプルで、ワーカースレッドから`label.post(...)`を呼び、`root.mainloop()`(`vwait forever`)がブロック中でも正しく起こされてジョブが実行され、UIが安全に更新されて`quit()`でmainloopが終了することを確認済み。

3. **QThread/wxThread的な専用スレッドラッパークラス（非推奨・対象外）**: Qt/wxWidgetsのスレッドラッパーはそれぞれのイベントループ・メタオブジェクトシステムと密結合した大規模な基盤であり、本家Python tkinterがこの種のクラスを持たないこと、cpp_tkが「Tcl/Tkの薄いラッパー」であることを踏まえ、対応しない方針。

---

## F. テスト基盤（2026-07-05導入）

### 経緯
これまでの動作確認は全て`example/CMakeLists.txt`に一時的にターゲットを追加し、確認後に削除する使い捨てのスモークテストで行っていた。「本番投入できるライブラリ」を名乗るには、リポジトリに残る自動テストとCIが最低限必須という認識のもと、テスト基盤を導入した。

### フレームワーク選定
[doctest](https://github.com/doctest/doctest)（単一ヘッダ、MITライセンス）を`test/doctest.h`にベンダリング(バージョン2.5.0、`test/LICENSE-doctest.txt`にライセンス文を同梱)。CTest(CMakeのテスト実行・集計機構、フレームワークではない)と組み合わせて使う。GoogleTestも検討したが、(a) cpp_tk自体が外部依存を増やさない薄いラッパー方針であること、(b) モックすべき対象が乏しく実際のTcl/Tkに対してテストするのが自然であること、(c) ビルドを軽く保ちたいこと、からdoctestを採用した。

### 構成
- `test/doctest.h` — ベンダリング済み単一ヘッダ
- `test/CMakeLists.txt` — テストファイルごとに個別の実行ファイル+CTestエントリを生成（1プロセス内で複数の`Tk_Init()`を扱う設計は未検証だったため、ファイル単位でプロセスを分離する方式にした。実際には同一プロセス内で複数の`tk::Tk`を直列生成しても問題なく動作することを`test_error_policy.cpp`/`test_interpreter_client.cpp`で確認済みだが、プロセス分離自体はCI上での障害の切り分けやすさの観点からも維持している）
- `test_error_policy.cpp` — `ErrorPolicy`(`DEFAULT`/`LENIENT_CALL`/`LENIENT_THREAD`)の全組み合わせ
- `test_interpreter_client.cpp` — `call()`エスケープハッチ(5クラス共通)、未初期化アクセス検知、`ArgValue(Var&)`、`Entry`/`Combobox`の`text_var_`除去後の回帰
- `test_thread_safety.cpp` — クロススレッド検知・`post()`(単一/複数ワーカースレッド)

`cmake --build build && ctest --test-dir build --output-on-failure`で実行する。

### CI（2026-07-05導入、GitHub Actions）
[.github/workflows/ci.yml](../.github/workflows/ci.yml)にWindows/Linux/macOSの3ジョブを用意した。
- **windows**: `msys2/setup-msys2`でこの開発環境と同じMSYS2 mingw64一式(`mingw-w64-x86_64-toolchain`/`cmake`/`ninja`/`tcl`/`tk`)を導入し、`shell: msys2 {0}`でconfigure/build/ctestを実行する。**確認済み(グリーン)**。
- **linux**: apt経由で`tcl-dev`/`tk-dev`を導入。TkはX前提のためウィンドウ生成(`withdraw()`するものも含む)に仮想ディスプレイが要り、`xvfb-run`でctestをラップしている。**確認済み(グリーン)**。
- **macos**: Apple標準のTcl/Tkは既知の問題があるため使わず、Homebrewの`tcl-tk`(keg-only)を`CMAKE_PREFIX_PATH`経由で検出させている。**失敗を確認済み。手元にMac実機が無いため、デバッグはユーザ判断でひとまずスコープ外とする決定済み**（再検証が必要になれば[todo.md](todo.md)へ再度起票する）。

### 副産物として発見した問題（対応済み）
テスト実装中に、**`ErrorPolicy::STRICT`という列挙子名が`<windows.h>`の`STRICT`マクロ(古いWin32の型チェック用マクロ)と衝突してコンパイルできない**ことが判明した(doctestが内部で`<windows.h>`をincludeするため顕在化した)。これはテストに限らず、Windows環境で`<windows.h>`を先にincludeする一般的な利用コードでも同様に壊れる実害があったため、ライブラリ本体側で`ErrorPolicy::STRICT`を`ErrorPolicy::DEFAULT`に改名した(意味は変わらず、既定値=全カテゴリでError送出)。

### 未対応・今後のスコープ（初期スコープ合意により後回し）
- **A節（OptionMenu/LabelFrame/Menu/Listbox/Textの追加メソッド等）のテスト**: 記載当時は未着手だったが、その後の第2〜5回棚卸しで`test_a_features.cpp`/`test_a_features_round2.cpp`/`test_new_classes.cpp`/`test_native_misc.cpp`/`test_round5_*.cpp`を通じて大部分が回帰テスト化された。`filedialog`/`messagebox`は実際にOSのダイアログを表示し操作をブロックするため、引き続き自動テストでは呼び出しを避け手動確認に留めている。
- **Linux/macOS上でのテスト実行確認**: 本節記載当時は未検証だったが、その後Linux(Ubuntu)はCIでグリーンを確認済み。macOSは上記CI節の通りスコープ外とする決定済み。

### `Var&`/`StringVar&`が右辺値を拒否すること(C-3)のコンパイル失敗テスト（2026-07-05対応済み）
通常の実行時テスト(doctest)の枠組みでは「コンパイルが失敗すること」自体を検証できないため、[test/compile_check/](../test/compile_check/)という専用ディレクトリを新設した。命名は当初案の`compile_fail`から`compile_check`に変更した（「失敗する」テストしか無いのは対称性を欠き、「意図通りコンパイルが通ること」を示す対照群も本質的に必要だと判断したため。対照群が無いと、右辺値拒否テストの失敗が本当に右辺値が理由なのか、単なるタイポ等の別要因なのかを切り分けられない）。

- **`should_compile/rvalue_reject_control.cpp`**: `Checkbutton::variable`/`Radiobutton::variable`/`Entry::textvariable`(classic/ttk)/`Combobox::textvariable`/`Spinbox::textvariable`(classic/ttk)/`ArgValue(Var&)`(`Menu::add_checkbutton`経由)の8箇所全てを、名前付きlvalueで正しく呼び出す対照群。通常の`OBJECT`ライブラリとして`cmake --build`に含めるだけでよい(コンパイルが失敗すればビルド自体が落ちるため、専用のCTestエントリは不要)。
- **`should_fail/*.cpp`**: 上記8箇所それぞれに一時オブジェクト(右辺値)を渡す誤ったコードを**1ファイル1API**で用意した(`checkbutton_variable.cpp`/`radiobutton_variable.cpp`/`entry_textvariable_classic.cpp`/`entry_textvariable_ttk.cpp`/`combobox_textvariable.cpp`/`spinbox_textvariable_classic.cpp`/`spinbox_textvariable_ttk.cpp`/`argvalue_var_ctor.cpp`)。1ファイルに複数の失敗ケースをまとめると、将来どれか1つだけが誤って通るようになる回帰があってもファイル全体としては失敗し続けるため検知できない、という理由で分割した。
- **CMake側の仕組み**: `test/compile_check/CMakeLists.txt`で`should_fail/*.cpp`を`file(GLOB ...)`で収集し、各ファイルにつき`add_test`で`${CMAKE_CXX_COMPILER} -std=c++11 -I<repo root> -c <src> -o <obj>`を直接実行するテストを登録、`set_tests_properties(... WILL_FAIL TRUE)`で「失敗すれば成功」と判定させている。`cpp_tk.hpp`はTcl/Tkに依存しないpimplヘッダ(標準ライブラリのみinclude)のため、`-c`(コンパイルのみ、リンクしない)で完結し、cpp_tkライブラリ本体やTcl/Tkへのリンクは一切不要。
- **メカニズムの動作確認**: `should_fail`の1ファイルを一時的に「正しい呼び出し」に書き換えてビルド・実行し、`ctest`が`Failed`と報告することを確認した上で元に戻した(`WILL_FAIL`が実際にコンパイル成否を弁別できていることの裏付け)。
- 全21件(既存13件＋`compile_fail_*` 8件)が`ctest`でグリーン。

---

## G. 拡張ライブラリの拡充（ユーザ獲得のための新規開発、2026-07-05方針決定）

### 経緯
README.mdの刷新に続き、ユーザ獲得のためには拡張ライブラリ（Python tkinterエコシステムにおける`tkcalendar`/`tkinterdnd2`相当）とサンプルコードの拡充が必要という認識で一致した。どのカテゴリから着手すべきかを議論した。

### 分類
1. **既存のTk/ttkプリミティブだけで組み上げる複合Widget**（`tkcalendar`相当）— 新規のTcl/Tk依存もプラットフォーム分岐も不要。
2. **別のTclパッケージをラップするもの**（`tkinterdnd2`相当、例: `tkdnd`）— `package require`で外部パッケージに依存する。
3. **Tcl/Tkを介さずOSネイティブAPIを直接叩くもの**（システムトレイ、通知等）— Tcl/Tkの制約そのものを飛び越える。

### 方針決定
- **1から着手する。** 理由: (a) 完全新規実装のためライセンス上の懸念が実質無い、(b) `docs/todo.md`で既に確立済みの`cpp_tk::custom`名前空間の運用方針（新規の合成ウィジェットが必要になった時点で追加する）にそのまま乗る、(c) 既存のビルド設定・CIの変更が不要、(d) README・スクリーンショットで見栄えがしユーザ獲得への費用対効果が高い。候補として**Tooltip**（実装コスト低・需要高）と**Calendar/DatePicker**（`tkcalendar`の象徴的な代替）を挙げた。
- **2は需要が確認できてから着手する。** 着手する場合もバイナリは同梱せず`package require`のみ行い、「別途インストールしてください」と案内する軽量な方式を推奨(バイナリ同梱は各パッケージのライセンス条項確認・プラットフォームごとのvendoring作業を伴い費用対効果が悪化するため)。
- **3は現時点では推奨しない。** 「薄いラッパー」というアイデンティティから外れる上、macOSの動作検証すら済んでいない現状で3プラットフォーム分のネイティブコードを抱えるのは時期尚早。

### ライセンス上の注意点（1のカテゴリでも）
外部コードの再配布は発生しないが、実装時に既存の`tkcalendar`等のソースコードを見て「移植」することは避ける。挙動・UIコンセプトを参考にするのと、ソースコードを読んで翻訳するのは別物として扱い、実装は必ずcpp_tk自身のAPIから独立して書き起こす。

### カテゴリ1内での代表的ライブラリ調査（2026-07-05実施）
「tkcalendarは代表例として出しただけ」との指摘を受け、同種（カテゴリ1）で需要の高そうな代表的ライブラリをGitHubスター数等で調査した。

| ライブラリ | 内容 | スター数 | 実装方式 |
|---|---|---|---|
| CustomTkinter | 全Widgetを独自にCanvas等で再描画しモダンなフラットUIに差し替える | 13.2k | Python、新規Tclパッケージ不要だが実装範囲が非常に大きい |
| ttkbootstrap | Bootstrap風の20以上のモダンテーマ＋Meter/DateEntry/Floodgauge等のボーナスWidget | 2.6k | 同上 |
| sv-ttk (Sun Valley theme) | モダンなttkテーマ1つのみ | 2,180 | **Tcl 87.9%+Pythonの薄いローダーのみ**(MIT)。新規Widgetクラスの追加無しに標準の`ttk::style`機構だけで動く |
| tksheet | 表形式データ専業のCanvasベースGrid Widget(セル編集・行列DnD・セル埋め込みWidget・仮想レンダリング) | 501 | 純Python(Canvas、可視範囲のみ再描画)、MIT、新規Tclパッケージ不要 |
| ttkwidgets | Tooltip風・オートコンプリートCombobox・CheckboxTreeview・Calendar等の詰め合わせ | 151 | 同上 |
| tkcalendar | Calendar/DateEntry専業 | 105 | 同上 |
| tkinter-tooltip等 | Tooltip専業だが競合パッケージが複数乱立、単一の支配的パッケージが無い | 小規模が多数 | 実装が単純すぎて共有依存にする動機が薄いことの裏返し |

**発見**: 最もスターを集めているのは複合Widgetではなく「モダンなttkテーマ」だった。`sv-ttk`は新規Widgetクラスの追加が一切無く、実装のほぼ全てがTclスクリプト・画像アセットであり、複合Widget(Calendar等)よりもさらに実装コストが低い。「見た目が古臭い」がtkinter系ユーザーの最大の不満点であることが読み取れる。また`tksheet`(501)は`tkcalendar`(105)の約5倍のスターを集めており、`ttk::Treeview`が階層リスト＋カラム表示に留まりセル編集・行列DnD・セル埋め込みWidget等の「スプレッドシート的操作」をカバーしていないという、実在するギャップを裏付けている。ただし実装難易度は調査した候補の中で最も高い(仮想レンダリング・セル編集オーバーレイ・行列リサイズ・複数選択・ソート・検索・DnD並べ替え等、CustomTkinter級の規模)。

### 優先順位の最終決定（2026-07-05）
実績を小さいものから積む方針とし、以下の順で着手する。
1. **モダンなttkテーマ**（最安・最高ROI）
2. **Calendar/DateEntry**（需要実証済み・実装規模は中程度）
3. **tksheet相当の表Widget**（需要は最も高いが実装規模も最大。いきなり全機能を狙わず、固定データセット・セル直接編集のみ・DnD/セル埋め込みWidgetは後回しというMVPスコープから始める）
4. **Tooltip**（優先度は低いが着手コストが低いためいずれ追加）

テーマについては、`sv-ttk`はMITライセンスなのでattribution付きでそのまま同梱することも法的には可能だが、「移植ではなく独立実装」という既存方針に照らして、オリジナルデザインで新規に起こすか既存MITテーマを明示的なクレジット付きで採用するかは未決定だった。

### テーマ採用方針の最終決定（2026-07-05）
「Tk/Tcl/Tkinterへの最大限のリスペクトを持ちながら、その利便性をC++に持ち込む」という本ライブラリの目的に照らし、`sv-ttk`をMITライセンスに基づき明示的なクレジット付きで採用する方針で決定した。

- **MITライセンス上の再配布義務**: 「著作権表示と許諾表示を、ソフトウェアの全ての複製または重要な部分に含めること」の1点のみ。`sv-ttk`のLICENSE(著作権者: rdbende、標準的なMIT文言)を確認済み。cpp_tk自体もMITのため、組み合わせに互換性上の懸念は無い。
- **前例に倣う**: `test/doctest.h`(doctestライブラリ、MIT)を`test/LICENSE-doctest.txt`にライセンス文を同梱する形でベンダリング済みの前例(F節参照)と同じ形式を踏襲する。
- **構成案**:
  ```
  thirdparty/sv_ttk/
  ├── sv.tcl              # sv-ttk本体のTclスクリプト(そのまま同梱)
  ├── theme/              # 付随する画像アセット(そのまま同梱)
  └── LICENSE-sv_ttk.txt  # sv-ttk本来のMITライセンス文(著作権者rdbendeの表示を保持)
  ```
- README.mdに「謝辞 / Acknowledgements」節を新設し、Sun Valley ttk theme by rdbende (MIT License)を同梱・利用している旨を明示する。
- C++側のローダー(テーマを読み込んでttk styleを切り替える薄い関数)はcpp_tk自身のオリジナル実装とする。

### 実装完了（2026-07-05）
`thirdparty/sv_ttk/`に実ファイル(`sv.tcl`/`theme/{dark,light}.tcl`/`theme/sprites_{dark,light}.tcl`/`theme/spritesheet_{dark,light}.png`/`LICENSE-sv_ttk.txt`)をベンダリングし、そこから生成した埋め込み用C++ソース`thirdparty/sv_ttk/sv_ttk_data.cpp`(`sv_ttk_data.hpp`が宣言する`EmbeddedFile`テーブル)を作成した。

- **埋め込み形式**: ビルド時コード生成ではなく、ベンダリング時に一度だけ手元で生成した静的なC++ソースとして書き出す方式にした(`test/doctest.h`のベンダリング方式と同じ扱い)。CMakeでの新規コード生成ステップは不要で、`sv_ttk_data.cpp`を`add_library`のソース一覧に加えるだけで済む。テキストファイル(5個のTclスクリプト)は生の内容をそのままC++の生文字列リテラル(`R"DELIM(...)DELIM"`)として埋め込み、バイナリファイル(2個のspritesheet PNG)はbase64エンコードした上で生文字列リテラルとして埋め込んだ。埋め込んだ内容が元ファイルとバイト単位で一致すること(PNGはbase64往復、TclスクリプトはR"()"の前後改行を除き完全一致)を確認済み。
- **実行時の展開**: `cpp_tk::custom::use_sv_ttk_theme(bool dark = true)`(`custom.hpp`/`custom.cpp`)を新設した。初回呼び出し時のみ、環境変数(`TEMP`/`TMP`/`TMPDIR`)から解決したOS一時ディレクトリ配下に、埋め込んだ7ファイルを元と同じ相対ディレクトリ構造で書き出し(`sv.tcl`内部の`[info script]`相対source/ファイル参照をそのまま機能させるため)、`sv.tcl`を`source`する。埋め込みアセットの中身(Tclスクリプト本文・base64データ)は、専用のTclプロシージャ(`cpp_tk_sv_ttk_write`、cpp_tk自身が書いた固定文字列)への引数として渡すことで、文字列展開(`eval`によるインジェクション)を一切経由せずに安全に書き出している。2回目以降の呼び出しはTclグローバル変数`::cpp_tk_sv_ttk_loaded`で再展開をスキップする。
- **発覚した実装上の罠**: `ttk::style theme use`(内部で`ttk::setTheme`が呼ぶ)は`<<ThemeChanged>>`を自動発火しない。sv.tcl側は配色適用(`configure_colors`)をこのイベントのバインドで行う設計のため、明示的に発火させる必要があった。当初`event generate . <<ThemeChanged>>`のみで済むと想定したが、**初回呼び出し直後に限り反映されない**現象が発生した(要因未特定。2回目以降の呼び出しでは問題なく機能する)。最終的に、確実性を優先して`configure_colors`プロシージャを直接呼び出す方式にし、`event generate`は既存のEntry/Combobox/Spinbox/Menu等(configure_colors以外の`<<ThemeChanged>>`バインド先)への反映のためあわせて発火する形にした。
- **テスト**: `test/test_sv_ttk_theme.cpp`を新規追加。dark/light切り替えが例外を投げず`ttk::style theme use`が正しい名前を返すこと、`ttk::style configure .`の設定内容(背景色等)がdark/lightで実際に異なること、2回目以降の呼び出しでも正しく動作することを確認済み(`ttk::style configure <style> -singleoption`という単一オプションクエリ形式は値を返さない仕様のため、オプション一覧全体を取得して検証する必要があった)。
- **副次的なコンパイルエラー修正**: Tclの生文字列リテラルの区切り識別子はC++11規格上16文字以内に制限されており、当初使った`CPPTK_SVTTK_LIGHT`等一部が17文字を超えコンパイルエラーになったため、`SVSV`/`SVDARK`/`SVLIGHT`等の短い識別子に変更した。
- README.mdに「🙏 謝辞」節を新設し、Sun Valley ttk theme by rdbende (MIT License)を明示。使用例も追加した。

### 2026-07-06追記
- **フォルダ名を`third_party/`→`thirdparty/`に変更**（ユーザの他プロジェクトでの命名との一貫性のため）。`CMakeLists.txt`/`custom.cpp`のinclude/`README.md`/本ファイル内の参照を全て追随済み。
- **`example/theme/sv_ttk_demo.cpp`を新規追加**。Sun Valley ttk themeの見た目を実際に触って確認できるデモで、Entry/Combobox/Checkbutton/Radiobutton/Progressbar/Scale/Accentボタンを配置し、「Toggle theme」ボタンでdark/lightをその場で切り替えられる。`example/theme/CMakeLists.txt`を新設し(既存の`example/`直下のforeachパターンとは別に、テーマ系デモをまとめる専用サブディレクトリとした。将来ttkbootstrap等を追加する余地を見込んでいる)、`example/CMakeLists.txt`から`add_subdirectory(theme)`で取り込む形にした。
  - 実装中に発覚した罠: `ttk::Frame radio_row(container);`のように**親と全く同じ型を1引数で渡すと、Widgetのコピー禁止(B節)によりコピーコンストラクタとの間で曖昧になりコンパイルエラーになる**(コピーコンストラクタは`delete`されているが、直接的な型一致がベストマッチとして選ばれてしまい、その上で削除済み関数の呼び出しとしてエラーになる)。`static_cast<const Widget&>(container)`で明示的にアップキャストして解消した。同じ型のWidgetを親として直接渡す際の一般的な注意点として認識しておく。
  - バックグラウンド起動して一定時間生存することを確認し(即座にクラッシュしないことの確認)、プロセスを正常終了させて動作確認済み。実際の見た目(GUI画面)はユーザ側で目視確認する。

### ttkbootstrap導入の検討結果（2026-07-06、見送り）
「ユーザ獲得のため他のモダンテーマも試したい」という要望を受け`ttkbootstrap`(2.6kスター)を調査したが、**sv-ttkとは実装の性質が根本的に異なる**ことが判明し見送った。
- sv-ttkは静的なTclスクリプト+画像アセット(Tcl 87.9%+薄いPythonローダー)であり、そのまま埋め込むだけで済んだ。
- 一方ttkbootstrapは`StylerTTK`というPythonクラスが**Pillow(PIL)を使って実行時に角丸ボタン等の画像を動的生成**する仕組みで、ベースの`clam`テーマの上に約20種類のカラーパレットごとに都度画像を描画してスタイルを構築している。cpp_tk(C++)側にはPillow相当のラスタ描画ライブラリが無いため、単純な埋め込みでは対応できない。
- 検討した対処候補: (1)`StylerTTK`のロジックをC++で完全に再実装する(角丸矩形のアンチエイリアス描画等が必要でsv-ttkの比ではない規模になる)、(2)実際にPython+ttkbootstrapを動かして生成物を1回だけ抽出し静的アセットとして埋め込む(sv-ttkと同様の方式に落とし込めるが、抽出パイプラインの新規実装が要る上、実行時の自由なカラーパレット変更はできず固定テーマ何種類かを選ぶ形になる)、(3)ttkbootstrapは見送り、同種の静的アセット構成の別テーマ(`rdbende`による`Azure-ttk-theme`/`Forest-ttk-theme`等)を検討する。
- **最終的にユーザの意図は「別のテーマ(見た目)」ではなく「別の課題(topic)」に取り組むことだったと判明**したため、ttkbootstrap自体は一旦保留とし、優先順位2の`Calendar`/`DateEntry`に進むことで合意した。ttkbootstrap自体への関心が再燃した場合は、上記(2)の抽出方式から検討する。

### `Calendar`実装完了（2026-07-06、優先順位2の前半）
`cpp_tk::custom::Calendar`(`custom.hpp`/`custom.cpp`)として実装した。事前の方針決定:
- **日付の表現**: `std::tm`や文字列は使わず、year/month/dayをint 3つで表現する(外部日付ライブラリ・cpp_tk全体の依存最小方針と一致)。
- **v1スコープ**: 月移動・日付選択・`get_date()`のみの最小構成。`mindate`/`maxdate`制限・イベント表示・年選択ドロップダウンは今後の課題としてスコープ外にした(tksheetのMVP方針と同じ考え方)。
- **日数/曜日計算**: 自前でうるう年等のカレンダー演算を書かず、`<ctime>`の`std::mktime()`に正規化させることで標準ライブラリへ委譲した(`day=0`を渡すと前月の最終日に正規化される性質を利用して月の日数を求める等)。

**実装中に発覚した設計上の要点**: 日付セルのクリック・前月/次月ボタンのコールバックは、Calendar自身の可変状態(表示中の年月・選択日・コールバック本体)を安全に参照する必要がある。当初「`handle()`経由でCalendar自身を再構築する」案を検討したが、**`handle()`が保持するのはWidget基底の状態(interp/full_name)のみで、Calendar固有のメンバ(選択日等)は復元されない**ことに気づいた(ScrolledText等の既存の合成Widgetはこの問題に遭遇していなかった。それらのコールバックは常に「Tcl側の状態だけで完結する操作」だったため)。この気づきを踏まえ、Calendar固有の可変状態は`struct Model`(選択日・表示月・グリッド用Frame・コールバック本体等をまとめる)に集約し、`std::shared_ptr<Model>`としてCalendar自身が1つだけ保持する設計にした。コールバックは(Calendarではなく)この`shared_ptr<Model>`をコピーキャプチャし、内部ヘルパー関数(`rebuild_calendar_grid`等、custom.cpp内の無名namespace)へ渡す形にした。`Model`はcustom.cpp内のヘルパー関数から名前を参照する必要があるため、`Calendar`クラス定義内でpublicな(ただし直接使用しない旨をコメントした)ネスト型として宣言した。

- `test/test_calendar.cpp`を新規追加(4件): 明示的な日付での初期化、`set_date()`、実際に日付セルをクリックしてのコールバック発火(`event_generate`+`nametowidget`で内部の日付グリッドの子を辿って検証)、引数省略時の本日日付。クリックテストの実装中に、C節8./H節周辺で既に確認済みの「先行するTEST_CASEがwithdraw()した状態を引き継ぐとevent_generateが配送されない」問題に再度遭遇し、同じ`withdraw();deiconify();`の順で確実に状態変化を起こす対処を適用した。
- `example/calendar_demo.cpp`を新規追加(選択した日付をLabelに表示するデモ)。`example/CMakeLists.txt`に追加。
- README.mdの特徴・使用例・サンプル実行に追記した。

---

## H. Tcl/Tk本体のランタイムスクリプト一式が実行環境に無いと起動できない問題（2026-07-05起票、未着手）

### 経緯
sv-ttkのTclアセット埋め込み方式（G節）を検討する過程で、「同じ問題はcpp_tkが依存するTcl/Tk本体のランタイムスクリプト一式（`init.tcl`/`tk.tcl`/`ttk/*.tcl`等）にも既に存在するのではないか」という指摘があった。調査の結果、これは事実であり、かつG節のsv-ttkとは**規模・性質が異なる別の問題**と判断したため、切り分けて起票する。

### 問題の内容
`Tcl_Init()`/`Tk_Init()`は、コンパイル済みの`libtcl`/`libtk`本体だけでなく、`$tcl_library`/`$tk_library`（通常は環境変数または実行ファイルからの相対パスで解決される）から`init.tcl`を筆頭とするTclスクリプト形式のランタイムライブラリ一式（`tk.tcl`、`ttk/*.tcl`のテーマエンジン、`msgs/*.msg`のローカライズカタログ、`encoding/*.enc`のエンコーディングテーブル等、合計150〜250ファイル規模）を`source`する。これが見つからない環境（Tcl/Tkが未インストールの一般ユーザ環境等）では`Tcl_Init()`自体が失敗し、cpp_tkベースのアプリが起動できない。現状の[cmake/FindTCL.cmake](../cmake/FindTCL.cmake)/[FindTk.cmake](../cmake/FindTk.cmake)はコンパイル済みライブラリとヘッダしか探しておらず、このスクリプト一式の所在は完全にシステムの実行時環境任せになっている。

### G節(sv-ttk)と同じ解法(C++バイト配列への埋め込み)を単純に適用できない理由
- **規模が2桁違う**: sv-ttkは4ファイル程度だったが、Tcl/Tk本体のスクリプト一式は150〜250ファイル・数MB規模。埋め込むとlibcpp_tk.aが大幅に肥大化する。
- **バージョン同期の制約**: このスクリプト一式は、リンクする`libtcl`/`libtk`のバージョンと完全に一致している必要がある。埋め込んだスクリプトと実行環境のTcl/Tkバージョンがズレると、内部的な互換性チェックで壊れる可能性がある。安全にやるには「cpp_tk自身がTcl/Tk本体を静的リンクし完全に自前で抱える」という、現状の「システムのTcl/Tkをfind_packageで見つけて使う」という軽量な設計方針からの大きな転換を伴う。
- **ライセンスが別物**: Tcl/Tk自体はMITではなく**Tcl/Tkライセンス**(BSD系の別の許諾文言)。再配布自体は同様に寛容で問題ないが、sv-ttkとは別に、Tcl/Tk自身の著作権表示を保持する必要がある(障害にはならない)。

### 既に業界標準の解法が存在する
この課題はPyInstaller/cx_Freeze等がPython製Tkinterアプリを配布する際に長年向き合ってきた既知の問題で、標準的な解法は「C++バイナリに埋め込む」のではなく、**Tcl/Tkのスクリプトディレクトリ一式を実行ファイルの隣にファイルとしてそのままコピーして配布する**（DLLを配布するのと同じ発想）というもの。埋め込みが有効なのは「数個の小さな自作/採用アセット」（G節のsv-ttk等）の場合であり、「上流の巨大なランタイム一式」に対しては業界標準に倣う方が合理的と判断した。

### 検討中の方向性（未決定）
`find_package(TCL)`/`find_package(Tk)`が検出したライブラリパスから対応するスクリプトディレクトリ(`$TCL_LIBRARY`/`$TK_LIBRARY`相当)も検出し、CMakeに「配布用に実行ファイルの隣へコピーする」パッケージングターゲットを新設する方向で検討中。具体的な設計はG節(sv-ttk埋め込み)の実装完了後に着手する。

---

## I. 親と同じ具象型のWidgetを1引数で渡すと、コピー禁止によりコンパイルエラーになる（2026-07-06起票、未着手）

### 経緯
`example/theme/sv_ttk_demo.cpp`(G節)の実装中に発見した。`ttk::Frame radio_row(container);`のように、**親として渡すWidgetの静的型が構築先と全く同じ具象型**の場合、コンパイルエラーになる。

```cpp
ttk::Frame container(root, {{"padding", 16}});
ttk::Frame radio_row(container); // エラー: use of deleted function 'Frame::Frame(const Frame&)'
```

### 原因
`Widget(const Widget&) = delete;`(B節、コピー時に「親と同じ型を1引数で渡す」呼び出しが子ウィジェット生成と衝突して曖昧になる事故を防ぐための意図的な設計)により、派生クラス(`Frame`等)の暗黙のコピーコンストラクタも連動して暗黙に`delete`される。

`Frame radio_row(container);`のオーバーロード解決では、以下の2つが候補になる:
- `Frame(const Frame&)`(暗黙のコピーコンストラクタ) — 引数`container`(`Frame&`)との束縛に変換が一切不要な**完全一致**。ただし`delete`済み。
- `Frame(const Widget&, options={})` — `Frame&`から`const Widget&`への派生→基底の参照束縛が必要な**変換一致**。

C++の規格上、**「完全一致」は「変換一致」より常に優先され、これは対象の候補が`delete`されているかどうかに関わらない**(削除済み関数が最良候補として選ばれた上で、呼び出し不可としてエラーになる)。そのため、他にどんな候補が存在しても、引数の静的型が構築先の具象型と完全に一致する限り、この問題は回避できない。

### 検討した対処候補（未決定）
- **`static_cast<const Widget&>(parent)`で明示的にアップキャストする**(現状のワークアラウンド、`sv_ttk_demo.cpp`で採用): 引数の静的型が`Widget`になるため、`Frame(const Frame&)`(`Frame&`→`const Frame&`は基底→派生の変換であり暗黙変換不可)が非viableになり解消する。ただし毎回明示的に書く必要があり、気付きにくい落とし穴であることに変わりはない。
- **薄いヘルパー関数を追加する案**: `inline const Widget& as_parent(const Widget& w) { return w; }`のような関数を用意し、`Frame radio_row(as_parent(container));`と書けるようにする。`static_cast`より読みやすく、意図(「親として渡している」)も明確になる。`as_parent()`自体は通常の関数呼び出しのオーバーロード解決(コンストラクタの完全一致優先とは無関係)で解決されるため、戻り値の静的型は常に`const Widget&`になり、問題を構造的に回避できる。
- **完全な自動解決は困難と判断**: C++の言語仕様上、コピーコンストラクタは常に暗黙にオーバーロード候補へ含まれ、`delete`されていても完全一致であれば必ず選ばれるため、他の関数シグネチャの工夫だけでは回避できない。回避するには「コンストラクタへ渡す実引数の静的型を、構築先の具象型と異なるものにする」しかなく、これは(a)ヘルパー関数を挟む(上記案)か、(b)呼び出し規約自体を`parent.handle()`渡しに変える(破壊的変更が大きすぎるため却下)のいずれか。
- Widgetのコピー禁止という設計自体(B節)を見直す案は、この問題の解決のためだけに撤回するには影響範囲(コピー禁止によって守られている不変条件)が大きすぎるため対象外とする。

### 実装完了（2026-07-06）
ユーザと相談の結果、根本原因は「第一引数で渡されるものが親なのかコピー元なのか、シグネチャのみでは判断できない」ことではなく、より正確には「C++がクラス定義のたびにコピーコンストラクタという候補を暗黙に注入し、それが型の完全一致時に(delete済みでも)無条件で最優先されてしまう」ことにあると整理した上で、`as_parent()`ヘルパーの追加とREADME.mdでの明文化を採用する方針で合意した。

- `cpp_tk.hpp`の`Widget`クラス定義直後に`inline const Widget& as_parent(const Widget& parent) { return parent; }`を追加した。普通の関数呼び出しはコンストラクタの「完全一致優先」ルールの影響を受けないため、`as_parent()`を経由すると引数の静的型が常に`const Widget&`になり、暗黙のコピーコンストラクタ候補が発動する条件(型の完全一致)自体を回避できる。
- `example/theme/sv_ttk_demo.cpp`の`static_cast<const Widget&>(...)`を`tk::as_parent(...)`に置き換えた。
- `test/test_widget_basics.cpp`に回帰テストを追加し、親と同じ具象型(`Frame`)を`as_parent()`経由で渡した場合にコンパイル・実行の両方が正しく機能することを確認した。
- README.md「⚠️ 制限事項」節に、原因(コピーコンストラクタの暗黙の完全一致優先)と`as_parent()`による回避方法をコード例付きで追記した。

---

## J. CIがハングして完了しない問題（2026-07-06対応済み）とLinux版Canvas::postscriptのSegFault（起票済み・未着手）

### CIハングの原因と対応
`develop`ブランチ運用への切替後、実際にCIを走らせたところ、Linux版`test_round5_bcd`の"Canvas::postscript"テストケースがSIGSEGVでクラッシュした後、`test_simpledialog_return`付近で**CIジョブ自体が10分以上進捗せず完了しない**という報告があった。

原因は`.github/workflows/ci.yml`の3ジョブ全ての`ctest`呼び出しに**`--timeout`オプションが指定されていなかった**こと。ctestは既定では1テストあたりのタイムアウトが無制限のため、いずれかのテストがハングすると(SIGSEGVによるクラッシュそのものはctestが正しく検知して次のテストに進んでいたが、その後の`test_simpledialog_return`前後で何らかの理由でハングしたとみられる)、ctest全体、ひいてはGitHub Actionsのジョブ自体が(既定の6時間タイムアウトに達するまで)無期限に待ち続けてしまう。

**対応**: 全3ジョブの`ctest`呼び出しに`--timeout 60`を追加し、さらに各ジョブに`timeout-minutes: 15`を設定した(ctest自身のタイムアウト機構をすり抜けるような、Configure/Buildステップでのハング等に対する二重の安全網)。これによりテストがハングしても、CIジョブは(失敗はするが)必ず一定時間内に完了するようになった。

### Linux版Canvas::postscriptのSegFault(未調査)
Linux版`test_round5_bcd.cpp`の"Canvas::postscript: PostScriptデータを文字列で返す"テストケースがSIGSEGVでクラッシュすることが判明した。Windows版では同テストは問題なくパスしている(このセッションを通じてWindows上のctestは繰り返しグリーンを確認済み)ため、**プラットフォーム固有の問題である可能性が高い**(Linux版TkのPostScript出力機構の差異、ヘッドレスXvfb環境特有の問題等が考えられるが未調査)。ユーザの指示により本件は別途調査することとし、今回はCIがこの種の失敗で無期限にハングしないようにする対応のみ行った。
