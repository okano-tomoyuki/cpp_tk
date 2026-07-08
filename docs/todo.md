# cpp_tk 残タスク

最終更新: 2026-07-05

現時点で残っている作業のみを列挙する。経緯・却下した設計案・発見済みバグの詳細な調査記録は [tasks.md](tasks.md) を参照（各項目に該当節を記載）。完了した項目はここには載せず、tasks.mdの方にのみ記録する。

---

## 最重要（要対応・議論継続中）

- [ ] **ユーザコールバックが`handle()`規約を破ると、検知不能なSegmentation Faultになり得る**
  `bind()`/`command()`/`after()`/`trace()`等のコールバックは`Widget::handle()`(`shared_ptr<Impl>`)経由でキャプチャする規約だが、これは人間が守る規約でしかなく型システムで強制されていない。ユーザが`[this]`や`Widget&`を直接キャプチャすると、対象破棄後にコールバック発火時点で解放済みメモリを参照する。ダングリングポインタ参照はC++例外を投げないため既存の`invoke_guarded()`/`ErrorPolicy`では一切検知できず、SIGSEGVで落ちるか静かに誤動作するかのどちらかになる。
  C++の性質上「根絶」はできないため「軽減」を目的にCIにAddressSanitizer/UndefinedBehaviorSanitizer有効ビルドの`linux-sanitizers`ジョブを追加済み(発生時に確実に検知できる。ローカルのWindows/MSYS2 GCCはsanitizerランタイム非同梱のため機能しない点に注意)。
  複数Widgetをまとめてキャプチャする`keep_alive()`ヘルパーも一度試作したが、`std::tuple`索引に名前が付かず可読性を損なうため撤回した(`custom.cpp`にある「1つずつ`handle()`をキャプチャして`Type(handle)`で再構築する」という素直な名前付きコピーキャプチャのイディオムを踏襲する方針に確定)。このイディオムはREADME.mdの「制限事項」節に明文化済み(2026-07-05)。
  完全な根絶策では無いため引き続き要検討。
  詳細: tasks.md C節8.

---

## 進行中（拡張ライブラリの新規開発）

- [ ] **DateEntryを追加する（優先順位2の後半、次に着手）**
  `tkcalendar`(105スター)相当。`Calendar`本体は実装完了(`cpp_tk::custom::Calendar`、`test/test_calendar.cpp`・`example/calendar_demo.cpp`)。残りはCalendarをポップアップ表示するEntry+ドロップダウンボタンの合成(`DateEntry`)。
  詳細: tasks.md G節
- [ ] **表形式データ専業のGrid Widgetを追加する（優先順位3、MVPスコープから）**
  `tksheet`(501スター、tkcalendarの約5倍)相当。`ttk::Treeview`はセル編集・行列DnD・セル埋め込みWidget等の「スプレッドシート的操作」をカバーしておらず需要は最も高いが、実装難易度も調査した候補の中で最大(仮想レンダリング等)。まず固定データセット・セル直接編集のみのMVPから始める。
  詳細: tasks.md G節
- [ ] **Tooltipを追加する（優先順位4、着手コストは低いがいずれ）**
  Tooltip専業パッケージは複数乱立し単一の支配的パッケージが無く、需要はあるが目玉機能としての訴求力は低いため優先度は下げた。
  詳細: tasks.md G節

外部のTclパッケージをラップする種類（`tkinterdnd2`相当）・OSネイティブAPIを直接叩く種類は、需要確認後・将来検討とし今は着手しない。

---

## バグ調査（起票済み・未着手）

- [ ] **Linux版でCanvas::postscriptがSegFaultする**
  `test_low_priority_parity_gaps.cpp`の"Canvas::postscript: PostScriptデータを文字列で返す"がLinux CI上でSIGSEGVでクラッシュする(Windowsでは問題なくパス)。プラットフォーム固有の問題である可能性が高いが未調査。CIがこの種の失敗でハングしないよう`--timeout`は対応済み(tasks.md J節)。
  詳細: tasks.md J節

---

## 配布・デプロイ（コピー方式は2026-07-08対応済み、zipfs単一ファイル化のみ将来課題）

- [x] **Tcl/Tk本体のランタイムスクリプト一式が実行環境に無いと、cpp_tkベースのアプリが起動できない**
  `cmake/CppTkRuntime.cmake`を追加し、`find_package`が検出したTcl/Tkインストールと同じ場所の`wish`を
  使って`$tcl_library`/`$tk_library`/zipfs対応有無を検出できるようにした。`CPP_TK_PACKAGE_RUNTIME_SCRIPTS`
  オプション(既定OFF)で実行ファイルの隣へスクリプト一式をコピーする配布方式(PyInstaller/cx_Freeze方式)、
  および`cpp_tk::set_runtime_library_paths()`によるユーザ指定の起点パス上書きAPIを実装済み。
  Tcl/Tk本体の自前ビルド(FetchContent等)はスコープ外と決定した(理由はtasks.md H節参照)。
  詳細: tasks.md H節

- [ ] **zipfsによるTcl/Tkランタイムの単一実行ファイル化(将来課題)**
  Tcl 9系(または`--enable-zipfs`ビルドの8.6)であれば、コピーしたスクリプト一式をzip化して
  実行ファイルにVFSマウントすることで単一ファイル配布が可能になる。検出自体
  (`CPP_TK_RUNTIME_HAS_ZIPFS`)は実装済みだが、このマシンのMSYS2 Tcl/Tk 8.6.13にはzipfsが
  一切含まれておらず(`tcl.h`にZipfs関連のC API宣言なし)実行時コードを検証できないため、
  zip化+VFSマウントの実装自体は見送った。zipfs対応のTcl/Tkが実機検証可能になり次第、着手する。
  詳細: tasks.md H節

---

## 対応しない方針で決定済み（参考・再検討不要）

- **macOSでのCI動作検証**: 手元にMac実機が無いため、現時点ではスコープ外と決定済み(Windows/Linuxは確認済み)。
- **QThread/wxThread的な専用スレッドラッパークラスの追加**: cpp_tkの「薄いラッパー」方針、本家tkinterにも無いことを踏まえ非対応と決定済み。
