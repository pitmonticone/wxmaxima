// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//  Copyright (C) 2020 Kuba Ober <kuba@mareimbrium.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//
//  SPDX-License-Identifier: GPL-2.0+

#ifndef TEXTSTYLE_H
#define TEXTSTYLE_H

#include "precomp.h"

/*! \file
 * This file declares everything needed for the text style system used
 * to style all the elements on the work sheet.
 */

#include <wx/colour.h>
#include <wx/config.h>
#include <wx/font.h>
#include <wx/settings.h>
#include <cstdint>
#include <functional>
#include "FontAttribs.h"
#include "FontVariantCache.h"

//! Returns a r,g,b components packed into a 32-bit 00bbggrr triple.
static constexpr uint32_t MAKE_RGB(uint32_t r, uint32_t g, uint32_t b)
{ return (0xFF & r) | ((0xFF & g) << 8) | ((0xFF & b) << 16); }

/*! Text Style Definition
 *
 * It is a well-performing replacement for wxFontInfo, with additional
 * color attribute.
 *
 * The text styles are also used as keys into the FontCache. 
 *
 */
class Style final
{
public:
  Style();
  explicit Style(AFontSize fontSize);
  Style(const Style &);

  Style &operator=(const Style &);
  bool operator==(const Style &o) const;

  /*! Read this style from a config source.
   * Only touches the attributes that were successfully read. Remaining attributes
   * are unchanged.
   */
  Style &Read(wxConfigBase *config, const wxString &where);
  //! Write this style to a config source
  void Write(wxConfigBase *config, const wxString &where) const;
  
  //! Gets a style that represents a given font. The font gets cached.
  static const Style &FromFont(const wxFont &font);
  //! Gets a style that represents a stock font. The font is pre-cached.
  static const Style &FromStockFont(wxStockGDI::Item font);

  constexpr static wxFontFamily Default_Family{wxFONTFAMILY_DEFAULT};
  constexpr static wxFontEncoding Default_Encoding{wxFONTENCODING_DEFAULT};
  constexpr static wxFontWeight Default_Weight{wxFONTWEIGHT_NORMAL};
  constexpr static wxFontStyle Default_FontStyle{wxFONTSTYLE_NORMAL};
  constexpr static bool Default_Underlined{false};
  constexpr static bool Default_Strikethrough{false};
  constexpr static AFontSize Default_FontSize{10.0f};
  constexpr static uint32_t Default_ColorRGB{MAKE_RGB(0, 0, 0)};
  static const wxColor &Default_Color();

  wxFontFamily GetFamily() const;
  wxFontEncoding GetEncoding() const;
  wxFontWeight GetWeight() const;
  bool IsBold() const { return GetWeight() == wxFONTWEIGHT_BOLD; }
  bool IsLight() const { return GetWeight() == wxFONTWEIGHT_LIGHT; }
  wxFontStyle GetFontStyle() const;
  bool IsItalic() const { return GetFontStyle() == wxFONTSTYLE_ITALIC; }
  bool IsSlant() const { return GetFontStyle() == wxFONTSTYLE_SLANT; }
  bool IsUnderlined() const;
  bool IsStrikethrough() const;
  const wxString &GetFontName() const;
  AFontSize GetFontSize() const;
  uint32_t GetRGBColor() const;
  wxColor GetColor() const { return wxColor(GetRGBColor()); }

  using did_change = bool;
  did_change SetFamily(wxFontFamily family);
  did_change SetEncoding(wxFontEncoding encoding);
  did_change SetWeight(wxFontWeight weight);
  did_change SetBold(bool bold = true);
  did_change SetLight(bool light = true);
  did_change SetFontStyle(wxFontStyle style);
  did_change SetItalic(bool italic = true);
  did_change SetSlant(bool slant = true);
  did_change SetUnderlined(bool underlined = true);
  did_change SetStrikethrough(bool strikethrough = true);
  did_change SetFontName(wxString fontName);
  did_change SetFontSize(AFontSize fontSize);
  did_change SetRGBColor(uint32_t rgb);
  did_change SetColor(const wxColor &color);
  did_change SetColor(wxSystemColour sysColour);

  Style& Family(wxFontFamily family) { return SetFamily(family), *this; }
  Style& Encoding(wxFontEncoding encoding) { return SetEncoding(encoding), *this; }
  Style& Weight(wxFontWeight weight) { return SetWeight(weight), *this; }
  Style& FontStyle(wxFontStyle style) { return SetFontStyle(style), *this; }
  Style& Bold(bool bold = true) { return SetBold(bold), *this; }
  Style& Light(bool light = true) { return SetLight(light), *this; }
  Style& Italic(bool italic = true) { return SetItalic(italic), *this; }
  Style& Slant(bool slant = true) { return SetSlant(slant), *this; }
  Style& Underlined(bool underlined = true) { return SetUnderlined(underlined), *this; }
  Style& Strikethrough(bool strikethrough = true) { return SetStrikethrough(strikethrough), *this; }
  Style& FontSize(float size) { return SetFontSize(AFontSize(size)), *this; }
  Style& FontSize(AFontSize fontSize) { return SetFontSize(fontSize), *this; }
  Style& RGBColor(uint32_t rgb) { return SetRGBColor(rgb), *this; }
  Style& Color(const wxColor &color) { return SetColor(color), *this; }
  Style& Color(uint8_t r, uint8_t g, uint8_t b) { return SetColor({r, g, b}), *this; }
  Style& Color(wxSystemColour sysColour) { return SetColor(sysColour), *this; }
  Style& ChangeLightness(int alpha) { return SetColor(GetColor().ChangeLightness(alpha)), *this; }

