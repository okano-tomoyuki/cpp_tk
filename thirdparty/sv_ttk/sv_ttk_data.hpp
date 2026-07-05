#ifndef CPP_TK_THIRD_PARTY_SV_TTK_DATA_HPP
#define CPP_TK_THIRD_PARTY_SV_TTK_DATA_HPP

#include <cstddef>

namespace cpp_tk
{
namespace custom
{
namespace detail
{

// Sun Valley ttk theme(https://github.com/rdbende/Sun-Valley-ttk-theme)を構成する各ファイルの
// 埋め込みデータ。relative_pathは"sv.tcl"を起点としたディレクトリ構造(実行時に一時ディレクトリへ
// 同じ構造で展開し、内部の[info script]相対source/ファイル参照をそのまま機能させるため)。
// binary_base64がtrueの場合、contentはbase64エンコード済みで、書き出し時にデコードする。
struct EmbeddedFile
{
    const char* relative_path;
    const char* content;
    std::size_t length;
    bool        binary_base64;
};

extern const EmbeddedFile sv_ttk_files[];
extern const std::size_t  sv_ttk_files_count;

} // namespace detail
} // namespace custom
} // namespace cpp_tk

#endif
