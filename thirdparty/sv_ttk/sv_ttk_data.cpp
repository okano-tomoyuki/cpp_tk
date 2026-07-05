// thirdparty/sv_ttk/sv_ttk_data.cpp
// Sun Valley ttk theme (https://github.com/rdbende/Sun-Valley-ttk-theme) by rdbende, MIT License.
// LICENSE-sv_ttk.txt に原文を同梱。ビルド時コード生成ではなく、ベンダリング時に一度だけ
// thirdparty/sv_ttk/配下の実ファイルから生成した静的な埋め込みデータ(test/doctest.hの
// ベンダリング方式と同じ扱い。上流更新時は本ファイルを再生成する)。
#include "sv_ttk_data.hpp"

namespace cpp_tk
{
namespace custom
{
namespace detail
{

static const char sv_tcl_content[] = R"SVSV(
package require Tk 8.6-

source [file join [file dirname [info script]] theme light.tcl]
source [file join [file dirname [info script]] theme dark.tcl]


if {[tk windowingsystem] == "win32"} {
  set static ""
} else {
  set static " static"
}

font create SunValleyCaptionFont -family "Segoe UI Variable$static Small" -size -12
font create SunValleyBodyFont -family "Segoe UI Variable$static Text" -size -14
font create SunValleyBodyStrongFont -family "Segoe UI Variable$static Text Semibold" -size -14
font create SunValleyBodyLargeFont -family "Segoe UI Variable$static Text" -size -18
font create SunValleySubtitleFont -family "Segoe UI Variable$static Display Semibold" -size -20
font create SunValleyTitleFont -family "Segoe UI Variable$static Display Semibold" -size -28
font create SunValleyTitleLargeFont -family "Segoe UI Variable$static Display Semibold" -size -40
font create SunValleyDisplayFont -family "Segoe UI Variable$static Display Semibold" -size -68


proc config_entry_font {w} {
  set font_config [$w config -font]
  if {[lindex $font_config 3] != [lindex $font_config 4]} {
    return
  }
  if {[ttk::style theme use] in {"sun-valley-dark" "sun-valley-light"}} {
    $w configure -font SunValleyBodyFont
  }
}


proc config_menus {w} {
  if {[tk windowingsystem] == "aqua" || [tk windowingsystem] == "win32"} {
    return
  }

  set theme [ttk::style theme use]
  if {$theme == "sun-valley-dark"} {
    $w configure \
      -relief solid \
      -borderwidth 1 \
      -activeborderwidth 0 \
      -background "#292929" \
      -activebackground $ttk::theme::sv_dark::colors(-selbg) \
      -activeforeground $ttk::theme::sv_dark::colors(-selfg) \
      -selectcolor $ttk::theme::sv_dark::colors(-selfg)
  } elseif {$theme == "sun-valley-light"} {
    $w configure \
      -relief solid \
      -borderwidth 1 \
      -activeborderwidth 0 \
      -background "#e7e7e7" \
      -activebackground $ttk::theme::sv_light::colors(-selbg) \
      -activeforeground $ttk::theme::sv_light::colors(-selfg) \
      -selectcolor $ttk::theme::sv_light::colors(-selfg)
  }

  if {[[winfo toplevel $w] cget -menu] != $w} {
    if {$theme == "sun-valley-dark"} {
      $w configure -borderwidth 0 -background $ttk::theme::sv_dark::colors(-bg)
    } elseif {$theme == "sun-valley-light"} {
      $w configure -borderwidth 0 -background $ttk::theme::sv_light::colors(-bg)
    }
  }
}


proc configure_colors {} {
  set theme [ttk::style theme use]
  if {$theme == "sun-valley-dark"} {
    ttk::style configure . \
      -background $ttk::theme::sv_dark::colors(-bg) \
      -foreground $ttk::theme::sv_dark::colors(-fg) \
      -troughcolor $ttk::theme::sv_dark::colors(-bg) \
      -focuscolor $ttk::theme::sv_dark::colors(-selbg) \
      -selectbackground $ttk::theme::sv_dark::colors(-selbg) \
      -selectforeground $ttk::theme::sv_dark::colors(-selfg) \
      -insertwidth 1 \
      -insertcolor $ttk::theme::sv_dark::colors(-fg) \
      -fieldbackground $ttk::theme::sv_dark::colors(-bg) \
      -font SunValleyBodyFont \
      -borderwidth 0 \
      -relief flat

    tk_setPalette \
      background $ttk::theme::sv_dark::colors(-bg) \
      foreground $ttk::theme::sv_dark::colors(-fg) \
      highlightColor $ttk::theme::sv_dark::colors(-selbg) \
      selectBackground $ttk::theme::sv_dark::colors(-selbg) \
      selectForeground $ttk::theme::sv_dark::colors(-selfg) \
      activeBackground $ttk::theme::sv_dark::colors(-selbg) \
      activeForeground $ttk::theme::sv_dark::colors(-selfg)

    ttk::style map . -foreground [list disabled $ttk::theme::sv_dark::colors(-disfg)]
  } elseif {$theme == "sun-valley-light"} {
    ttk::style configure . \
      -background $ttk::theme::sv_light::colors(-bg) \
      -foreground $ttk::theme::sv_light::colors(-fg) \
      -troughcolor $ttk::theme::sv_light::colors(-bg) \
      -focuscolor $ttk::theme::sv_light::colors(-selbg) \
      -selectbackground $ttk::theme::sv_light::colors(-selbg) \
      -selectforeground $ttk::theme::sv_light::colors(-selfg) \
      -insertwidth 1 \
      -insertcolor $ttk::theme::sv_light::colors(-fg) \
      -fieldbackground $ttk::theme::sv_light::colors(-bg) \
      -font SunValleyBodyFont \
      -borderwidth 0 \
      -relief flat

    tk_setPalette \
      background $ttk::theme::sv_light::colors(-bg) \
      foreground $ttk::theme::sv_light::colors(-fg) \
      highlightColor $ttk::theme::sv_light::colors(-selbg) \
      selectBackground $ttk::theme::sv_light::colors(-selbg) \
      selectForeground $ttk::theme::sv_light::colors(-selfg) \
      activeBackground $ttk::theme::sv_light::colors(-selbg) \
      activeForeground $ttk::theme::sv_light::colors(-selfg)

    ttk::style map . -foreground [list disabled $ttk::theme::sv_light::colors(-disfg)]
  }
}


bind [winfo class .] <<ThemeChanged>> {+configure_colors}
bind TEntry <<ThemeChanged>> {+config_entry_font %W}
bind TCombobox <<ThemeChanged>> {+config_entry_font %W}
bind TSpinbox <<ThemeChanged>> {+config_entry_font %W}
bind Menu <<ThemeChanged>> {+config_menus %W}
)SVSV";

static const char dark_tcl_content[] = R"SVDARK(
source [file join [file dirname [info script]] sprites_dark.tcl]

namespace eval ttk::theme::sv_dark {
  package provide ttk::theme::sv_dark 2.5