  bool CantChangeFontName() const {return m.cantChangeFontName;}
  bool CantChangeFontVariant() const {return m.cantChangeFontVariant;}
  void CantChangeFontName(bool changeForbidden) {m.cantChangeFontName = changeForbidden;}
  void CantChangeFontVariant(bool changeForbidden) {m.cantChangeFontVariant = changeForbidden;}
  wxFontInfo GetAsFontInfo() const;


  bool IsFontOk();
  wxFont GetFont() const;

  //! Sets all font-related properties based on another font
  did_change SetFromFont(const wxFont&);
  //! Sets all font-related properties based on another style, including size, font style and weight
  did_change SetFontFrom(const Style&);
  //! Sets font-face-only properties based on another style
  did_change SetFontFaceFrom(const Style&);
  //! Sets font-face and size only properties based on another style (not attributes like bold, etc.)
  did_change SetFontFaceAndSizeFrom(const Style&);

  constexpr static bool IsFractionalFontSizeSupported() {
    return wxCHECK_VERSION(3, 1, 2); } //-V686 //-V501
  static AFontSize GetFontSize(const wxFont &);
  static void SetFontSize(wxFont &, AFontSize fontSize);
  void ClearCache()
    {
      if(m.fontCache)
        m.fontCache->ClearCache();
    }
  std::shared_ptr<FontVariantCache> GetFontCache() const {return m.fontCache;}
private:
  WX_DECLARE_STRING_HASH_MAP( std::shared_ptr<FontVariantCache>,     // type of the values
                              FontVariantCachesMap); // name of the class

  static wxString m_emptyString;
  static FontVariantCachesMap m_fontCaches;

  struct Data
  {
    // 8/4-byte members
    mutable std::shared_ptr<FontVariantCache> fontCache;
    // 4-byte members
    uint32_t rgbColor = Default_ColorRGB;
    // 2-byte members
    AFontSize fontSize = Default_FontSize;
    AFontFamily family = Default_Family;
    AFontEncoding encoding = Default_Encoding;
    AFontWeight weight = Default_Weight;
    AFontStyle fontStyle = Default_FontStyle;
    // 1-byte members
    bool underlined : 1;
    bool strikethrough : 1;
    bool cantChangeFontName    : 1; // !< Allow to change only color, underline etc.
    bool cantChangeFontVariant : 1; // !< Allow to change only color

    Data() : underlined(false), strikethrough(false), cantChangeFontName(false),
             cantChangeFontVariant(false) {}
  } m;

  // cppcheck-suppress noExplicitConstructor
};


/*! All text styles known to wxMaxima
 *
 * \attention If this list is changed, the config dialogue
 * sometimes needs additional tweaking after that.
 */
enum TextStyle : int8_t
{
  TS_CODE_DEFAULT       , //<! The font code uses by default
  TS_CODE_VARIABLE      ,
  TS_CODE_FUNCTION      ,
  TS_CODE_COMMENT       ,
  TS_CODE_NUMBER        ,
  TS_CODE_STRING        ,
  TS_CODE_OPERATOR      ,
  TS_CODE_LISP          ,
  TS_CODE_ENDOFLINE     ,
  TS_ASCIIMATHS         ,
  TS_MATH               ,
  TS_TEXT               ,
  TS_VARIABLE           ,
  TS_NUMBER             ,
  TS_FUNCTION           ,
  TS_SPECIAL_CONSTANT   ,
  TS_GREEK_CONSTANT     ,
  TS_STRING             ,
  TS_INPUT              ,
  TS_OUTDATED           ,
  TS_MAIN_PROMPT        ,
  TS_OTHER_PROMPT       ,
  TS_LABEL              ,
  TS_USERLABEL          ,
  TS_HIGHLIGHT          ,
  TS_WARNING            ,
  TS_ERROR              ,
  TS_TITLE              ,
  TS_SECTION            ,
  TS_SUBSECTION         ,
  TS_SUBSUBSECTION      ,
  TS_HEADING5           ,
  TS_HEADING6           ,
  TS_TEXT_BACKGROUND    ,
  TS_DOCUMENT_BACKGROUND,
  TS_CELL_BRACKET       ,
  TS_ACTIVE_CELL_BRACKET,
  TS_CURSOR             ,
  TS_SELECTION          ,
  TS_EQUALSSELECTION    ,
  NUMBEROFSTYLES, //!< This is not a style, but its value tells us how many styles are defined
  TS_INVALID //!< If a text style cannot be determined this value is used
};

#endif // TEXTSTYLE_H