  array set colors {
    -fg      "#fafafa"
    -bg      "#1c1c1c"
    -disfg   "#595959"
    -selfg   "#ffffff"
    -selbg   "#2f60d8"
    -accent  "#57c8ff"
  }

  proc load_images {imgfile} {
    variable I
    image create photo spritesheet -file $imgfile -format png
    foreach {name x y width height} $::spriteinfo {
      set I($name) [image create photo -width $width -height $height]
      $I($name) copy spritesheet -from $x $y [expr {$x+$width}] [expr {$y+$height}]
    }
  }

  load_images [file join [file dirname [info script]] spritesheet_dark.png]

  ttk::style theme create sun-valley-dark -parent clam -settings {
        
    # Button
    ttk::style layout TButton {
      Button.button -children {
        Button.padding -children {
          Button.label -side left -expand 1
        } 
      }
    }

    ttk::style configure TButton -padding {8 2 8 3} -anchor center -foreground $colors(-fg)
    ttk::style map TButton -foreground [list disabled "#7a7a7a" pressed "#d0d0d0"]
    
    ttk::style element create Button.button image \
      [list $I(button-rest) \
        {selected disabled} $I(button-dis) \
        disabled $I(button-dis) \
        selected $I(button-rest) \
        pressed $I(button-pressed) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew

    # Toolbutton
    ttk::style layout Toolbutton {
      Toolbutton.button -children {
        Toolbutton.padding -children {
          Toolbutton.label -side left -expand 1
        } 
      }
    }

    ttk::style configure Toolbutton -padding {8 2 8 3} -anchor center
    
    ttk::style element create Toolbutton.button image \
      [list $I(empty) \
        disabled $I(button-dis) \
        pressed $I(button-pressed) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew

    # Accent.TButton
    ttk::style layout Accent.TButton {
      AccentButton.button -children {
        AccentButton.padding -children {
          AccentButton.label -side left -expand 1
        } 
      }
    }

    ttk::style configure Accent.TButton -padding {8 2 8 3} -anchor center -foreground "#000000"
    ttk::style map Accent.TButton -foreground [list pressed "#25536a" disabled "#a5a5a5"]

    ttk::style element create AccentButton.button image \
      [list $I(button-accent-rest) \
        {selected disabled} $I(button-accent-dis) \
        disabled $I(button-accent-dis) \
        selected $I(button-accent-rest) \
        pressed $I(button-accent-pressed) \
        {active focus} $I(button-accent-focus-hover) \
        active $I(button-accent-hover) \
        focus $I(button-accent-focus) \
      ] -border 4 -sticky nsew

    # Menubutton
    ttk::style layout TMenubutton {
      Menubutton.button -children {
        Menubutton.padding -children {
          Menubutton.label -side left -expand 1
          Menubutton.indicator -side right -sticky nsew
        }
      }
    }

    ttk::style configure TMenubutton -padding {8 2 13 3}

    ttk::style element create Menubutton.button image \
      [list $I(button-rest) \
        disabled $I(button-dis) \
        pressed $I(button-pressed) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew 

    ttk::style element create Menubutton.indicator image $I(down) -width 10 -sticky e

    # OptionMenu
    ttk::style layout TOptionMenu {
      OptionMenu.button -children {
        OptionMenu.padding -children {
          OptionMenu.label -side left -expand 1
          OptionMenu.indicator -side right -sticky nsew
        }
      }
    }
    
    ttk::style configure TOptionMenu -padding {8 2 13 3}

    ttk::style element create OptionMenu.button image \
      [list $I(button-rest) \
        disabled $I(button-dis) \
        pressed $I(button-pressed) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew 

    ttk::style element create OptionMenu.indicator image $I(down) -width 10 -sticky e

    # Checkbutton
    ttk::style layout TCheckbutton {
      Checkbutton.button -children {
        Checkbutton.padding -children {
          Checkbutton.indicator -side left
          Checkbutton.label -side right -expand 1
        }
      }
    }

    ttk::style configure TCheckbutton -padding 4

    ttk::style element create Checkbutton.indicator image \
      [list $I(check-unsel-rest) \
        {alternate disabled} $I(check-tri-dis) \
        {selected disabled} $I(check-dis) \
        disabled $I(check-unsel-dis) \
        {pressed alternate} $I(check-tri-hover) \
        {active focus alternate} $I(check-tri-focus-hover) \
        {active alternate} $I(check-tri-hover) \
        {focus alternate} $I(check-tri-focus) \
        alternate $I(check-tri-rest) \
        {pressed selected} $I(check-hover) \
        {active focus selected} $I(check-focus-hover) \
        {active selected} $I(check-hover) \
        {focus selected} $I(check-focus) \
        selected $I(check-rest) \
        {pressed !selected} $I(check-unsel-pressed) \
        {active focus} $I(check-unsel-focus-hover) \
        active $I(check-unsel-hover) \
        focus $I(check-unsel-focus) \
      ] -width 26 -sticky w

    # Switch.TCheckbutton
    ttk::style layout Switch.TCheckbutton {
      Switch.button -children {
        Switch.padding -children {
          Switch.indicator -side left
          Switch.label -side right -expand 1
        }
      }
    }

    ttk::style element create Switch.indicator image \
      [list $I(switch-off-rest) \
        {selected disabled} $I(switch-dis) \
        disabled $I(switch-off-dis) \
        {pressed selected} $I(switch-pressed) \
        {active focus selected} $I(switch-focus-hover) \
        {active selected} $I(switch-hover) \
        {focus selected} $I(switch-focus) \
        selected $I(switch-rest) \
        {pressed !selected} $I(switch-off-pressed) \
        {active focus} $I(switch-off-focus-hover) \
        active $I(switch-off-hover) \
        focus $I(switch-off-focus) \
      ] -width 46 -sticky w

    # Toggle.TButton
    ttk::style layout Toggle.TButton {
      ToggleButton.button -children {
        ToggleButton.padding -children {
          ToggleButton.label -side left -expand 1
        } 
      }
    }

    ttk::style configure Toggle.TButton -padding {8 2 8 3} -anchor center -foreground $colors(-fg)

    ttk::style map Toggle.TButton -foreground \
      [list {selected disabled} "#a5a5a5" \
        {selected pressed} "#d0d0d0" \
        selected "#000000" \
        pressed "#25536a" \
        disabled "#7a7a7a"
      ]

    ttk::style element create ToggleButton.button image \
      [list $I(button-rest) \
        {selected disabled} $I(button-accent-dis) \
        disabled $I(button-dis) \
        {pressed selected} $I(button-rest) \
        {active focus selected} $I(button-accent-focus-hover) \
        {active selected} $I(button-accent-hover) \
        {focus selected} $I(button-accent-focus) \
        selected $I(button-accent-rest) \
        {pressed !selected} $I(button-accent-rest) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew

    # Radiobutton
    ttk::style layout TRadiobutton {
      Radiobutton.button -children {
        Radiobutton.padding -children {
          Radiobutton.indicator -side left
          Radiobutton.label -side right -expand 1
        }
      }
    }

    ttk::style configure TRadiobutton -padding 4

    ttk::style element create Radiobutton.indicator image \
      [list $I(radio-unsel-rest) \
        {selected disabled} $I(radio-dis) \
        disabled $I(radio-unsel-dis) \
        {pressed selected} $I(radio-pressed) \
        {active focus selected} $I(radio-focus-hover) \
        {active selected} $I(radio-hover) \
        {focus selected} $I(radio-focus) \
        selected $I(radio-rest) \
        {pressed !selected} $I(radio-unsel-pressed) \
        {active focus} $I(radio-unsel-focus-hover) \
        active $I(radio-unsel-hover) \
        focus $I(radio-unsel-focus) \
      ] -width 26 -sticky w

    # Entry
    ttk::style configure TEntry -foreground $colors(-fg) -padding {6 1 4 2}
    ttk::style map TEntry -foreground [list disabled "#757575" pressed "#cfcfcf"]

    ttk::style element create Entry.field image \
      [list $I(textbox-rest) \
        {focus hover !invalid} $I(textbox-focus) \
        invalid $I(textbox-error) \
        disabled $I(textbox-dis) \
        {focus !invalid} $I(textbox-focus) \
        hover $I(textbox-hover) \
      ] -border 5 -sticky nsew

    # Combobox
    ttk::style layout TCombobox {
      Combobox.field -sticky nsew -children {
        Combobox.arrow -side right -sticky ns
        Combobox.padding -sticky nsew -children {
          Combobox.textarea -sticky nsew
        }
      }
    }
        
    ttk::style configure TCombobox -foreground $colors(-fg) -padding {6 1 0 2}
    ttk::style configure ComboboxPopdownFrame -borderwidth 1 -relief solid
    ttk::style map TCombobox -foreground [list disabled "#757575" pressed "#cfcfcf"]
    
    ttk::style map TCombobox -selectbackground [list \
      {readonly hover} $colors(-selbg) \
      {readonly focus} $colors(-selbg) \
    ] -selectforeground [list \
      {readonly hover} $colors(-selfg) \
      {readonly focus} $colors(-selfg) \
    ]

    ttk::style element create Combobox.field image \
      [list $I(textbox-rest) \
        {readonly focus} $I(button-focus) \
        {readonly disabled} $I(button-dis) \
        {readonly pressed} $I(button-pressed) \
        {readonly hover} $I(button-hover) \
        readonly $I(button-rest) \
        {focus hover !invalid} $I(textbox-focus) \
        invalid $I(textbox-error) \
        disabled $I(textbox-dis) \
        focus $I(textbox-focus) \
        {focus !invalid} $I(textbox-focus) \
        hover $I(textbox-hover) \
      ] -border 5
        
    ttk::style element create Combobox.arrow image $I(down) -width 34 -sticky {}

    # Spinbox
    ttk::style layout TSpinbox {
      Spinbox.field -side top -sticky we -children {
        Spinbox.downarrow -side right -sticky ns
        Spinbox.uparrow -side right -sticky ns
        Spinbox.padding -sticky nswe -children {
          Spinbox.textarea -sticky nsew
        }
      }
    }

    ttk::style configure TSpinbox -foreground $colors(-fg) -padding {6 1 0 2}
    ttk::style map TSpinbox -foreground [list disabled "#757575" pressed "#cfcfcf"]

    ttk::style element create Spinbox.field image \
      [list $I(textbox-rest) \
        {focus hover !invalid} $I(textbox-focus) \
        invalid $I(textbox-error) \
        disabled $I(textbox-dis) \
        focus $I(textbox-focus) \
        {focus !invalid} $I(textbox-focus) \
        hover $I(textbox-hover) \
      ] -border 5 -sticky nsew

    ttk::style element create Spinbox.uparrow image $I(up) -width 34 -height 16 -sticky {}
    ttk::style element create Spinbox.downarrow image $I(down) -width 34 -height 16 -sticky {}

    # Progressbar
    ttk::style element create Horizontal.Progressbar.trough image $I(progressbar-trough-hor) -border 1 -sticky ew
    ttk::style element create Horizontal.Progressbar.pbar image $I(progressbar-bar-hor) -border 2 -sticky ew

    ttk::style element create Vertical.Progressbar.trough image $I(progressbar-trough-vert) -border 1 -sticky ns
    ttk::style element create Vertical.Progressbar.pbar image $I(progressbar-bar-vert) -border 2 -sticky ns

    # Scale
    ttk::style element create Horizontal.Scale.trough image $I(slider-trough-hor) \
      -border 5 -padding 0 -sticky {ew}

    ttk::style element create Vertical.Scale.trough image $I(slider-trough-vert) \
      -border 5 -padding 0 -sticky {ns}

    ttk::style element create Scale.slider image \
      [list $I(slider-thumb-rest) \
        disabled $I(slider-thumb-dis) \
        pressed $I(slider-thumb-pressed) \
        {active focus} $I(slider-thumb-focus-hover) \
        active $I(slider-thumb-hover) \
        focus $I(slider-thumb-focus) \
      ] -sticky {}

    # Scrollbar
    ttk::style layout Vertical.TScrollbar {
      Vertical.Scrollbar.trough -sticky ns -children {
        Vertical.Scrollbar.uparrow -side top
        Vertical.Scrollbar.downarrow -side bottom
        Vertical.Scrollbar.thumb -expand 1
      }
    }

    ttk::style layout Horizontal.TScrollbar {
      Horizontal.Scrollbar.trough -sticky ew -children {
        Horizontal.Scrollbar.leftarrow -side left
        Horizontal.Scrollbar.rightarrow -side right
        Horizontal.Scrollbar.thumb -expand 1
      }
    }

    ttk::style element create Horizontal.Scrollbar.trough image $I(scrollbar-trough-hor) -sticky ew -border {6 0}
    ttk::style element create Horizontal.Scrollbar.thumb image $I(scrollbar-thumb-hor) -sticky ew -border {3 0}

    ttk::style element create Horizontal.Scrollbar.rightarrow image $I(scrollbar-right) -sticky e -width 13
    ttk::style element create Horizontal.Scrollbar.leftarrow image $I(scrollbar-left) -sticky w -width 13

    ttk::style element create Vertical.Scrollbar.trough image $I(scrollbar-trough-vert) -sticky ns -border {0 6}
    ttk::style element create Vertical.Scrollbar.thumb image $I(scrollbar-thumb-vert) -sticky ns -border {0 3}

    ttk::style element create Vertical.Scrollbar.uparrow image $I(scrollbar-up) -sticky n -height 13
    ttk::style element create Vertical.Scrollbar.downarrow image $I(scrollbar-down) -sticky s -height 13

    # Separator
    ttk::style element create Separator.separator image $I(sep) -width 1 -height 1

    # Sizegrip
    ttk::style element create Sizegrip.sizegrip image $I(grip) -sticky nsew

    # Card
    ttk::style layout Card.TFrame {
      Card.field {
        Card.padding -expand 1 
      }
    }

    ttk::style element create Card.field image $I(card) -border 10 -padding 4 -sticky nsew

    # Labelframe
    ttk::style layout TLabelframe {
      Labelframe.border {
        Labelframe.padding -expand 1 -children {
          Labelframe.label -side left
        }
      }
    }

    ttk::style element create Labelframe.border image $I(card) -border 5 -padding 4 -sticky nsew
    ttk::style configure TLabelframe.Label -font SunValleyCaptionFont

    # Notebook
    ttk::style layout TNotebook {
      Notebook.border -children {
        TNotebook.Tab -expand 1
      }
    }

    ttk::style configure TNotebook -padding 1
    ttk::style configure TNotebook.Tab -focuscolor $colors(-accent)
    ttk::style element create Notebook.border image $I(notebook-border) -border 5 -padding 5

    ttk::style element create Notebook.tab image \
      [list $I(tab-rest) \
        selected $I(tab-selected) \
        active $I(tab-hover) \
      ] -border 13 -padding {16 14 16 6} -height 32

    # Treeview
    ttk::style configure Heading -font SunValleyCaptionFont
    ttk::style configure Treeview \
        -background $colors(-bg) \
        -rowheight [expr {[font metrics SunValleyBodyFont -linespace] + 3}] \
        -font SunValleyBodyFont

    ttk::style map Treeview -background {selected "#292929"} -foreground "selected $colors(-selfg)"

    ttk::style element create Treeview.field image $I(card) -border 5 -width 0 -height 0
    
    ttk::style element create Treeheading.cell image \
      [list $I(heading-rest) \
        pressed $I(heading-pressed) \
        active $I(heading-hover)
      ] -border 5 -padding 14 -sticky nsew
    
    ttk::style element create Treeitem.indicator image \
      [list $I(right) \
        user2 $I(empty) \
        user1 $I(down) \
      ] -width 26 -sticky {}

    # Panedwindow
    ttk::style configure Sash \
      -lightcolor "#9e9e9e" \
      -darkcolor "#9e9e9e" \
      -bordercolor "#9e9e9e" \
      -sashthickness 4 \
      -gripcount 20
  }
}
)SVDARK";

static const char light_tcl_content[] = R"SVLIGHT(
source [file join [file dirname [info script]] sprites_light.tcl]

namespace eval ttk::theme::sv_light {
  package provide ttk::theme::sv_light 2.5

  array set colors {
    -fg      "#1c1c1c"
    -bg      "#fafafa"
    -disfg   "#a0a0a0"
    -selfg   "#ffffff"
    -selbg   "#2f60d8"
    -accent  "#005fb8"
  }

  proc load_images {imgfile} {
    variable I
    image create photo spritesheet -file $imgfile -format png
    foreach {name x y width height} $::spriteinfo {
      set I($name) [image create photo -width $width -height $height]
      $I($name) copy spritesheet -from $x $y [expr {$x+$width}] [expr {$y+$height}]
    }
  }

  load_images [file join [file dirname [info script]] spritesheet_light.png]

  ttk::style theme create sun-valley-light -parent clam -settings {
        
    # Button
    ttk::style layout TButton {
      Button.button -children {
        Button.padding -children {
          Button.label -side left -expand 1
        } 
      }
    }

    ttk::style configure TButton -padding {8 2 8 3} -anchor center -foreground $colors(-fg)
    ttk::style map TButton -foreground [list disabled "#a2a2a2" pressed "#636363" active "#1a1a1a"]
    
    ttk::style element create Button.button image \
      [list $I(button-rest) \
        {selected disabled} $I(button-dis) \
        disabled $I(button-dis) \
        selected $I(button-rest) \
        pressed $I(button-pressed) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew

    # Toolbutton
    ttk::style layout Toolbutton {
      Toolbutton.button -children {
        Toolbutton.padding -children {
          Toolbutton.label -side left -expand 1
        } 
      }
    }

    ttk::style configure Toolbutton -padding {8 2 8 3} -anchor center
    
    ttk::style element create Toolbutton.button image \
      [list $I(empty) \
        disabled $I(button-dis) \
        pressed $I(button-pressed) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew

    # Accent.TButton
    ttk::style layout Accent.TButton {
      AccentButton.button -children {
        AccentButton.padding -children {
          AccentButton.label -side left -expand 1
        } 
      }
    }

    ttk::style configure Accent.TButton -padding {8 2 8 3} -anchor center -foreground "#ffffff"
    ttk::style map Accent.TButton -foreground [list pressed "#c1d8ee" disabled "#ffffff"]

    ttk::style element create AccentButton.button image \
      [list $I(button-accent-rest) \
        {selected disabled} $I(button-accent-dis) \
        disabled $I(button-accent-dis) \
        selected $I(button-accent-rest) \
        pressed $I(button-accent-pressed) \
        {active focus} $I(button-accent-focus-hover) \
        active $I(button-accent-hover) \
        focus $I(button-accent-focus) \
      ] -border 4 -sticky nsew

    # Menubutton
    ttk::style layout TMenubutton {
      Menubutton.button -children {
        Menubutton.padding -children {
          Menubutton.label -side left -expand 1
          Menubutton.indicator -side right -sticky nsew
        }
      }
    }

    ttk::style configure TMenubutton -padding {8 2 13 3}

    ttk::style element create Menubutton.button image \
      [list $I(button-rest) \
        disabled $I(button-dis) \
        pressed $I(button-pressed) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew 

    ttk::style element create Menubutton.indicator image $I(down) -width 10 -sticky e

    # OptionMenu
    ttk::style layout TOptionMenu {
      OptionMenu.button -children {
        OptionMenu.padding -children {
          OptionMenu.label -side left -expand 1
          OptionMenu.indicator -side right -sticky nsew
        }
      }
    }
    
    ttk::style configure TOptionMenu -padding {8 2 13 3}

    ttk::style element create OptionMenu.button image \
      [list $I(button-rest) \
        disabled $I(button-dis) \
        pressed $I(button-pressed) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew 

    ttk::style element create OptionMenu.indicator image $I(down) -width 10 -sticky e

    # Checkbutton
    ttk::style layout TCheckbutton {
      Checkbutton.button -children {
        Checkbutton.padding -children {
          Checkbutton.indicator -side left
          Checkbutton.label -side right -expand 1
        }
      }
    }

    ttk::style configure TCheckbutton -padding 4

    ttk::style element create Checkbutton.indicator image \
      [list $I(check-unsel-rest) \
        {alternate disabled} $I(check-tri-dis) \
        {selected disabled} $I(check-dis) \
        disabled $I(check-unsel-dis) \
        {pressed alternate} $I(check-tri-hover) \
        {active focus alternate} $I(check-tri-focus-hover) \
        {active alternate} $I(check-tri-hover) \
        {focus alternate} $I(check-tri-focus) \
        alternate $I(check-tri-rest) \
        {pressed selected} $I(check-hover) \
        {active focus selected} $I(check-focus-hover) \
        {active selected} $I(check-hover) \
        {focus selected} $I(check-focus) \
        selected $I(check-rest) \
        {pressed !selected} $I(check-unsel-pressed) \
        {active focus} $I(check-unsel-focus-hover) \
        active $I(check-unsel-hover) \
        focus $I(check-unsel-focus) \
      ] -width 26 -sticky w

    # Switch.TCheckbutton
    ttk::style layout Switch.TCheckbutton {
      Switch.button -children {
        Switch.padding -children {
          Switch.indicator -side left
          Switch.label -side right -expand 1
        }
      }
    }

    ttk::style element create Switch.indicator image \
      [list $I(switch-off-rest) \
        {selected disabled} $I(switch-dis) \
        disabled $I(switch-off-dis) \
        {pressed selected} $I(switch-pressed) \
        {active focus selected} $I(switch-focus-hover) \
        {active selected} $I(switch-hover) \
        {focus selected} $I(switch-focus) \
        selected $I(switch-rest) \
        {pressed !selected} $I(switch-off-pressed) \
        {active focus} $I(switch-off-focus-hover) \
        active $I(switch-off-hover) \
        focus $I(switch-off-focus) \
      ] -width 46 -sticky w

    # Toggle.TButton
    ttk::style layout Toggle.TButton {
      ToggleButton.button -children {
        ToggleButton.padding -children {
          ToggleButton.label -side left -expand 1
        } 
      }
    }

    ttk::style configure Toggle.TButton -padding {8 2 8 3} -anchor center -foreground $colors(-fg)

    ttk::style map Toggle.TButton -foreground \
      [list {selected disabled} "#ffffff" \
        {selected pressed} "#636363" \
        selected "#ffffff" \
        pressed "#c1d8ee" \
        disabled "#a2a2a2" \
        active "#1a1a1a"
      ]

    ttk::style element create ToggleButton.button image \
      [list $I(button-rest) \
        {selected disabled} $I(button-accent-dis) \
        disabled $I(button-dis) \
        {pressed selected} $I(button-rest) \
        {active focus selected} $I(button-accent-focus-hover) \
        {active selected} $I(button-accent-hover) \
        {focus selected} $I(button-accent-focus) \
        selected $I(button-accent-rest) \
        {pressed !selected} $I(button-accent-rest) \
        {active focus} $I(button-focus-hover) \
        active $I(button-hover) \
        focus $I(button-focus) \
      ] -border 4 -sticky nsew

    # Radiobutton
    ttk::style layout TRadiobutton {
      Radiobutton.button -children {
        Radiobutton.padding -children {
          Radiobutton.indicator -side left
          Radiobutton.label -side right -expand 1
        }
      }
    }

    ttk::style configure TRadiobutton -padding 4

    ttk::style element create Radiobutton.indicator image \
      [list $I(radio-unsel-rest) \
        {selected disabled} $I(radio-dis) \
        disabled $I(radio-unsel-dis) \
        {pressed selected} $I(radio-pressed) \
        {active focus selected} $I(radio-focus-hover) \
        {active selected} $I(radio-hover) \
        {focus selected} $I(radio-focus) \
        selected $I(radio-rest) \
        {pressed !selected} $I(radio-unsel-pressed) \
        {active focus} $I(radio-unsel-focus-hover) \
        active $I(radio-unsel-hover) \
        focus $I(radio-unsel-focus) \
      ] -width 26 -sticky w

    # Entry
    ttk::style configure TEntry -foreground $colors(-fg) -padding {6 1 4 2}
    ttk::style map TEntry -foreground [list disabled $colors(-disfg) pressed "#636363" active "#626262"]

    ttk::style element create Entry.field image \
      [list $I(textbox-rest) \
        {focus hover !invalid} $I(textbox-focus) \
        invalid $I(textbox-error) \
        disabled $I(textbox-dis) \
        {focus !invalid} $I(textbox-focus) \
        hover $I(textbox-hover) \
      ] -border 5 -sticky nsew

    # Combobox
    ttk::style layout TCombobox {
      Combobox.field -sticky nsew -children {
        Combobox.arrow -side right -sticky ns
        Combobox.padding -sticky nsew -children {
          Combobox.textarea -sticky nsew
        }
      }
    }

    ttk::style configure TCombobox -foreground $colors(-fg) -padding {6 1 0 2}
    ttk::style configure ComboboxPopdownFrame -borderwidth 1 -relief solid
    ttk::style map TCombobox -foreground [list disabled $colors(-disfg) pressed "#636363" active "#626262"]
    
    ttk::style map TCombobox -selectbackground [list \
      {readonly hover} $colors(-selbg) \
      {readonly focus} $colors(-selbg) \
    ] -selectforeground [list \
      {readonly hover} $colors(-selfg) \
      {readonly focus} $colors(-selfg) \
    ]

    ttk::style element create Combobox.field image \
      [list $I(textbox-rest) \
        {readonly focus} $I(button-focus) \
        {readonly disabled} $I(button-dis) \
        {readonly pressed} $I(button-pressed) \
        {readonly hover} $I(button-hover) \
        readonly $I(button-rest) \
        {focus hover !invalid} $I(textbox-focus) \
        invalid $I(textbox-error) \
        disabled $I(textbox-dis) \
        focus $I(textbox-focus) \
        {focus !invalid} $I(textbox-focus) \
        hover $I(textbox-hover) \
      ] -border 5

    ttk::style element create Combobox.arrow image $I(down) -width 34 -sticky {}

    # Spinbox
    ttk::style layout TSpinbox {
      Spinbox.field -side top -sticky we -children {
        Spinbox.downarrow -side right -sticky ns
        Spinbox.uparrow -side right -sticky ns
        Spinbox.padding -sticky nswe -children {
          Spinbox.textarea -sticky nsew
        }
      }
    }

    ttk::style configure TSpinbox -foreground $colors(-fg) -padding {6 1 0 2}
    ttk::style map TSpinbox -foreground [list disabled $colors(-disfg) pressed "#636363" active "#626262"]

    ttk::style element create Spinbox.field image \
      [list $I(textbox-rest) \
        {focus hover !invalid} $I(textbox-focus) \
        invalid $I(textbox-error) \
        disabled $I(textbox-dis) \
        focus $I(textbox-focus) \
        {focus !invalid} $I(textbox-focus) \
        hover $I(textbox-hover) \
      ] -border 5 -sticky nsew

    ttk::style element create Spinbox.uparrow image $I(up) -width 34 -height 16 -sticky {}
    ttk::style element create Spinbox.downarrow image $I(down) -width 34 -height 16 -sticky {}

    # Progressbar
    ttk::style element create Horizontal.Progressbar.trough image $I(progressbar-trough-hor) \
      -border 1 -sticky ew
    ttk::style element create Horizontal.Progressbar.pbar image $I(progressbar-bar-hor) \
      -border 2 -sticky ew

    ttk::style element create Vertical.Progressbar.trough image $I(progressbar-trough-vert) \
      -border 1 -sticky ns
    ttk::style element create Vertical.Progressbar.pbar image $I(progressbar-bar-vert) \
      -border 2 -sticky ns

    # Scale
    ttk::style element create Horizontal.Scale.trough image $I(slider-trough-hor) \
      -border 5 -padding 0 -sticky {ew}

    ttk::style element create Vertical.Scale.trough image $I(slider-trough-vert) \
      -border 5 -padding 0 -sticky {ns}

    ttk::style element create Scale.slider image \
      [list $I(slider-thumb-rest) \
        disabled $I(slider-thumb-dis) \
        pressed $I(slider-thumb-pressed) \
        {active focus} $I(slider-thumb-focus-hover) \
        active $I(slider-thumb-hover) \
        focus $I(slider-thumb-focus) \
      ] -sticky {}

    # Scrollbar
    ttk::style layout Vertical.TScrollbar {
      Vertical.Scrollbar.trough -sticky ns -children {
        Vertical.Scrollbar.uparrow -side top
        Vertical.Scrollbar.downarrow -side bottom
        Vertical.Scrollbar.thumb -expand 1
      }
    }

    ttk::style layout Horizontal.TScrollbar {
      Horizontal.Scrollbar.trough -sticky ew -children {
        Horizontal.Scrollbar.leftarrow -side left
        Horizontal.Scrollbar.rightarrow -side right
        Horizontal.Scrollbar.thumb -expand 1
      }
    }

    ttk::style element create Horizontal.Scrollbar.trough image $I(scrollbar-trough-hor) -sticky ew -border {6 0}
    ttk::style element create Horizontal.Scrollbar.thumb image $I(scrollbar-thumb-hor) -sticky ew -border {3 0}

    ttk::style element create Horizontal.Scrollbar.rightarrow image $I(scrollbar-right) -sticky e -width 13
    ttk::style element create Horizontal.Scrollbar.leftarrow image $I(scrollbar-left) -sticky w -width 13

    ttk::style element create Vertical.Scrollbar.trough image $I(scrollbar-trough-vert) -sticky ns -border {0 6}
    ttk::style element create Vertical.Scrollbar.thumb image $I(scrollbar-thumb-vert) -sticky ns -border {0 3}

    ttk::style element create Vertical.Scrollbar.uparrow image $I(scrollbar-up) -sticky n -height 13
    ttk::style element create Vertical.Scrollbar.downarrow image $I(scrollbar-down) -sticky s -height 13

    # Separator
    ttk::style element create Separator.separator image $I(sep) -width 1 -height 1

    # Sizegrip
    ttk::style element create Sizegrip.sizegrip image $I(grip) -sticky nsew

    # Card
    ttk::style layout Card.TFrame {
      Card.field {
        Card.padding -expand 1 
      }
    }

    ttk::style element create Card.field image $I(card) -border 10 -padding 4 -sticky nsew

    # Labelframe
    ttk::style layout TLabelframe {
      Labelframe.border {
        Labelframe.padding -expand 1 -children {
          Labelframe.label -side left
        }
      }
    }

    ttk::style element create Labelframe.border image $I(card) -border 5 -padding 4 -sticky nsew
    ttk::style configure TLabelframe.Label -font SunValleyCaptionFont

    # Notebook
    ttk::style layout TNotebook {
      Notebook.border -children {
        TNotebook.Tab -expand 1
      }
    }

    ttk::style configure TNotebook -padding 1
    ttk::style configure TNotebook.Tab -focuscolor $colors(-accent)
    ttk::style element create Notebook.border image $I(notebook-border) -border 5 -padding 5

    ttk::style element create Notebook.tab image \
      [list $I(tab-rest) \
        selected $I(tab-selected) \
        active $I(tab-hover) \
      ] -border 13 -padding {16 14 16 6} -height 32

    # Treeview
    ttk::style configure Heading -font SunValleyCaptionFont
    ttk::style configure Treeview \
        -background $colors(-bg) \
        -rowheight [expr {[font metrics SunValleyBodyFont -linespace] + 3}] \
        -font SunValleyBodyFont

    ttk::style map Treeview -background {selected "#e7e7e7"} -foreground {selected "#191919"}

    ttk::style element create Treeview.field image $I(card) -border 5 -width 0 -height 0
    ttk::style element create Treeheading.cell image \
      [list $I(heading-rest) \
        pressed $I(heading-pressed) \
        active $I(heading-hover)
      ] -border 5 -padding 14 -sticky nsew
    
    ttk::style element create Treeitem.indicator image \
      [list $I(right) \
        user2 $I(empty) \
        user1 $I(down) \
      ] -width 26 -sticky {}

    # Panedwindow
    ttk::style configure Sash \
      -lightcolor "#676767" \
      -darkcolor "#676767" \
      -bordercolor "#676767" \
      -sashthickness 4 \
      -gripcount 20
  }
}
)SVLIGHT";

static const char sprites_dark_tcl_content[] = R"SVSPRD(
set ::spriteinfo [list \
  card 0 0 50 50 \
  notebook-border 50 0 40 40 \
  switch-dis 50 40 40 20 \
  switch-focus-hover 0 50 40 20 \
  switch-focus 0 70 40 20 \
  switch-hover 40 60 40 20 \
  switch-off-dis 90 0 40 20 \
  switch-off-focus-hover 90 20 40 20 \
  switch-off-focus 90 40 40 20 \
  switch-off-hover 80 60 40 20 \
  switch-off-pressed 0 90 40 20 \
  switch-off-rest 40 80 40 20 \
  switch-pressed 80 80 40 20 \
  switch-rest 0 110 40 20 \
  tab-hover 130 0 32 32 \
  tab-rest 130 32 32 32 \
  tab-selected 120 64 32 32 \
  heading-hover 40 100 22 22 \
  heading-pressed 62 100 22 22 \
  heading-rest 84 100 22 22 \
  slider-thumb-dis 106 100 22 22 \
  slider-thumb-focus-hover 128 96 22 22 \
  slider-thumb-focus 0 130 22 22 \
  slider-thumb-hover 22 130 22 22 \
  slider-thumb-pressed 44 122 22 22 \
  slider-thumb-rest 66 122 22 22 \
  slider-trough-hor 88 122 22 22 \
  slider-trough-vert 110 122 22 22 \
  button-accent-dis 132 118 20 20 \
  button-accent-focus-hover 0 152 20 20 \
  button-accent-focus 20 152 20 20 \
  button-accent-hover 40 152 20 20 \
  button-accent-pressed 60 144 20 20 \
  button-accent-rest 80 144 20 20 \
  button-dis 100 144 20 20 \
  button-focus-hover 120 144 20 20 \
  button-focus 140 138 20 20 \
  button-hover 162 0 20 20 \
  button-pressed 162 20 20 20 \
  button-rest 162 40 20 20 \
  check-dis 162 60 20 20 \
  check-focus-hover 150 96 20 20 \
  check-focus 152 116 20 20 \
  check-hover 160 136 20 20 \
  check-pressed 0 172 20 20 \
  check-rest 20 172 20 20 \
  check-tri-dis 40 172 20 20 \
  check-tri-focus-hover 160 156 20 20 \
  check-tri-focus 140 158 20 20 \
  check-tri-hover 60 164 20 20 \
  check-tri-pressed 80 164 20 20 \
  check-tri-rest 100 164 20 20 \
  check-unsel-dis 120 164 20 20 \
  check-unsel-focus-hover 182 0 20 20 \
  check-unsel-focus 182 20 20 20 \
  check-unsel-hover 182 40 20 20 \
  check-unsel-pressed 182 60 20 20 \
  check-unsel-rest 180 80 20 20 \
  progressbar-bar-hor 180 100 20 5 \
  progressbar-bar-vert 172 80 5 20 \
  progressbar-trough-hor 152 80 20 5 \
  progressbar-trough-vert 172 100 5 20 \
  radio-dis 180 105 20 20 \
  radio-focus-hover 180 125 20 20 \
  radio-focus 180 145 20 20 \
  radio-hover 180 165 20 20 \
  radio-pressed 160 176 20 20 \
  radio-rest 140 178 20 20 \
  radio-unsel-dis 0 192 20 20 \
  radio-unsel-focus-hover 20 192 20 20 \
  radio-unsel-focus 40 192 20 20 \
  radio-unsel-hover 180 185 20 20 \
  radio-unsel-pressed 60 184 20 20 \
  radio-unsel-rest 80 184 20 20 \
  scrollbar-thumb-hor 160 196 20 12 \
  scrollbar-thumb-vert 100 184 12 20 \
  scrollbar-trough-hor 112 198 20 12 \
  scrollbar-trough-vert 202 0 12 20 \
  textbox-dis 0 212 20 20 \
  textbox-error 20 212 20 20 \
  textbox-focus 40 212 20 20 \
  textbox-hover 60 210 20 20 \
  textbox-rest 80 210 20 20 \
  down 40 50 10 5 \
  empty 152 64 10 10 \
  grip 152 85 10 10 \
  right 162 85 5 10 \
  sep 202 20 10 10 \
  up 40 55 10 5 \
  scrollbar-down 132 138 8 6 \
  scrollbar-left 44 144 6 8 \
  scrollbar-right 50 144 6 8 \
  scrollbar-up 172 120 8 6 \
]
)SVSPRD";

static const char sprites_light_tcl_content[] = R"SVSPRL(
set ::spriteinfo [list \
  card 0 0 50 50 \
  notebook-border 50 0 40 40 \
  switch-dis 50 40 40 20 \
  switch-focus-hover 0 50 40 20 \
  switch-focus 0 70 40 20 \
  switch-hover 40 60 40 20 \
  switch-off-dis 90 0 40 20 \
  switch-off-focus-hover 90 20 40 20 \
  switch-off-focus 90 40 40 20 \
  switch-off-hover 80 60 40 20 \
  switch-off-pressed 0 90 40 20 \
  switch-off-rest 40 80 40 20 \
  switch-pressed 80 80 40 20 \
  switch-rest 0 110 40 20 \
  tab-hover 130 0 32 32 \
  tab-rest 130 32 32 32 \
  tab-selected 120 64 32 32 \
  heading-hover 40 100 22 22 \
  heading-pressed 62 100 22 22 \
  heading-rest 84 100 22 22 \
  slider-thumb-dis 106 100 22 22 \
  slider-thumb-focus-hover 128 96 22 22 \
  slider-thumb-focus 0 130 22 22 \
  slider-thumb-hover 22 130 22 22 \
  slider-thumb-pressed 44 122 22 22 \
  slider-thumb-rest 66 122 22 22 \
  slider-trough-hor 88 122 22 22 \
  slider-trough-vert 110 122 22 22 \
  button-accent-dis 132 118 20 20 \
  button-accent-focus-hover 0 152 20 20 \
  button-accent-focus 20 152 20 20 \
  button-accent-hover 40 152 20 20 \
  button-accent-pressed 60 144 20 20 \
  button-accent-rest 80 144 20 20 \
  button-dis 100 144 20 20 \
  button-focus-hover 120 144 20 20 \
  button-focus 140 138 20 20 \
  button-hover 162 0 20 20 \
  button-pressed 162 20 20 20 \
  button-rest 162 40 20 20 \
  check-dis 162 60 20 20 \
  check-focus-hover 150 96 20 20 \
  check-focus 152 116 20 20 \
  check-hover 160 136 20 20 \
  check-pressed 0 172 20 20 \
  check-rest 20 172 20 20 \
  check-tri-dis 40 172 20 20 \
  check-tri-focus-hover 160 156 20 20 \
  check-tri-focus 140 158 20 20 \
  check-tri-hover 60 164 20 20 \
  check-tri-pressed 80 164 20 20 \
  check-tri-rest 100 164 20 20 \
  check-unsel-dis 120 164 20 20 \
  check-unsel-focus-hover 182 0 20 20 \
  check-unsel-focus 182 20 20 20 \
  check-unsel-hover 182 40 20 20 \
  check-unsel-pressed 182 60 20 20 \
  check-unsel-rest 180 80 20 20 \
  progressbar-bar-hor 180 100 20 5 \
  progressbar-bar-vert 172 80 5 20 \
  progressbar-trough-hor 152 80 20 5 \
  progressbar-trough-vert 172 100 5 20 \
  radio-dis 180 105 20 20 \
  radio-focus-hover 180 125 20 20 \
  radio-focus 180 145 20 20 \
  radio-hover 180 165 20 20 \
  radio-pressed 160 176 20 20 \
  radio-rest 140 178 20 20 \
  radio-unsel-dis 0 192 20 20 \
  radio-unsel-focus-hover 20 192 20 20 \
  radio-unsel-focus 40 192 20 20 \
  radio-unsel-hover 180 185 20 20 \
  radio-unsel-pressed 60 184 20 20 \
  radio-unsel-rest 80 184 20 20 \
  scrollbar-thumb-hor 160 196 20 12 \
  scrollbar-thumb-vert 100 184 12 20 \
  scrollbar-trough-hor 112 198 20 12 \
  scrollbar-trough-vert 202 0 12 20 \
  textbox-dis 0 212 20 20 \
  textbox-error 20 212 20 20 \
  textbox-focus 40 212 20 20 \
  textbox-hover 60 210 20 20 \
  textbox-rest 80 210 20 20 \
  down 40 50 10 5 \
  empty 152 64 10 10 \
  grip 152 85 10 10 \
  right 162 85 5 10 \
  sep 202 20 10 10 \
  up 40 55 10 5 \
  scrollbar-down 132 138 8 6 \
  scrollbar-left 44 144 6 8 \
  scrollbar-right 50 144 6 8 \
  scrollbar-up 172 120 8 6 \
]
)SVSPRL";

static const char spritesheet_dark_b64[] = R"SVB64D(
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)SVB64D";

static const char spritesheet_light_b64[] = R"SVB64L(
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)SVB64L";

const EmbeddedFile sv_ttk_files[] =
{
    {"sv.tcl",                     sv_tcl_content,           sizeof(sv_tcl_content) - 1,           false},
    {"theme/dark.tcl",             dark_tcl_content,         sizeof(dark_tcl_content) - 1,         false},
    {"theme/light.tcl",            light_tcl_content,        sizeof(light_tcl_content) - 1,        false},
    {"theme/sprites_dark.tcl",     sprites_dark_tcl_content, sizeof(sprites_dark_tcl_content) - 1, false},
    {"theme/sprites_light.tcl",    sprites_light_tcl_content,sizeof(sprites_light_tcl_content) - 1,false},
    {"theme/spritesheet_dark.png", spritesheet_dark_b64,     sizeof(spritesheet_dark_b64) - 1,     true},
    {"theme/spritesheet_light.png",spritesheet_light_b64,    sizeof(spritesheet_light_b64) - 1,    true},
};

const std::size_t sv_ttk_files_count = sizeof(sv_ttk_files) / sizeof(sv_ttk_files[0]);

} // namespace detail
} // namespace custom
} // namespace cpp_tk
