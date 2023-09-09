// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode:
// nil -*-
//
//  Copyright (C) 2006-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2012 Doug Ilijev <doug.ilijev@gmail.com>
//            (C) 2014-2020 Gunter Königsmann <wxMaxima@physikbuch.de>
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

/*! \file
  This file defines the class EditorCell

  EditorCell is the Cell type that represents the field that contains user
  input.
*/

#include "EditorCell.h"

#include "CellImpl.h"
#include "CellPointers.h"
#include "MarkDown.h"
#include "wxMaxima.h"
#include "wxMaximaFrame.h"
#include <algorithm>
#include <wx/clipbrd.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include "RegexSearch.h"

EditorCell::EditorCell(GroupCell *group, Configuration *config,
                       const wxString &text)
  : Cell(group, config), m_text(text) {
  InitBitFields();
  m_text.Replace(wxS("\u2028"), "\n");
  m_text.Replace(wxS("\u2029"), "\n");
  SetValue(TabExpand(text, 0));
  m_height = m_charHeight + 2 * Scale_Px(2);
  m_center = m_height / 2;
  m_width = 2 * Scale_Px(2);
  SetStyle(TS_CODE_DEFAULT);
}

wxString EditorCell::EscapeHTMLChars(wxString input) {
  input.Replace(wxS("&"), wxS("&amp;"));
  input.Replace(wxS("\""), wxS("&quot;"));
  input.Replace(wxS("<"), wxS("&lt;"));
  input.Replace(wxS(">"), wxS("&gt;"));
  input.Replace(wxS("\n"), wxS("<br/>\n"));
  input.Replace(wxS("\r"), wxS(" "));
  return input;
}

DEFINE_CELL(EditorCell)

void EditorCell::AddDrawParameter(wxString param) {
  SaveValue();

  if (param == wxEmptyString)
    return;

  wxString paramTrimmed = param;
  paramTrimmed.Trim();
  if (paramTrimmed == wxEmptyString)
    return;

  size_t pos = 1;

  // Insert a comma in front of the parameter, if necessary
  wxString::const_iterator ch = m_text.begin();
  bool commaNeededBefore = false;
  bool commaNeededAfter = false;
  while (ch < m_text.end()) {
    if ((*ch == wxS('(')) || (*ch == wxS('[')) || (*ch == wxS(',')))
      commaNeededBefore = false;
    else {
      if (!((*ch == wxS(' ')) || (*ch == wxS('\n')) || (*ch == wxS('\r')) ||
            (*ch == wxS('\t'))))
        commaNeededBefore = true;
    }

    if (pos > GetSelection().CursorPosition())
      break;
    else {
      ++ch;
      ++pos;
    }
  }

  // if(ch < m_text.end())
  //  ++ch;

  while (ch < m_text.end()) {
    if ((*ch == wxS(')')) || (*ch == wxS(']')) || (*ch == wxS(','))) {
      commaNeededAfter = false;
      break;
    }
    if ((*ch != wxS(' ')) && (*ch != wxS('\n')) && (*ch != wxS('\r')) &&
        (*ch != wxS('\t'))) {
      commaNeededAfter = true;
      break;
    }

    ++ch;
    ++pos;
  }

  if (commaNeededAfter)
    param += wxS(",");

  wxString textAfterParameter =
    m_text.Right(m_text.Length() - GetSelection().CursorPosition());
  m_text = m_text.Left(GetSelection().CursorPosition());
  m_text.Trim();
  if (commaNeededBefore) {
    m_text += wxS(",");
    GetSelection().CursorMove(1);
  }

  wxStringTokenizer lines(param, wxS("\n"), wxTOKEN_RET_EMPTY_ALL);
  while (lines.HasMoreTokens()) {
    // Todo: Don't insert a newline if we are at the beginning of a line.
    ProcessNewline(false);
    wxString line = lines.GetNextToken();
    line.Trim(false);
    m_text += line;
    GetSelection().CursorMove(line.Length());
  }
  m_text += textAfterParameter;
  StyleText();
}

void EditorCell::SearchStartedHere(size_t index) const {
  m_cellPointers->m_cellSearchStartedIn = const_cast<EditorCell *>(this);
  m_cellPointers->m_indexSearchStartedAt = index;
}

void EditorCell::SearchStartedHere() const {
  m_cellPointers->m_cellSearchStartedIn = const_cast<EditorCell *>(this);
  m_cellPointers->m_indexSearchStartedAt = GetSelection().CursorPosition();
}

void EditorCell::MouseSelectionStartedHere() const {
  m_cellPointers->m_cellMouseSelectionStartedIn =
    const_cast<EditorCell *>(this);
}

void EditorCell::KeyboardSelectionStartedHere() const {
  m_cellPointers->m_cellKeyboardSelectionStartedIn =
    const_cast<EditorCell *>(this);
}

wxString EditorCell::GetFullCommandUnderCursor() {
  if (!IsActive())
    return wxEmptyString;

  if (m_text == wxEmptyString)
    return wxEmptyString;

  wxString result;
  size_t pos = 1;

  wxString::const_iterator ch = m_text.begin();
  while (ch < m_text.end()) {
    result += *ch;
    if (*ch == wxS('\\')) {
      ++ch;
      ++pos;
      if (ch < m_text.end())
        result += *ch;
    } else {
      if ((*ch == ';') || (*ch == '$')) {
        if (GetSelection().CursorPosition() < pos)
          return result;
        result = wxEmptyString;
      }
    }

    if (ch < m_text.end()) {
      ++ch;
      ++pos;
    }
  }
  return result;
}

wxString EditorCell::PrependNBSP(wxString input) {
  bool firstSpace = true;
  wxString retval;

  input.Replace(wxS("\r"), wxS(" "));

  for (size_t i = 0; i < input.Length(); i++) {
    wxChar ch = input.GetChar(i);
    if (ch == wxS('\n'))
      firstSpace = true;

    if (ch == wxS(' ')) {
      if (firstSpace) {
        firstSpace = false;
        retval += ch;
      } else
        retval += wxS("\u00A0");
    } else {
      retval += ch;
      firstSpace = true;
    }
  }
  return retval;
}

// cppcheck-suppress uninitMemberVar symbolName=EditorCell::m_wordList
// cppcheck-suppress uninitMemberVar symbolName=EditorCell::m_styledText
// cppcheck-suppress uninitMemberVar symbolName=EditorCell::m_textHistory
// cppcheck-suppress uninitMemberVar symbolName=EditorCell::m_positionHistory
// cppcheck-suppress uninitMemberVar symbolName=EditorCell::m_startHistory
// cppcheck-suppress uninitMemberVar symbolName=EditorCell::m_endHistory
// cppcheck-suppress uninitMemberVar symbolName=EditorCell::m_fontName
// cppcheck-suppress uninitMemberVar symbolName=EditorCell::m_tokens
EditorCell::EditorCell(GroupCell *group, const EditorCell &cell)
  : EditorCell(group, cell.m_configuration, cell.m_text) {
  CopyCommonData(cell);
}

wxString EditorCell::ToString() const { return ToString(false); }

wxString EditorCell::ToString(bool dontLimitToSelection) const {
  wxString text = m_text;
  // Remove all soft line breaks
  text.Replace(wxS('\r'), wxS(' '));
  // Convert non-breakable spaces to breakable ones
  text.Replace(wxS("\u00a0"), wxS(" "));

  if (SelectionActive() && (!dontLimitToSelection)) {
    auto start = GetSelection().Left();
    auto end   = GetSelection().Right();
    if (end >= m_text.Length())
      end = m_text.Length() - 1;
    if (start < 0)
      start = 0;
    text = m_text.SubString(start, end);
  }
  return text;
}

wxString EditorCell::ToMatlab() const { return ToMatlab(false); }

wxString EditorCell::ToMatlab(bool dontLimitToSelection) const {
  wxString text = m_text;
  // Remove all soft line breaks
  text.Replace(wxS('\r'), wxS(' '));
  // Convert non-breakable spaces to breakable ones
  text.Replace(wxS("\u00a0"), wxS(" "));

  if (SelectionActive() && (!dontLimitToSelection)) {
    auto start = GetSelection().Left();
    auto end   = GetSelection().Right();
    if (end >= m_text.Length())
      end = m_text.Length() - 1;
    if (start < 0)
      start = 0;
    text = m_text.SubString(start, end);
  }
  return text;
}

wxString EditorCell::ToRTF() const {
  wxString retval;

  switch (m_type) {
  case MC_TYPE_TITLE:
    retval += wxS("\\pard\\s16\\b\\f0\\fs56 ") + RTFescape(m_text) + wxS("\n");
    break;
  case MC_TYPE_SECTION:
    retval += wxS("\\pard\\s1\\b\\f0\\fs40 ") + RTFescape(m_text) + wxS("\n");
    break;
  case MC_TYPE_SUBSECTION:
    retval += wxS("\\pard\\s2\\b\\f0\\fs36 ") + RTFescape(m_text) + wxS("\n");
    break;
  case MC_TYPE_SUBSUBSECTION:
    retval += wxS("\\pard\\s3\\b\\f0\\fs32 ") + RTFescape(m_text) + wxS("\n");
    break;
  case MC_TYPE_HEADING5:
    retval += wxS("\\pard\\s4\\b\\f0\\fs32 ") + RTFescape(m_text) + wxS("\n");
    break;
  case MC_TYPE_HEADING6:
    retval += wxS("\\pard\\s5\\b\\f0\\fs32 ") + RTFescape(m_text) + wxS("\n");
    break;
  case MC_TYPE_PROMPT:
    retval += wxString::Format(wxS("\\cf%i"), GetTextStyle()) +
      wxS("\\pard\\s22\\li1105\\lin1105\\fi-1105\\f0\\fs24 ") +
      RTFescape(m_text) + wxS("\n");
    break;
  case MC_TYPE_INPUT: {
    retval += wxS(" ");
    for (const auto &textSnippet : m_styledText) {
      wxString text = RTFescape(textSnippet.GetText());

      if (textSnippet.IsStyleSet()) {
        retval +=
	  wxString::Format(wxS("\\cf%i "), static_cast<int>(textSnippet.GetTextStyle()));
        retval += RTFescape(textSnippet.GetText());
      } else {
        retval += wxString::Format(wxS("\\cf%i "), static_cast<int>(TS_CODE_DEFAULT));
        retval += wxS("{") + RTFescape(textSnippet.GetText()) + wxS("}\n");
      }
      if (textSnippet.GetText().Contains(wxS("\n"))) {
        retval += wxS("\\pard\\s21\\li1105\\lin1105\\f0\\fs24 ");
      }
    }
    retval += wxString::Format(wxS("\\cf%i "), static_cast<int>(TS_CODE_DEFAULT));
    break;
  }
  default:
    retval += wxS("\\pard\\s0 ") + RTFescape(m_text);
    break;
  }
  return retval;
}

wxString EditorCell::ToTeX() const {
  wxString text = m_text;
  if (!text.StartsWith(wxS("TeX:"))) {
    text.Replace(wxS("\u00a0"), wxS("~"));
    text.Replace(wxS("\\"), wxS("\\ensuremath{\\backslash}"));
    text.Replace(wxS("\r"), wxS(" "));
    text.Replace(wxS("^"), wxS("\\^ "));
    text.Replace(wxS("\u00B0"), wxS("\\ensuremath{^\\circ}"));
    text.Replace(wxS("\u2212"), wxS("-")); // unicode minus sign
    text.Replace(wxS("\u2052"), wxS("-")); // commercial minus sign
    text.Replace(wxS("\uFE63"), wxS("-")); // unicode small minus sign
    text.Replace(wxS("\uFF0D"), wxS("-")); // unicode big minus sign
    text.Replace(wxS("\uFF0B"), wxS("+")); // unicode big plus
    text.Replace(wxS("\uFB29"), wxS("+")); // hebrew alternate plus
    text.Replace(wxS("\u03B1"), wxS("\\ensuremath{\\alpha}"));
    text.Replace(wxS("\u00B1"), wxS("\\ensuremath{\\pm}"));
    text.Replace(wxS("\u00B2"), wxS("\\ensuremath{^2}"));
    text.Replace(wxS("\u00B3"), wxS("\\ensuremath{^3}"));
    text.Replace(wxS("\u221A"), wxS("\\ensuremath{\\sqrt{}}"));
    text.Replace(wxS("\u2148"), wxS("\\ensuremath{\\mathbbm{i}}"));
    text.Replace(wxS("\u2147"), wxS("\\ensuremath{\\mathbbm{e}}"));
    text.Replace(wxS("\u210f"), wxS("\\ensuremath{\\hbar}"));
    text.Replace(wxS("\u2203"), wxS("\\ensuremath{\\exists}"));
    text.Replace(wxS("\u2204"), wxS("\\ensuremath{\\nexists}"));
    text.Replace(wxS("\u2208"), wxS("\\ensuremath{\\in}"));
    text.Replace(wxS("\u21D2"), wxS("\\ensuremath{\\Longrightarrow}"));
    text.Replace(wxS("\u221e"), wxS("\\ensuremath{\\infty}"));
    text.Replace(wxS("\u22C0"), wxS("\\ensuremath{\\wedge}"));
    text.Replace(wxS("\u22C1"), wxS("\\ensuremath{\\vee}"));
    text.Replace(wxS("\u22bb"), wxS("\\ensuremath{\\oplus}"));
    text.Replace(wxS("\u22BC"), wxS("\\ensuremath{\\overline{\\wedge}}"));
    text.Replace(wxS("\u22BB"), wxS("\\ensuremath{\\overline{\\vee}}"));
    text.Replace(wxS("\u00AC"), wxS("\\ensuremath{\\setminus}"));
    text.Replace(wxS("\u22C3"), wxS("\\ensuremath{\\cup}"));
    text.Replace(wxS("\u22C2"), wxS("\\ensuremath{\\cap}"));
    text.Replace(wxS("\u2286"), wxS("\\ensuremath{\\subseteq}"));
    text.Replace(wxS("\u2282"), wxS("\\ensuremath{\\subset}"));
    text.Replace(wxS("\u2288"), wxS("\\ensuremath{\\not\\subseteq}"));
    text.Replace(wxS("\u0127"), wxS("\\ensuremath{\\hbar}"));
    text.Replace(wxS("\u0126"), wxS("\\ensuremath{\\Hbar}"));
    text.Replace(wxS("\u2205"), wxS("\\ensuremath{\\emptyset}"));
    text.Replace(wxS("\u00BD"), wxS("\\ensuremath{\\frac{1}{2}}"));
    text.Replace(wxS("\u03B2"), wxS("\\ensuremath{\\beta}"));
    text.Replace(wxS("\u03B3"), wxS("\\ensuremath{\\gamma}"));
    text.Replace(wxS("\u03B4"), wxS("\\ensuremath{\\delta}"));
    text.Replace(wxS("\u03B5"), wxS("\\ensuremath{\\epsilon}"));
    text.Replace(wxS("\u03B6"), wxS("\\ensuremath{\\zeta}"));
    text.Replace(wxS("\u03B7"), wxS("\\ensuremath{\\eta}"));
    text.Replace(wxS("\u03B8"), wxS("\\ensuremath{\\theta}"));
    text.Replace(wxS("\u03B9"), wxS("\\ensuremath{\\iota}"));
    text.Replace(wxS("\u03BA"), wxS("\\ensuremath{\\kappa}"));
    text.Replace(wxS("\u03BB"), wxS("\\ensuremath{\\lambda}"));
    text.Replace(wxS("\u03BC"), wxS("\\ensuremath{\\mu}"));
    text.Replace(wxS("\u03BD"), wxS("\\ensuremath{\\nu}"));
    text.Replace(wxS("\u03BE"), wxS("\\ensuremath{\\xi}"));
    text.Replace(wxS("\u03BF"), wxS("o"));
    text.Replace(wxS("\u03C0"), wxS("\\ensuremath{\\pi}"));
    text.Replace(wxS("\u03C1"), wxS("\\ensuremath{\\rho}"));
    text.Replace(wxS("\u03C3"), wxS("\\ensuremath{\\sigma}"));
    text.Replace(wxS("\u03C4"), wxS("\\ensuremath{\\tau}"));
    text.Replace(wxS("\u03C5"), wxS("\\ensuremath{\\upsilon}"));
    text.Replace(wxS("\u03C6"), wxS("\\ensuremath{\\phi}"));
    text.Replace(wxS("\u03C7"), wxS("\\ensuremath{\\chi}"));
    text.Replace(wxS("\u03C8"), wxS("\\ensuremath{\\psi}"));
    text.Replace(wxS("\u03C9"), wxS("\\ensuremath{\\omega}"));
    text.Replace(wxS("\u0391"), wxS("A"));
    text.Replace(wxS("\u0392"), wxS("B"));
    text.Replace(wxS("\u0393"), wxS("\\ensuremath{\\Gamma}"));
    text.Replace(wxS("\u0394"), wxS("\\ensuremath{\\Delta}"));
    text.Replace(wxS("\u0395"), wxS("E"));
    text.Replace(wxS("\u0396"), wxS("Z"));
    text.Replace(wxS("\u0397"), wxS("H"));
    text.Replace(wxS("\u0398"), wxS("\\ensuremath{\\Theta}"));
    text.Replace(wxS("\u0399"), wxS("I"));
    text.Replace(wxS("\u039A"), wxS("K"));
    text.Replace(wxS("\u039B"), wxS("\\ensuremath{\\Lambda}"));
    text.Replace(wxS("\u039C"), wxS("M"));
    text.Replace(wxS("\u039D"), wxS("N"));
    text.Replace(wxS("\u039E"), wxS("\\ensuremath{\\Xi}"));
    text.Replace(wxS("\u039F"), wxS("O"));
    text.Replace(wxS("\u03A0"), wxS("\\ensuremath{\\Pi}"));
    text.Replace(wxS("\u03A1"), wxS("P"));
    text.Replace(wxS("\u03A3"), wxS("\\ensuremath{\\Sigma}"));
    text.Replace(wxS("\u03A4"), wxS("T"));
    text.Replace(wxS("\u03A5"), wxS("\\ensuremath{\\Upsilon}"));
    text.Replace(wxS("\u03A6"), wxS("\\ensuremath{\\Phi}"));
    text.Replace(wxS("\u03A7"), wxS("X"));
    text.Replace(wxS("\u03A8"), wxS("\\ensuremath{\\Psi}"));
    text.Replace(wxS("\u03A9"), wxS("\\ensuremath{\\Omega}"));
    text.Replace(wxS("\u2202"), wxS("\\ensuremath{\\partial}"));
    text.Replace(wxS("\u222b"), wxS("\\ensuremath{\\int}"));
    text.Replace(wxS("\u2245"), wxS("\\ensuremath{\\approx}"));
    text.Replace(wxS("\u221d"), wxS("\\ensuremath{\\propto}"));
    text.Replace(wxS("\u2260"), wxS("\\ensuremath{\\neq}"));
    text.Replace(wxS("\u2264"), wxS("\\ensuremath{\\leq}"));
    text.Replace(wxS("\u2265"), wxS("\\ensuremath{\\geq}"));
    text.Replace(wxS("\u226A"), wxS("\\ensuremath{\\ll}"));
    text.Replace(wxS("\u226B"), wxS("\\ensuremath{\\gg}"));
    text.Replace(wxS("\u220e"), wxS("\\ensuremath{\\blacksquare}"));
    text.Replace(wxS("\u2263"), wxS("\\ensuremath{\\equiv}"));
    text.Replace(wxS("\u2211"), wxS("\\ensuremath{\\sum}"));
    text.Replace(wxS("\u220F"), wxS("\\ensuremath{\\prod}"));
    text.Replace(wxS("\u2225"), wxS("\\ensuremath{\\parallel}"));
    text.Replace(wxS("\u27C2"), wxS("\\ensuremath{\\bot}"));
    text.Replace(wxS("~"), wxS("\\ensuremath{\\sim }"));
    text.Replace(wxS("_"), wxS("\\_"));
    text.Replace(wxS("$"), wxS("\\$"));
    text.Replace(wxS("%"), wxS("\\%"));
    text.Replace(wxS("&"), wxS("\\&"));
    text.Replace(wxS("@"), wxS("\\ensuremath{@}"));
    text.Replace(wxS("#"), wxS("\\ensuremath{\\neq}"));
    text.Replace(wxS("\u00A0"), wxS("~")); // A non-breakable space
    text.Replace(wxS("<"), wxS("\\ensuremath{<}"));
    text.Replace(wxS(">"), wxS("\\ensuremath{>}"));
    text.Replace(wxS("\u219D"), wxS("\\ensuremath{\\leadsto}"));
    text.Replace(wxS("\u2192"), wxS("\\ensuremath{\\rightarrow}"));
    text.Replace(wxS("\u27F6"), wxS("\\ensuremath{\\longrightarrow}"));
    // Now we might want to introduce some markdown:
    MarkDownTeX MarkDown(m_configuration);
    if (m_type != MC_TYPE_INPUT)
      text = MarkDown.MarkDown(text);
    else {
      text.Replace(wxS("\n"), wxS("\\\\\n"));
      text.Replace(wxS(" "), wxS("\\ "));
    }
  } else {
    text = text.Mid(5, text.Length());
  }
  return text;
}

wxString EditorCell::ToXML() const {
  wxString xmlstring = m_text;
  // convert it, so that the XML parser doesn't fail
  xmlstring.Replace(wxS("&"), wxS("&amp;"));
  xmlstring.Replace(wxS("<"), wxS("&lt;"));
  xmlstring.Replace(wxS(">"), wxS("&gt;"));
  xmlstring.Replace(wxS("'"), wxS("&apos;"));
  xmlstring.Replace(wxS("\""), wxS("&quot;"));
  xmlstring.Replace(wxS("\n"), wxS("</line>\n<line>"));
  xmlstring.Replace(wxS("\r"), wxS(" "));
  xmlstring = wxS("<line>") + xmlstring + wxS("</line>\n");
  wxString head = wxS("<editor");
  switch (m_type) {
  case MC_TYPE_TEXT:
    head += wxS(" type=\"text\"");
    break;
  case MC_TYPE_TITLE:
    head += wxS(" type=\"title\" sectioning_level=\"1\"");
    break;
  case MC_TYPE_SECTION:
    head += wxS(" type=\"section\" sectioning_level=\"2\"");
    break;
  case MC_TYPE_SUBSECTION:
    head += wxS(" type=\"subsection\" sectioning_level=\"3\"");
    break;
  case MC_TYPE_SUBSUBSECTION:
    // We save subsubsections as subsections with a higher sectioning level:
    // This makes them backwards-compatible in the way that they are displayed
    // as subsections on old wxMaxima installations.
    head += wxS(" type=\"subsection\" sectioning_level=\"4\"");
    break;
  case MC_TYPE_HEADING5:
    head += wxS(" type=\"subsection\" sectioning_level=\"5\"");
    break;
  case MC_TYPE_HEADING6:
    head += wxS(" type=\"subsection\" sectioning_level=\"6\"");
    break;
  case MC_TYPE_INPUT:
  default:
    head += wxS(" type=\"input\"");
    break;
  }
  head += wxS(">\n");

  return head + xmlstring + wxS("</editor>\n");
}

void EditorCell::ConvertNumToUNicodeChar() {
  if (GetSelection().CursorPosition() = 0)
    return;
  int numLen = 0;
  while ((GetSelection().CursorPosition() > 0) &&
         (((m_text[GetSelection().CursorPosition() - 1] >= '0') &&
           (m_text[GetSelection().CursorPosition() - 1] <= '9')) ||
          ((m_text[GetSelection().CursorPosition() - 1] >= 'a') &&
           (m_text[GetSelection().CursorPosition() - 1] <= 'f')) ||
          ((m_text[GetSelection().CursorPosition() - 1] >= 'A') &&
           (m_text[GetSelection().CursorPosition() - 1] <= 'F')))) {
    numLen++;
    GetSelection().CursorMove(-1);
  }

  wxString numString =
    m_text.SubString(GetSelection().CursorPosition(),
		     GetSelection().CursorPosition() + numLen - 1);
  long number;
  if (!numString.ToLong(&number, 16))
    return;

  wxString newChar;
  {
    wxLogNull suppressConversationErrors;
    newChar = wxChar(number);
  }
  m_text = m_text.Left(GetSelection().CursorPosition()) + newChar +
    m_text.Right(m_text.Length() - GetSelection().CursorPosition() - numLen);
  GetSelection().StepCursor(newChar.Length());
}

bool EditorCell::IsZoomFactorChanged() const {
  double constexpr eps = 0.04;
  double diff = m_configuration->GetZoomFactor() - m_lastZoomFactor;
  return diff < -eps || diff > eps;
}

bool EditorCell::NeedsRecalculation(AFontSize fontSize) const {
  return Cell::NeedsRecalculation(fontSize) || m_containsChanges || m_isDirty;
}

void EditorCell::Recalculate(AFontSize fontsize) {
  if(NeedsRecalculation(fontsize))
    {
      Cell::Recalculate(fontsize);
      m_isDirty = false;
      if (IsZoomFactorChanged()) {
	m_widths.clear();
	m_lastZoomFactor = m_configuration->GetZoomFactor();
      }
      StyleText();
      SetFont(m_configuration->GetRecalcDC());

      // Measure the text height using characters that might extend below or above
      // the region ordinary characters move in.
      wxCoord charWidth;
      m_configuration->GetRecalcDC()->GetTextExtent(wxS("äXÄgy"), &charWidth, &m_charHeight);

      // We want a little bit of vertical space between two text lines (and between
      // two labels).
      m_charHeight += 2 * MC_TEXT_PADDING;
      wxCoord width = 0, tokenwidth, tokenheight, linewidth = 0;

      m_numberOfLines = 1;

      for (auto &textSnippet : m_styledText) {
	if ((textSnippet.GetText().StartsWith(wxS('\n')) ||
	     (textSnippet.GetText().StartsWith(wxS('\r'))))) {
	  m_numberOfLines++;
	  linewidth = textSnippet.GetIndentPixels();
	} else {
	  m_configuration->GetRecalcDC()->GetTextExtent(textSnippet.GetText(), &tokenwidth, &tokenheight);
	  textSnippet.SetWidth(tokenwidth);
	  linewidth += tokenwidth;
	  width = wxMax(width, linewidth);
	}

	// Handle folding
	if (m_firstLineOnly)
	  m_numberOfLines = 1;

	// Assign empty lines a minimum width
	if (m_text == wxEmptyString)
	  width = charWidth;

	// Add a line border
	m_width = width + 2 * Scale_Px(2);

	// Calculate the cell height
	if (m_firstLineOnly)
	  m_height = m_charHeight + 2 * Scale_Px(2);
	else
	  m_height = m_numberOfLines * m_charHeight + 2 * Scale_Px(2);

	if (m_height < m_charHeight + 2 * Scale_Px(2))
	  m_height = (m_charHeight) + 2 * Scale_Px(2);

	// The center lies in the middle of the 1st line
	m_center = m_charHeight / 2;
      }
      m_containsChanges = false;
    }
}

wxString EditorCell::ToHTML() const {
  wxString retval;

  for (const auto &tmp : OnList(this)) {
    for (const auto &textSnippet : tmp.m_styledText) {
      wxString text = PrependNBSP(EscapeHTMLChars(textSnippet.GetText()));

      if (textSnippet.IsStyleSet()) {
        switch (textSnippet.GetTextStyle()) {
        case TS_CODE_COMMENT:
          retval +=
	    wxS("<span class=\"code_comment\">") + text + wxS("</span>");
          break;
        case TS_CODE_VARIABLE:
          retval +=
	    wxS("<span class=\"code_variable\">") + text + wxS("</span>");
          break;
        case TS_CODE_FUNCTION:
          retval +=
	    wxS("<span class=\"code_function\">") + text + wxS("</span>");
          break;
        case TS_CODE_NUMBER:
          retval += wxS("<span class=\"code_number\">") + text + wxS("</span>");
          break;
        case TS_CODE_STRING:
          retval += wxS("<span class=\"code_string\">") + text + wxS("</span>");
          break;
        case TS_CODE_OPERATOR:
          retval +=
	    wxS("<span class=\"code_operator\">") + text + wxS("</span>");
          break;
        case TS_CODE_LISP:
          retval += wxS("<span class=\"code_lisp\">") + text + wxS("</span>");
          break;
        case TS_CODE_ENDOFLINE:
        default:
          retval +=
	    wxS("<span class=\"code_endofline\">") + text + wxS("</span>");
          break;
        }
      } else
        retval += text;
    }
  }
  return retval;
}

void EditorCell::MarkSelection(wxDC *dc, size_t start, size_t end, TextStyle style) {
  if ((start < 0) || (end < 0))
    return;
  wxPoint point, point1;
  size_t pos1 = start, pos2 = start;

#if defined(__WXOSX__)
    dc->SetPen(wxNullPen); // no border on rectangles
#else
    dc->SetPen(*(wxThePenList->FindOrCreatePen(
					       m_configuration->GetColor(style), 1, wxPENSTYLE_SOLID)));
  // window linux, set a pen
#endif
    dc->SetBrush(*(wxTheBrushList->FindOrCreateBrush(
						     m_configuration->GetColor(style)))); // highlight c.
  while (pos1 <
         end) // go through selection, draw a rect for each line of selection
    {
      while (pos1 < end && m_text.GetChar(pos1) != '\n' &&
	     m_text.GetChar(pos1) != '\r')
	pos1++;

      point = PositionToPoint(pos2);  // left  point
      point1 = PositionToPoint(pos1); // right point
      size_t selectionWidth = point1.x - point.x;
      wxRect rect;
#if defined(__WXOSX__)
      rect = GetRect(); // rectangle representing the cell
      if (pos1 !=
	  end) // we have a \n, draw selection to the right border (mac behaviour)
	selectionWidth = rect.GetRight() - point.x;
#endif

      rect = wxRect(point.x, point.y + Scale_Px(1) - m_center, selectionWidth,
		    m_charHeight);
      // draw the rectangle if it is in the region that is to be updated.
      if (m_configuration->InUpdateRegion(rect))
	dc->DrawRectangle(CropToUpdateRegion(rect));
      pos1++;
      pos2 = pos1;
    }
}

/* Draws the editor cell including selection and cursor

   The order this cell is drawn is:
   1. draw selection (wxCOPY), TS_SELECTION color
   2. mark matching parenthesis (wxCOPY), TS_SELECTION color
   3. draw all text (wxCOPY)
   4. draw the caret (wxCOPY), TS_CURSOR color

   The text is not taken from m_text but from the list of styled text snippets
   StyleText() converts m_text into. This way the decisions needed for styling
   text are cached for later use.
*/
void EditorCell::Draw(wxPoint point, wxDC *dc, wxDC *antialiassingDC) {
  Cell::Draw(point, dc, antialiassingDC);

  if (!IsHidden() && (DrawThisCell())) {
    wxRect rect = GetRect();
    wxCoord y = rect.GetY();
    if((m_height < 1) || (m_width < 1) || (y < 0))
      return;
    
    // Set the background to the cell's background color
    if (GetTextStyle() == TS_TEXT) {
      if (m_configuration->InUpdateRegion(rect) &&
	  (m_configuration->EditorBackgroundColor() !=
	   m_configuration->DefaultBackgroundColor()))
	{
	  wxBrush *br;
	  br = wxTheBrushList->FindOrCreateBrush(m_configuration->EditorBackgroundColor());
	  dc->SetBrush(*br);
	  dc->SetPen(*wxTRANSPARENT_PEN);
	  auto width = m_configuration->GetCanvasSize().GetWidth() - rect.x;
	  rect.SetWidth(width);
	  dc->DrawRectangle(CropToUpdateRegion(rect));
	}
    }
    SetFont(dc);

    m_selectionChanged = false;

    //
    // Mark text that coincides with the selection
    //
    if (!m_cellPointers->m_selectionString.IsEmpty()) {
      size_t start = 0;
      wxString text(m_text);
      text.Replace(wxS('\r'), wxS(' '));
      while ((start = text.find(m_cellPointers->m_selectionString, start)) !=
             wxNOT_FOUND) {
        size_t end = start + m_cellPointers->m_selectionString.Length();

        // Mark only text that won't be marked in the next step:
        // This would not only be unnecessary but also could cause
        // selections to flicker in very long texts
        if ((!IsActive()) || (start != GetSelection().Left()))
          MarkSelection(dc, start, end, TS_EQUALSSELECTION);
        if (m_cellPointers->m_selectionString.Length() == 0)
          end++;
        start = end;
      }
    }

    if (IsActive()) // draw selection or matching parens
      {
	//
	// Mark selection
	//
	if (GetSelection().IsActive())
	  MarkSelection(dc, GetSelection().Left(),
			GetSelection().Right(), TS_SELECTION);

	//
	// Matching parens - draw only if we don't have selection
	//
	else if ((m_paren1 != -1 && m_paren2 != -1) &&
		 (m_configuration->ShowMatchingParens())) {
	  {

#if defined(__WXOSX__)
		dc->SetPen(wxNullPen); // no border on rectangles
#else
		dc->SetPen(*(wxThePenList->FindOrCreatePen(m_configuration->GetColor(TS_SELECTION), 1,
							   wxPENSTYLE_SOLID))); // window linux, set a pen
#endif
		dc->SetBrush(*(wxTheBrushList->FindOrCreateBrush(m_configuration->GetColor(TS_SELECTION)))); // highlight c.
	  }
	  wxPoint matchPoint = PositionToPoint(m_paren1);
	  wxCoord width, height;
	  dc->GetTextExtent(m_text.GetChar(m_paren1), &width, &height);
	  wxRect matchRect(matchPoint.x + 1,
			   matchPoint.y + Scale_Px(2) - m_center + 1, width - 1,
			   height - 1);
	  if (m_configuration->InUpdateRegion(matchRect))
	    dc->DrawRectangle(CropToUpdateRegion(matchRect));
	  matchPoint = PositionToPoint(m_paren2);
	  dc->GetTextExtent(m_text.GetChar(m_paren1), &width, &height);
	  matchRect =
            wxRect(matchPoint.x + 1, matchPoint.y + Scale_Px(2) - m_center + 1,
                   width - 1, height - 1);
	  if (m_configuration->InUpdateRegion(matchRect))
	    dc->DrawRectangle(CropToUpdateRegion(matchRect));
	} // else if (m_paren1 != -1 && m_paren2 != -1)
      }   // if (IsActive())

    //
    // Draw the text
    //

    wxRect updateRegion = m_configuration->GetUpdateRegion();
    wxPoint TextStartingpoint = point;
    wxPoint TextCurrentPoint = TextStartingpoint;
    int lastStyle = -1;
    size_t lastIndent = 0;
    for (auto &textSnippet : m_styledText) {
      wxCoord width;

      // A newline is a separate token.
      if ((textSnippet.GetText() == wxS("\n")) || (textSnippet.GetText() == wxS("\r"))) {
        if ((textSnippet.GetText() == wxS("\n")))
          lastIndent = textSnippet.GetIndentPixels();

        // A newline =>
        // set the point to the beginning of the next line.
        TextCurrentPoint.x = TextStartingpoint.x;
        TextCurrentPoint.y += m_charHeight;
        TextCurrentPoint.x += textSnippet.GetIndentPixels();
      } else {
        // We need to draw some text.

        // Grab a pen of the right color.
	if (lastStyle != textSnippet.GetTextStyle()) {
	  dc->SetTextForeground(m_configuration->GetColor(textSnippet.GetTextStyle()));
	  lastStyle = textSnippet.GetTextStyle();
	}

        // Draw a char that shows we continue an indentation - if this is
        // needed.
        if (!textSnippet.GetIndentChar().IsEmpty())
          dc->DrawText(textSnippet.GetIndentChar(),
                       TextStartingpoint.x + lastIndent,
                       TextCurrentPoint.y - m_center);

        // Determine the box the will be is in.
        if (!textSnippet.SizeKnown()) {
	  wxCoord height;
          dc->GetTextExtent(textSnippet.GetText(), &width, &height);
          textSnippet.SetWidth(width);
        } else
          width = textSnippet.GetWidth();
        wxRect textRect(TextCurrentPoint.x, TextCurrentPoint.y - m_center,
                        TextCurrentPoint.x + width,
                        TextCurrentPoint.y - m_center + m_charHeight);

        // Draw the text only if it overlaps the update region
        if (((!m_configuration->ClipToDrawRegion())) ||
            (updateRegion.Intersects(textRect)))
          dc->DrawText(textSnippet.GetText(), TextCurrentPoint.x,
                       TextCurrentPoint.y - m_center);
        TextCurrentPoint.x += width;
      }
    }
    //
    // Draw the caret
    //

    if (m_displayCaret && m_hasFocus && IsActive()) {
      size_t caretInLine = 0;
      size_t caretInColumn = 0;

      PositionToXY(GetSelection().CursorPosition(), &caretInColumn, &caretInLine);

      size_t lineWidth = GetLineWidth(caretInLine, caretInColumn);

      dc->SetPen(*(wxThePenList->FindOrCreatePen(
						 m_configuration->GetColor(TS_CURSOR), 1, wxPENSTYLE_SOLID)));
      dc->SetBrush(*(wxTheBrushList->FindOrCreateBrush(
						       m_configuration->GetColor(TS_CURSOR), wxBRUSHSTYLE_SOLID)));
#if defined(__WXOSX__)
      // draw 1 pixel shorter caret than on windows
      dc->DrawRectangle(
			point.x + lineWidth - m_configuration->GetCursorWidth(),
			point.y + Scale_Px(1) - m_center + caretInLine * m_charHeight,
			m_configuration->GetCursorWidth(), m_charHeight - Scale_Px(5));
#else
      dc->DrawRectangle(
			point.x + +lineWidth - m_configuration->GetCursorWidth() / 2,
			point.y + Scale_Px(2) - m_center + caretInLine * m_charHeight,
			m_configuration->GetCursorWidth(), m_charHeight - Scale_Px(3));
#endif
    }
  }
}

void EditorCell::SetType(CellType type) {
  m_widths.clear();
  Cell::SetType(type);
}

void EditorCell::SetStyle(TextStyle style) {
  m_widths.clear();
  Cell::SetStyle(style);
}

void EditorCell::SetFont(wxDC *dc) const {
  if(!dc)
    return;
  const wxFont &font = GetFont();
  if(!dc->GetFont().IsSameAs(font))
    dc->SetFont(font);
}

wxSize EditorCell::GetTextSize(wxString const &text) {
  wxDC *dc = m_configuration->GetRecalcDC();
  StringHash::const_iterator it = m_widths.find(text);

  // If we already know this text piece's size we return the cached value
  if (it != m_widths.end())
    return it->second;

  // Ask wxWidgets to return this text piece's size (slow!)
  wxSize sz = dc->GetTextExtent(text);
  m_widths[text] = sz;
  return sz;
}

void EditorCell::SetForeground(wxDC *dc) {
  dc->SetTextForeground(m_configuration->GetColor(GetTextStyle()));
}

wxString EditorCell::GetCurrentCommand() {
  // Discard all chars behind the cursor.
  wxString lineTillCursor = m_text.Left(GetSelection().CursorPosition());

  wxString command;
  wxString possibleCommand;
  wxString::const_iterator it = lineTillCursor.begin();
  while (it != lineTillCursor.end()) {
    if (wxIsalpha(*it) || (*it == wxS('_')) || (*it == wxS('\\'))) {
      if (*it == '\\') {
        possibleCommand += *it;
        ++it;
      }
      if (it != lineTillCursor.end()) {
        possibleCommand += *it;
        ++it;
      }
      while ((it != lineTillCursor.end()) &&
             ((wxIsalnum(*it) || (*it == wxS('_')) || (*it == wxS('\\'))))) {
        if (*it == '\\') {
          possibleCommand += *it;
          ++it;
        }
        if (it != lineTillCursor.end()) {
          possibleCommand += *it;
          ++it;
        }
      }
    } else
      switch (wxChar(*it)) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        while ((it != lineTillCursor.end()) &&
               ((*it == wxS(' ')) || (*it == wxS('\t')) || (*it == wxS('\n')) ||
                (*it == wxS('\r'))))
          ++it;
        if ((it != lineTillCursor.end()) && (*it == wxS('('))) {
          command = possibleCommand;
          possibleCommand = wxEmptyString;
          ++it;
        }
        break;
      case '(':
        if ((possibleCommand != wxEmptyString))
          command = possibleCommand;
        ++it;
        break;
      case '$':
      case ';': {
        command = wxEmptyString;
        possibleCommand = wxEmptyString;
        ++it;
        break;
      }
      default:
        possibleCommand = wxEmptyString;
        ++it;
        break;
      }
  }
  return command;
}

wxString EditorCell::TabExpand(const wxString &input_, size_t posInLine) {
  if (posInLine < 0)
    posInLine = 0;
  wxString retval;

  // Convert the text to our line endings.
  wxString input =
    input_; // TODO the state machine below can be changed instead
  input.Replace(wxS("\r\n"), wxS("\n"));

  wxString::const_iterator ch = input.begin();
  while (ch < input.end()) {
    if ((*ch == wxS('\n'))) {
      posInLine = 0;
      retval += *ch;
      ++ch;
      continue;
    }

    if (*ch == wxS('\t')) {
      switch (posInLine - (posInLine / 4) * 4) {
      case 0:
        retval += wxS("    ");
        break;
      case 1:
        retval += wxS("   ");
        break;
      case 2:
        retval += wxS("  ");
        break;
      case 3:
        retval += wxS(" ");
        break;
      }
      posInLine = 0;
      ++ch;
      continue;
    } else
      retval += *ch;
    if (ch < input.end()) {
      ++ch;
      ++posInLine;
    }
  }
  // TODO: Implement the actual TAB expansion
  return retval;
}

size_t EditorCell::BeginningOfLine(size_t pos) const {
  if (m_text.empty())
    return 0;
  if (pos > m_text.size())
    pos = m_text.size();
  if (pos > 0)
    pos--;
  if (pos < 0)
    pos = 0;
  while (pos > 0) {
    if ((m_text[pos] == wxS('\n')) || (m_text[pos] == wxS('\r')))
      break;
    pos--;
  }
  if ((m_text[pos] == wxS('\n')) || (m_text[pos] == wxS('\r')))
    pos++;
  return pos;
}

size_t EditorCell::EndOfLine(size_t pos) {
  if (pos < 0)
    pos = 0;
  while (pos < m_text.length() && m_text[pos] != wxS('\n') &&
         m_text[pos] != wxS('\r'))
    pos++;
  return pos;
}

#if defined __WXOSX__

bool EditorCell::HandleCtrlCommand(wxKeyEvent &ev) {
  int code = ev.GetKeyCode();
  bool done = true;

  if (code >= 32)
    return false;

  code = code + 'A' - 1;

  switch (code) {
  case 'K': {
    ClearSelection();
    SaveValue();
    size_t end = EndOfLine(GetSelection().CursorPosition());
    if (end == GetSelection().CursorPosition())
      end++;
    m_text = m_text.SubString(0, GetSelection().CursorPosition() - 1) +
      m_text.SubString(end, m_text.length());
    m_isDirty = true;
    break;
  }

  case 'E': {
    ClearSelection();
    size_t end = EndOfLine(GetSelection().CursorPosition());
    if (ev.ShiftDown())
      GetSelection().SetSelection(GetSelection().CursorPosition(), end);
    GetSelection().CursorPosition() = end;
    m_displayCaret = true;
    break;
  }

  case 'A': {
    ClearSelection();
    size_t start = BeginningOfLine(GetSelection().CursorPosition());
    if (ev.ShiftDown())
      GetSelection().SetSelection(start, GetSelection().CursorPosition());
    GetSelection().CursorPosition(start);
    m_displayCaret = true;
    break;
  }

  default:
    done = false;
    break;
  }

  return done;
}

#endif // __WXOSX__

void EditorCell::ProcessEvent(wxKeyEvent &event) {
  bool done;
#ifdef __WXOSX__
  done = HandleCtrlCommand(event);
  if (!done)
#endif
    done = HandleSpecialKey(event);

  if ((!done) && (wxIsprint(event.GetUnicodeKey())))
    HandleOrdinaryKey(event);

  if (m_type == MC_TYPE_INPUT)
    FindMatchingParens();

  if (m_isDirty)
    StyleText();
  m_displayCaret = true;
}

size_t EditorCell::GetIndentDepth(wxString text, size_t positionOfCaret) {
  // Don't indent parenthesis that aren't part of code cells.
  if (m_type != MC_TYPE_INPUT)
    return 0;

  // A list of by how many chars we need to indent the current line.
  std::vector<size_t> indentChars;
  indentChars.push_back(0);

  wxString::const_iterator it = m_text.begin();

  // Determine how many parenthesis this cell opens or closes before the point
  size_t pos = 0;
  while ((pos < positionOfCaret) && (it < m_text.end())) {
    wxChar ch = *it;
    if (ch == wxS('\\')) {
      ++pos;
      ++it;
      continue;
    }

    if (ch == wxS('\"')) {
      ++pos;
      ++it;
      while ((it < m_text.end()) && (pos < positionOfCaret) &&
             (*it != wxS('\"'))) {
        ++pos;
        ++it;
      }
    }

    if ((ch == wxS('(')) || (ch == wxS('[')) || (ch == wxS('{'))) {
      if (indentChars.empty())
        indentChars.push_back(4);
      else
        indentChars.push_back(indentChars.back() + 4);
    }

    if ((ch == wxS(')')) || (ch == wxS(']')) || (ch == wxS('}'))) {
      if (!indentChars.empty())
        indentChars.pop_back();
    }

    // A comma removes all extra indentation from a "do" or an "if".
    if (ch == wxS(',')) {
      // Discard any extra indentation from a "then" or a "do" from the last
      // item of indentChars.
      if (!indentChars.empty()) {
        size_t lst;
        indentChars.pop_back();
        if (!indentChars.empty())
          lst = indentChars.back() + 4;
        else
          lst = 0;
        indentChars.push_back(lst);
      }
    }

    // A semicolon or a dollar sign restarts indentation completely.
    if ((ch == wxS(';')) || (ch == wxS('$'))) {
      // Discard any indentation data
      while (!indentChars.empty())
        indentChars.pop_back();

      // Start fresh with zero indentation.
      indentChars.push_back(0);
    }

    // A "do" or an "if" increases the current indentation level by a tab.
    if ((!wxIsalnum(ch)) || (pos == 0)) {
      // Concatenate the current with the following two characters
      wxString::const_iterator it2(it);
      wxString rest(*it2);
      ++it2;
      if (it2 < m_text.end()) {
        rest += wxString(*it2);
        ++it2;
        if (it2 < m_text.end())
          rest += wxString(*it2);
      }
      // Handle a "do"
      if (rest.StartsWith(wxS("do")) &&
          ((rest.Length() < 3) || (!wxIsalnum(rest[2])))) {
        size_t lst = 0;
        if (!indentChars.empty()) {
          lst = indentChars.back();
          indentChars.pop_back();
        }
        indentChars.push_back(lst + 4);
      }

      // Handle a "if"
      if (rest.StartsWith(wxS("if")) &&
          ((rest.Length() < 3) || (!wxIsalnum(rest[2])))) {
        size_t lst = 0;
        if (!indentChars.empty()) {
          lst = indentChars.back();
          indentChars.pop_back();
        }
        indentChars.push_back(lst + 4);
      }
    }

    if (it < m_text.end()) {
      ++pos;
      ++it;
    }
  }

  if (it < m_text.end()) {
    if ((text[positionOfCaret] == wxS(')')) ||
        (text[positionOfCaret] == wxS(']')) ||
        (text[positionOfCaret] == wxS('}'))) {
      if (!indentChars.empty())
        indentChars.pop_back();
    }
  }

  size_t retval;
  if (indentChars.empty())
    retval = 0;
  else
    retval = indentChars.back();

  // A fast way to get the next 5 characters
  wxString rightOfCursor;
  for (int i = 0; i < 5; i++) {
    if (it >= m_text.end())
      break;

    rightOfCursor += *it;
    ++it;
  }

  rightOfCursor.Trim();
  rightOfCursor.Trim(false);
  if (((rightOfCursor.StartsWith(wxS("else"))) ||
       (rightOfCursor.StartsWith(wxS("then")))) &&
      (rightOfCursor.Length() > 4) && (!(wxIsalnum(rightOfCursor[4]))))
    retval -= 4;

  if (retval < 0)
    retval = 0;

  return retval;
}

void EditorCell::ProcessNewline(bool keepCursorAtStartOfLine) {
  if (GetSelection().IsActive()) // we have a selection, delete it, then proceed
    {
      SaveValue();
      auto start = GetSelection().Left();
      auto end =   GetSelection().Right();
      m_text =
        m_text.SubString(0, start - 1) + m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(start);
      ClearSelection();
    }

  {
    bool autoIndent = m_configuration->GetAutoIndent();
    // If the cursor is at the beginning of a line we will move it there again
    // after indenting.
    bool cursorAtStartOfLine =
      keepCursorAtStartOfLine &&
      (GetSelection().CursorPosition() == BeginningOfLine(GetSelection().CursorPosition()));

    // If the cursor is part of the whitespace at the beginning of the line
    // we move it to its end if this makes sense.
    if (autoIndent) {
      size_t i = BeginningOfLine(GetSelection().CursorPosition());
      while ((i < GetSelection().CursorPosition()) && (m_text[i] == wxS(' ')))
        ++i;
      if (i == GetSelection().CursorPosition())
        while ((GetSelection().CursorPosition() < m_text.Length() - 1) &&
               (m_text[GetSelection().CursorPosition()] == wxS(' ')))
          GetSelection().StepCursor(1);
    }

    size_t indentChars = GetIndentDepth(m_text, GetSelection().CursorPosition());

    // The string we indent with.
    wxString indentString;
    if (autoIndent && (indentChars > 0))
      for (size_t i = 0; i < indentChars; i++)
        indentString += wxS(" ");

    wxString newLines = m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
    if (autoIndent) {
      // Remove leading spaces from the text that follows the cursor
      wxString newLines_noleadingSpaces;
      newLines_noleadingSpaces.reserve(newLines.Length());
      wxString::const_iterator it = newLines.begin();
      for (; (it != newLines.end()) && (*it == ' '); ++it) {
      }

      for (; it != newLines.end(); ++it)
        newLines_noleadingSpaces += *it;
      newLines = newLines_noleadingSpaces;
    }
    m_text = m_text.SubString(0, GetSelection().CursorPosition() - 1) + wxS("\n") +
      indentString + newLines;
    GetSelection().StepCursor(1);
    if ((indentChars > 0) && (autoIndent)) {
      GetSelection().CursorPosition() = BeginningOfLine(GetSelection().CursorPosition());
      GetSelection().StepCursor(indentChars);
    }
    m_isDirty = true;
    m_containsChanges = true;

    if ((!m_configuration->CursorJump()) ||
        ((cursorAtStartOfLine) && (!autoIndent)))
      GetSelection().CursorPosition(BeginningOfLine(GetSelection().CursorPosition()));
  }
}

bool EditorCell::HandleSpecialKey(wxKeyEvent &event) {
  bool done = true;

  if (((event.GetKeyCode() == 'x') || (event.GetKeyCode() == 'u')) &&
      (event.AltDown())) {
    ConvertNumToUNicodeChar();
    return true;
  }

  if ((event.GetKeyCode() != WXK_DOWN) &&
      (event.GetKeyCode() != WXK_PAGEDOWN) &&
      (event.GetKeyCode() != WXK_PAGEUP) &&
#ifdef WXK_NUMPAD_PRIOR
      (event.GetKeyCode() != WXK_NUMPAD_PRIOR) &&
#endif
#ifdef WXK_PRIOR
      (event.GetKeyCode() != WXK_PRIOR) &&
#endif
#ifdef WXK_NEXT
      (event.GetKeyCode() != WXK_NEXT) &&
#endif
#ifdef WXK_NUMPAD_NEXT
      (event.GetKeyCode() != WXK_NUMPAD_NEXT) &&
#endif
      (event.GetKeyCode() != WXK_UP))
    m_caretColumn = -1; // make caretColumn invalid

  switch (event.GetKeyCode()) {
  case WXK_LEFT:
    {
      auto pos = GetSelection().CursorPosition();
      SaveValue();
      if (event.ControlDown()) {
	size_t lastpos = GetSelection().CursorPosition();
	while ((pos > 0) &&
	       (wxIsalnum(m_text[pos - 1]) ||
		m_text[pos - 1] == wxS('_') ||
		((pos > 1) &&
		 (m_text[pos - 2] == wxS('\\'))))) {
	  if ((pos > 1) &&
	      (m_text[pos - 2] == wxS('\\')))
	    pos--;
	  pos--;
	}
	
	while ((pos > 0) &&
	       (wxIsspace(m_text[pos - 1])))
	  pos--;
	
	if ((lastpos == pos) && (pos > 0))
	  pos--;
      } else if (event.AltDown()) {
	size_t count = 0;
	
	while (pos > 0 && count >= 0) {
	  pos--;
        if (m_text[pos] == '(' ||
            m_text[pos] == '[')
          count--;
        else if (m_text[pos] == ')' ||
                 m_text[pos] == ']')
          count++;
      }
    } else if (pos > 0)
      pos--;

    if (event.ShiftDown())
      GetSelection().End(pos);
    else
      GetSelection().CursorPosition(pos);

    break;

    case WXK_RIGHT:
      {
	auto pos = GetSelection().CursorPosition();
	SaveValue();

	if (event.ControlDown()) {
	  size_t lastpos = pos;

	  while ((pos < m_text.Length()) &&
		 (wxIsalnum(m_text[pos]) ||
		  m_text[pos] == wxS('_') ||
		  m_text[pos] == wxS('\\'))) {
	    if (m_text[pos] == wxS('\\'))
	      pos++;
	    if (pos < m_text.Length())
	      pos++;
	  }

	  while ((pos < m_text.Length()) &&
		 (wxIsspace(m_text[pos])))
	    pos++;

	  if ((pos < m_text.Length()) &&
	      (lastpos == pos))
	    pos++;
	} else if (event.AltDown()) {
	  size_t count = 0;

	  while (pos < m_text.Length() && count >= 0) {
	    pos++;
	    if ((m_text[pos - 1] == '(') ||
		(m_text[pos - 1] == '['))
	      count++;
	    else if ((m_text[pos - 1] == ')') ||
		     (m_text[pos - 1] == ']'))
	      count--;
	  }
	}

	else if (pos < m_text.Length())
	  pos++;

	if (event.ShiftDown())
	  GetSelection().End(pos);
	else
	  GetSelection().CursorPosition(pos);

	break;
      }
      case WXK_END:
	{
	  auto pos = GetSelection().CursorPosition();
	  SaveValue();
	  
	  if (event.ControlDown()) {
	    pos = m_text.Length();
	    if (pos < -1)
	      pos = -1;
	  } else {
	    while (pos < m_text.Length() &&
		   m_text.GetChar(pos) != '\n' &&
		   m_text.GetChar(pos) != '\r')
	      pos++;
	  }
	  
	if (event.ShiftDown())
	  GetSelection().End(pos);
	else
	  GetSelection().CursorPosition(pos);
	break;

  case WXK_HOME:
    {
      auto pos = GetSelection().CursorPosition();
      SaveValue();
      
      if (event.ControlDown())
        pos = 0;
      else {
        size_t col, lin;
        PositionToXY(pos, &col, &lin);
        pos = XYToPosition(0, lin);
      }
      
      if (event.ShiftDown())
	GetSelection().End(pos);
      else
	GetSelection().CursorPosition(pos);
      break;
    }
    break;
    
  case WXK_PAGEDOWN:
#ifdef WXK_NEXT
  case WXK_NEXT:
#endif
#ifdef WXK_NUMPAD_NEXT
  case WXK_NUMPAD_NEXT:
#endif
    {
      auto pos = GetSelection().CursorPosition();
      SaveValue();
      size_t column;
      size_t line;
      PositionToXY(pos, &column, &line); // get current line
      if (m_caretColumn > -1)
        column = m_caretColumn;
      else
        m_caretColumn = column;
      
      if (line < m_numberOfLines - 1) // can we go down ?
	{
	  size_t scrolllength = m_configuration->GetCanvasSize().y - m_charHeight;
	  
	  while ((line < m_numberOfLines - 1) && (scrolllength > 0)) {
	    line++;
	    pos = XYToPosition(column, line);
	    scrolllength -= m_charHeight;
	  }
	} else { // we can't go down. move caret to the end
        pos = m_text.Length();
        m_caretColumn = -1; // make caretColumn invalid
      }
      
      if (event.ShiftDown())
	GetSelection().End(pos);
      else
	GetSelection().CursorPosition(pos);
      break;
    }
    break;

  case WXK_DOWN:
    {
      auto pos = GetSelection().CursorPosition();
      SaveValue();
      size_t column, line;
      PositionToXY(pos, &column, &line); // get current line
      if (m_caretColumn > -1)
        column = m_caretColumn;
      else
        m_caretColumn = column;

      if (line < m_numberOfLines - 1) // can we go down ?
        pos = XYToPosition(column, line + 1);
      else { // we can't go down. move caret to the end
        pos = m_text.Length();
        if (pos < -1)
          pos = -1;
        m_caretColumn = -1; // make caretColumn invalid
      }

      if (event.ShiftDown())
	GetSelection().End(pos);
      else
	GetSelection().CursorPosition(pos);
    }
    break;

  case WXK_PAGEUP:
#ifdef WXK_PRIOR
  case WXK_PRIOR:
#endif
#ifdef WXK_NUMPAD_PRIOR
  case WXK_NUMPAD_PRIOR:
#endif
    {
      auto pos = GetSelection().CursorPosition();
      SaveValue();

      size_t column, line;
      PositionToXY(pos, &column, &line); // get current line
      if (m_caretColumn > -1)
        column = m_caretColumn;
      else
        m_caretColumn = column;

      if (line > 0) // can we go up?
	{
	  size_t scrolllength = m_configuration->GetCanvasSize().y - m_charHeight;

	  while ((line > 0) && (scrolllength > 0)) {
	    line--;
	    pos = XYToPosition(column, line);
	    scrolllength -= m_charHeight;
	  }
	} else { // we can't move up, move to the beginning
        pos = 0;
        m_caretColumn = -1; // make caretColumn invalid
      }

      if (event.ShiftDown())
	GetSelection().End(pos);
      else
	GetSelection().CursorPosition(pos);
    }
    break;

  case WXK_UP:
    {
      auto pos = GetSelection().CursorPosition();
      SaveValue();

      size_t column, line;
      PositionToXY(pos, &column, &line); // get current line
      if (m_caretColumn > -1)
        column = m_caretColumn;
      else
        m_caretColumn = column;

      if (line > 0) // can we go up?
        pos = XYToPosition(column, line - 1);
      else { // we can't move up, move to the beginning
        pos = 0;
        m_caretColumn = -1; // make caretColumn invalid
      }

      if (event.ShiftDown())
	GetSelection().End(pos);
      else
	GetSelection().CursorPosition(pos);
    }
    break;

  case WXK_RETURN:
    SaveValue();
    ProcessNewline();
    StyleText();
    break;

  case WXK_DELETE:
    // On windows CMD+WXK_BACK is passed to us as CMD+WXK_DELETE.
    if (!event.CmdDown()) {
      SaveValue();
      if (!GetSelection().IsActive()) {
        if (GetSelection().CursorPosition() < m_text.Length()) {
          m_isDirty = true;
          m_containsChanges = true;
          m_text = m_text.SubString(0, GetSelection().CursorPosition() - 1) +
	    m_text.SubString(GetSelection().CursorPosition() + 1, m_text.Length());
        }
      } else {
        m_isDirty = true;
        m_containsChanges = true;
        SaveValue();
        m_saveValue = true;
        auto start = GetSelection().Left();
        auto end   = GetSelection().Right();
        m_text = m_text.SubString(0, start - 1) +
	  m_text.SubString(end, m_text.Length());
        GetSelection().CursorPosition(start);
      }
    } else {
      // Ctrl+Backspace is pressed.

      m_containsChanges = true;
      m_isDirty = true;

      size_t lastpos = GetSelection().CursorPosition();
      size_t pos     = GetSelection().CursorPosition();
      // Delete characters until the end of the current word or number
      while (pos > 0 &&
             wxIsalnum(m_text[pos - 1])) {
        pos--;
        m_text = m_text.SubString(0, pos - 1) +
	  m_text.SubString(pos + 1, m_text.Length());
      }
      // Delete Spaces, Tabs and Newlines until the next printable character
      while (pos > 0 &&
             wxIsspace(m_text[pos - 1])) {
        pos--;
        m_text = m_text.SubString(0, pos - 1) +
	  m_text.SubString(pos + 1, m_text.Length());
      }

      // If we didn't delete anything till now delete one single character.
      if (lastpos == pos) {
        pos--;
        m_text = m_text.SubString(0, pos - 1) +
	  m_text.SubString(pos + 1, m_text.Length());
      }
    }
    StyleText();
    GetSelection().CursorPosition(pos);
    break;

  case WXK_BACK:
    SaveValue();
    {
      auto pos = GetSelection().CursorPosition();
      if (SelectionActive()) {
	m_saveValue = true;
	m_containsChanges = true;
	m_isDirty = true;
	auto start = GetSelection().Left();
	auto end   = GetSelection().Right();
	m_text = m_text.SubString(0, start - 1) +
	  m_text.SubString(end, m_text.Length());
	pos = start;
	ClearSelection();
	StyleText();
	break;
      } else {
	if (!event.CmdDown()) {
	  // Backspace without Ctrl => Delete one character if there are
	  // characters to delete.
	  if (pos > 0) {
          m_containsChanges = true;
          m_isDirty = true;
	  
          if (m_text.SubString(0, pos - 1).Right(4) ==
              wxS("    ")) {
            m_text = m_text.SubString(0, pos - 5) +
	      m_text.SubString(pos, m_text.Length());
            pos -= 4;
          } else {
            /// If deleting ( in () then delete both.
            size_t right = pos;
            if (pos < m_text.Length() &&
                m_configuration->GetMatchParens() &&
                ((m_text.GetChar(pos - 1) == '[' &&
                  m_text.GetChar(pos) == ']') ||
                 (m_text.GetChar(pos - 1) == '(' &&
                  m_text.GetChar(pos) == ')') ||
                 (m_text.GetChar(pos - 1) == '{' &&
                  m_text.GetChar(pos) == '}') ||
                 (m_text.GetChar(pos - 1) == '"' &&
                  m_text.GetChar(pos) == '"')))
              right++;
            m_text = m_text.SubString(0, pos - 2) +
	      m_text.SubString(right, m_text.Length());
            pos--;
          }
	  }
	  
	} else {
	  // Ctrl+Backspace is pressed.
	  
	  m_containsChanges = true;
	  m_isDirty = true;
	  
	  size_t lastpos = pos;
	  // Delete characters until the end of the current word or number
	  while (pos > 0 &&
		 wxIsalnum(m_text[pos - 1])) {
	    pos--;
	    m_text = m_text.SubString(0, pos - 1) +
	      m_text.SubString(pos + 1, m_text.Length());
	  }
	  // Delete Spaces, Tabs and Newlines until the next printable character
	  while (pos > 0 &&
		 wxIsspace(m_text[pos - 1])) {
	    pos--;
	    m_text = m_text.SubString(0, pos - 1) +
	      m_text.SubString(pos + 1, m_text.Length());
	  }
	  
	  // If we didn't delete anything till now delete one single character.
	  if (lastpos == pos) {
	    pos--;
	    m_text = m_text.SubString(0, pos - 1) +
	      m_text.SubString(pos + 1, m_text.Length());
	  }
	}
      }
      StyleText();
      GetSelection().CursorPosition(pos);
    }
    break;

  case WXK_TAB:
    {
      auto pos = GetSelection().CursorPosition();
      
      m_isDirty = true;
      if (!FindNextTemplate(event.ShiftDown())) {
	m_containsChanges = true;
	{
	  if (SelectionActive()) {
	    // Selection active and Tab
	    SaveValue();

	    auto start = GetSelection().Left();
	    auto end   = GetSelection().Right();
	    size_t newLineIndex = wxMin(m_text.find(wxS('\n'), start),
					m_text.find(wxS('\r'), start));

	    if (((newLineIndex != wxNOT_FOUND) && (newLineIndex < end)) ||
		(m_text.SubString(newLineIndex, start).Trim() == wxEmptyString)) {
	      start = BeginningOfLine(start);
	      size_t pos = start;

	      if ((m_text[end] == wxS('\n')))
		end++;

	      if (end > m_text.Length())
		end = m_text.Length();

	      while (pos < end) {
		if (event.ShiftDown()) {
		  for (int i = 0; i < 4; i++)
		    if (m_text[pos] == wxS(' ')) {
		      m_text = m_text.SubString(0, pos - 1) +
			m_text.SubString(pos + 1, m_text.Length());
		      if (end > 0)
			end--;
		    }
		} else {
		  m_text = m_text.SubString(0, pos - 1) + wxS("    ") +
		    m_text.SubString(pos, m_text.Length());
		  end += 4;
		  pos += 4;
		}
		while ((pos < end) && (m_text[pos] != wxS('\n')) &&
		       (m_text[pos] != wxS('\r')))
		  pos++;
		if ((pos < end) &&
		    ((m_text[pos] == wxS('\n')) || (m_text[pos] == wxS('\r'))))
		  pos++;
	      }
	      SetSelection(start, end);
	    } else {
	      m_text = m_text.SubString(0, start - 1) +
		m_text.SubString(end, m_text.Length());
	      ClearSelection();
	    }
	    pos = start;
	    StyleText();
	    break;
	  } else {
	    if (!event.ShiftDown()) {
	      // Selection active and Tab was pressed without Shift
	      size_t col, line;
	      PositionToXY(pos, &col, &line);
	      wxString ins;
	      do {
		col++;
		ins += wxS(" ");
	      } while (col % 4 != 0);

	      m_text = m_text.SubString(0, pos - 1) + ins +
		m_text.SubString(pos, m_text.Length());
	      pos += ins.Length();
	    } else {
	      // Selection active and Shift+Tab
	      size_t start = BeginningOfLine(pos);
	      if (m_text.SubString(start, start + 3) == wxS("    ")) {
		m_text = m_text.SubString(0, start - 1) +
		  m_text.SubString(start + 4, m_text.Length());
		if (pos > start) {
		  pos = start;
		  while ((pos < m_text.Length()) &&
			 (m_text[pos] == wxS(' ')))
		    pos++;
		}
	      }
	    }
	  }
	}
      }
    }
    GetSelection().CursorPosition(pos);
    StyleText();
    break;

    /* Ignored keys */
  case WXK_WINDOWS_LEFT:
  case WXK_WINDOWS_RIGHT:
  case WXK_WINDOWS_MENU:
  case WXK_COMMAND:
  case WXK_START:
    break;

  default:
    done = false;
    break;
  }

  return done;
}

bool EditorCell::HandleOrdinaryKey(wxKeyEvent &event) {
  if (event.ControlDown() && !event.AltDown())
    return false;

  m_isDirty = true;
  m_containsChanges = true;
  bool insertLetter = true;

  if (m_saveValue) {
    SaveValue();
    m_saveValue = false;
  }

  wxChar keyCode;
  keyCode = event.GetUnicodeKey();

  // If we got passed a non-printable character we have to send it back to the
  // hotkey management.
  if (keyCode == WXK_NONE) {
    event.Skip();
    return false;
  }

  // It may be not too intelligent to cache all pieces of a word we arrived at
  // during typing
  if (keyCode == ' ')
    m_widths.clear();

  if (m_historyPosition != -1) {
    m_history.erase(m_history.begin() + m_historyPosition + 1, m_history.end());
    m_historyPosition = -1;
  }

  // if we have a selection either put parens around it (and don't write the
  // letter afterwards) or delete selection and write letter (insertLetter =
  // true).
  if ((GetSelection().Start() > 0) && (GetSelection().IsActive())) {
    SaveValue();
    auto start = GetSelection().Left();
    auto end   = GetSelection().Right();

    switch (keyCode) {
    case '(':
      m_text = m_text.SubString(0, start - 1) + wxS("(") +
	m_text.SubString(start, end - 1) + wxS(")") +
	m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(start);
      insertLetter = false;
      break;
    case '\"':
      m_text = m_text.SubString(0, start - 1) + wxS("\"") +
	m_text.SubString(start, end - 1) + wxS("\"") +
	m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(start);
      insertLetter = false;
      break;
    case '{':
      m_text = m_text.SubString(0, start - 1) + wxS("{") +
	m_text.SubString(start, end - 1) + wxS("}") +
	m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(start);
      insertLetter = false;
      break;
    case '[':
      m_text = m_text.SubString(0, start - 1) + wxS("[") +
	m_text.SubString(start, end - 1) + wxS("]") +
	m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(start);
      insertLetter = false;
      break;
    case ')':
      m_text = m_text.SubString(0, start - 1) + wxS("(") +
	m_text.SubString(start, end - 1) + wxS(")") +
	m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(end + 2);
      insertLetter = false;
      break;
    case '}':
      m_text = m_text.SubString(0, start - 1) + wxS("{") +
	m_text.SubString(start, end - 1) + wxS("}") +
	m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(end + 2);
      insertLetter = false;
      break;
    case ']':
      m_text = m_text.SubString(0, start - 1) + wxS("[") +
	m_text.SubString(start, end - 1) + wxS("]") +
	m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(end + 2);
      insertLetter = false;
      break;
    default: // delete selection
      m_text = m_text.SubString(0, start - 1) +
	m_text.SubString(end, m_text.Length());
      GetSelection().CursorPosition(end + 2);
      break;
    }
    ClearSelection();
    StyleText();
  } // end if (GetSelection().IsActive())

  // insert letter if we didn't insert brackets around selection
  if (insertLetter) {
    wxString chr;

    chr = event.GetUnicodeKey();

    if (event.ShiftDown())
      chr.Replace(wxS(" "), wxS("\u00a0"));

    m_text = m_text.SubString(0, GetSelection().CursorPosition() - 1) + chr +
      m_text.SubString(GetSelection().CursorPosition(), m_text.Length());

    GetSelection().CursorMove(1);

    if (m_configuration->GetMatchParens()) {
      switch (keyCode) {
      case '(':
        m_text = m_text.SubString(0, GetSelection().CursorPosition() - 1) + wxS(")") +
	  m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
        break;
      case '[':
        m_text = m_text.SubString(0, GetSelection().CursorPosition() - 1) + wxS("]") +
	  m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
        break;
      case '{':
        m_text = m_text.SubString(0, GetSelection().CursorPosition() - 1) + wxS("}") +
	  m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
        break;
      case '"':
        if (GetSelection().CursorPosition() < m_text.Length() &&
            m_text.GetChar(GetSelection().CursorPosition()) == '"')
          m_text = m_text.SubString(0, GetSelection().CursorPosition() - 2) +
	    m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
        else
          m_text = m_text.SubString(0, GetSelection().CursorPosition() - 1) + wxS("\"") +
	    m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
        break;
      case ')': // jump over ')'
        if (GetSelection().CursorPosition() < m_text.Length() &&
            m_text.GetChar(GetSelection().CursorPosition()) == ')')
          m_text = m_text.SubString(0, GetSelection().CursorPosition() - 2) +
	    m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
        break;
      case ']': // jump over ']'
        if (GetSelection().CursorPosition() < m_text.Length() &&
            m_text.GetChar(GetSelection().CursorPosition()) == ']')
          m_text = m_text.SubString(0, GetSelection().CursorPosition() - 2) +
	    m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
        break;
      case '}': // jump over '}'
        if (GetSelection().CursorPosition() < m_text.Length() &&
            m_text.GetChar(GetSelection().CursorPosition()) == '}')
          m_text = m_text.SubString(0, GetSelection().CursorPosition() - 2) +
	    m_text.SubString(GetSelection().CursorPosition(), m_text.Length());
        break;
      case '+':
        // case '-': // this could mean negative.
      case '*':
      case '/':
      case '^':
      case '=':
      case ',':
        size_t len = m_text.Length();
        if (m_configuration->GetInsertAns()) {
          // Insert an "%" before an operator that begins this cell
          if (len == 1 && GetSelection().CursorPosition() == 1) {
            m_text = m_text.SubString(0, GetSelection().CursorPosition() - 2) + wxS("%") +
	      m_text.SubString(GetSelection().CursorPosition() - 1, m_text.Length());
            GetSelection().StepCursor(1);
          }

          // If this operator happens to be the first letter of a comment start
          // sign we remove the "%" again as the unability to begin a code cell
          // with a comment in the obvious way tends to surprise users.
          if ((len == 3) && (GetSelection().CursorPosition() == 3) &&
              (m_text.StartsWith(wxS("%/*")))) {
            m_text = m_text.SubString(GetSelection().CursorPosition() - 2, m_text.Length());
            GetSelection().StepCursor(-1);
          }
        }
        break;
      }
    }
  } // end if (insertLetter)

  StyleText();
  return true;
}

/**
 * For a given quotation mark ("), find a matching quote.
 * Since there are no nested quotes, an odd-numbered, non-escaped quote
 * is an opening quote, and an even-numbered non-escaped quote
 * is a closing quote.
 *
 * @return true if matching quotation marks were found; false otherwise
 */
bool EditorCell::FindMatchingQuotes() {
  if (GetSelection().CursorPosition() < 0) {
    m_paren1 = m_paren2 = -1;
    return false;
  }

  size_t pos = 0;
  for (auto const &tok : m_tokens) {
    if ((tok.GetText().StartsWith(wxS("\""))) &&
        (tok.GetText().EndsWith(wxS("\"")))) {
      size_t tokenEnd = pos + tok.GetText().Length() - 1;
      if (((unsigned)GetSelection().CursorPosition() == tokenEnd) ||
           (GetSelection().CursorPosition() == pos)) {
        m_paren1 = pos;
        m_paren2 = tokenEnd;
        return true;
      }
    }
    if (pos > GetSelection().CursorPosition())
      return false;
    pos += tok.GetText().Length();
  }
  return false;
}

void EditorCell::FindMatchingParens() {
  m_paren1 = m_paren2 = -1;
  if (GetSelection().CursorPosition() >= m_text.Length())
    return;

  wxChar charUnderCursor = m_text.GetChar(GetSelection().CursorPosition());
  if (charUnderCursor == wxS('\"')) {
    FindMatchingQuotes();
    return;
  }
  if ((charUnderCursor == wxS('(')) || (charUnderCursor == wxS('[')) ||
      (charUnderCursor == wxS('{'))) {
    size_t parenLevel = 0;
    size_t pos = 0;
    for (auto const &tok : m_tokens) {
      if (pos >= GetSelection().CursorPosition()) {
        if ((tok.GetText().StartsWith(wxS("("))) ||
            (tok.GetText().StartsWith(wxS("["))) ||
            (tok.GetText().StartsWith(wxS("{"))))
          parenLevel++;
        else {
          if ((tok.GetText().StartsWith(wxS(")"))) ||
              (tok.GetText().StartsWith(wxS("]"))) ||
              (tok.GetText().StartsWith(wxS("}")))) {
            parenLevel--;
            if (parenLevel == 0) {
              m_paren1 = GetSelection().CursorPosition();
              m_paren2 = pos;
              return;
            }
          }
        }
      }
      pos += tok.GetText().Length();
    }
    return;
  }
  if ((charUnderCursor == wxS(')')) || (charUnderCursor == wxS(']')) ||
      (charUnderCursor == wxS('}'))) {
    size_t parenLevel = 0;
    size_t pos = m_text.Length() - 1;
    auto const tokens = MaximaTokenizer(m_text, m_configuration).PopTokens();
    for (auto tok = tokens.rbegin(); tok != tokens.rend(); ++tok) {
      if (pos <= GetSelection().CursorPosition()) {
        if ((tok->GetText().StartsWith(wxS("("))) ||
            (tok->GetText().StartsWith(wxS("["))) ||
            (tok->GetText().StartsWith(wxS("{")))) {
          parenLevel--;
          if (parenLevel == 0) {
            m_paren1 = pos;
            m_paren2 = GetSelection().CursorPosition();
            return;
          }
        } else {
          if ((tok->GetText().StartsWith(wxS(")"))) ||
              (tok->GetText().StartsWith(wxS("]"))) ||
              (tok->GetText().StartsWith(wxS("}")))) {
            parenLevel++;
          }
        }
      }
      pos -= tok->GetText().Length();
    }
  }
}

wxString EditorCell::InterpretEscapeString(const wxString &txt) {
  auto &escCode = Configuration::GetEscCode(txt);
  if (!escCode.empty())
    return escCode;

  long int unicodeval = -1;
  if (txt.ToLong(&unicodeval, 16)) {
    if (unicodeval > 32)
      return wxUniChar(unicodeval);
    return wxS(" ");
  }
  return {};
}

void EditorCell::DeactivateCursor() {
  EditorCell *editor = m_cellPointers->m_activeCell;
  if (editor) {
    editor->ClearSelection();
    editor->m_paren1 = editor->m_paren2 = -1;
  }
  m_cellPointers->m_activeCell = nullptr;
}

bool EditorCell::ActivateCursor() {
  bool retval = false;
  if (!m_cellPointers->m_activeCell)
    DeactivateCursor();

  SaveValue();
  m_displayCaret = true;
  m_hasFocus = true;
  m_cellPointers->m_activeCell = this;

  ClearSelection();
  m_paren1 = m_paren2 = -1;

  // upon activation unhide the parent groupcell
  if(m_firstLineOnly)
    {
      m_firstLineOnly = false;
      StyleText();
      retval = true;
    }
  GetGroup()->Hide(false);
  if (GetType() == MC_TYPE_INPUT)
    FindMatchingParens();
  return retval;
}

bool EditorCell::AddEnding() {
  // Lisp cells don't require a maxima line ending
  if (m_configuration->InLispMode())
    return false;

  // Cells that aren't code cells don't require a maxima line ending.
  if (GetType() != MC_TYPE_INPUT)
    return false;

  bool endingNeeded = true;

  for (auto const &tok : GetAllTokens()) {
    TextStyle itemStyle = tok.GetTextStyle();
    if ((itemStyle == TS_CODE_ENDOFLINE) || (itemStyle == TS_CODE_LISP)) {
      endingNeeded = false;
    } else {
      if ((!tok.GetText().StartsWith(" ")) &&
          (!tok.GetText().StartsWith("\t")) &&
          (!tok.GetText().StartsWith("\n")) &&
          (!tok.GetText().StartsWith("\r")) &&
          (!(itemStyle == TS_CODE_COMMENT)))
        endingNeeded = true;
    }
  }

  if (endingNeeded) {
    m_text += wxS(";");
    m_paren1 = m_paren2 = m_width = -1;
    StyleText();
    return true;
  }
  return false;
}

//
// lines and columns are counted from zero
// position of caret is pos if caret is just before the character
//   at position pos in m_text.
//
void EditorCell::PositionToXY(size_t position, size_t *x, size_t *y) {
  size_t col = 0, lin = 0;
  size_t pos = 0;

  wxString::const_iterator it = m_text.begin();
  while ((pos < position) && (it < m_text.end())) {
    if ((*it == '\n') || (*it == '\r')) {
      col = 0;
      lin++;
    } else
      col++;

    ++it;
    ++pos;
  }

  *x = col;
  *y = lin;
}

size_t EditorCell::XYToPosition(size_t x, size_t y) {
  size_t col = 0, lin = 0, pos = 0;

  wxString::const_iterator it = m_text.begin();
  while ((it < m_text.end()) && (lin < y)) {
    if ((*it == '\n') || (*it == '\r'))
      lin++;

    ++it;
    ++pos;
  }

  while ((it < m_text.end()) && (pos < m_text.Length()) && (col < x)) {
    if ((*it == '\n') || (*it == '\r'))
      break;
    ++pos;
    ++col;
    ++it;
  }

  return pos;
}

wxPoint EditorCell::PositionToPoint(int pos) {
  SetFont(m_configuration->GetRecalcDC());

  wxCoord x = m_currentPoint.x, y = m_currentPoint.y;

  if ((x < 0) || (y < 0))
    return wxPoint(-1, -1);

  wxCoord width;
  size_t cX, cY;

  if (pos < 0)
    pos = GetSelection().CursorPosition();

  PositionToXY(pos, &cX, &cY);

  width = GetLineWidth(cY, cX);

  x += width;
  y += m_charHeight * cY;

  return wxPoint(x, y);
}

void EditorCell::SelectPointText(const wxPoint point) {
  wxString s;
  SetFont(m_configuration->GetRecalcDC());
  auto pos = GetSelection().CursorPosition();
  ClearSelection();
  wxPoint posInCell(point);

  wxASSERT_MSG(m_currentPoint.x >= 0, _("Bug: x position of cell is unknown!"));
  wxASSERT_MSG(m_currentPoint.y >= 0, _("Bug: y position of cell is unknown!"));
  posInCell -= m_currentPoint;
  //  posInCell -= wxPoint(m_fontSize, 2);
  posInCell.y -= m_center;

  size_t lin = posInCell.y / m_charHeight + 1;
  if (posInCell.y < 0)
    lin = 0;
  size_t lineStart = XYToPosition(0, lin);
  pos = lineStart;
  // Find the text snippet the line we search for begins with
  size_t currentLine = 1;
  wxCoord indentPixels = 0;
  std::vector<StyledText>::const_iterator textSnippet;
  for (textSnippet = m_styledText.begin();
       ((textSnippet != m_styledText.end()) && (currentLine <= lin));
       ++textSnippet) {
    if ((textSnippet->GetText() == '\n') || (textSnippet->GetText() == '\r')) {
      indentPixels = textSnippet->GetIndentPixels();
      currentLine++;
    }
  }

  if (GetType() == MC_TYPE_INPUT) {
    // Code cell

    wxCoord xpos = 0;
    // Find the text snippet the cursor is in
    while ((textSnippet != m_styledText.end()) && (xpos < posInCell.x)) {
      wxString txt = textSnippet->GetText();
      wxCoord firstCharWidth;
      firstCharWidth = GetTextSize(txt.Left(1)).GetWidth();

      if ((txt == wxS("\n")) || (txt == wxS("\r")))
        break;

      wxCoord w = GetTextSize(txt).GetWidth();
      if (xpos + w + firstCharWidth / 2 < posInCell.x) {
        xpos += w;
        post += txt.Length();
      } else
        break;

      ++textSnippet;
    }

    wxCoord lastwidth = 0;
    wxString snippet;
    if (textSnippet != m_styledText.end())
      snippet = textSnippet->GetText();

    lastwidth = GetTextSize(snippet.Left(1)).GetWidth();
    lastwidth = -lastwidth;

    // Now determine which char inside this text snippet the cursor is at
    if ((snippet != wxS("\r")) && (snippet != wxS("\n"))) {
      for (size_t i = 0; i < snippet.Length(); i++) {
        wxCoord width = GetTextSize(snippet.Left(i)).GetWidth();
        if (xpos + width + (width - lastwidth) / 2 < posInCell.x)
          post++;
        else
          break;
        lastwidth = width;
      }
    }
    m_displayCaret = true;
    m_caretColumn = -1;
    FindMatchingParens();
    // The line that now follows is pure paranoia.
    post = wxMin(post, m_text.Length());
    if (post < -1)
      post = -1;
  } else {
    // Text cell

    wxString text = m_text;

    // Handle indentation.
    posInCell.x -= indentPixels;

    while (post < text.Length() &&
           text.GetChar(post) != '\n' &&
           text.GetChar(post) != '\r') {
      wxCoord width;
      width =
	GetTextSize(text.SubString(lineStart, post)).GetWidth();
      if (width > posInCell.x)
        break;

      post++;
    }
    post = wxMin(post, text.Length());
    if (post < -1)
      post = -1;

    m_displayCaret = true;
    m_caretColumn = -1;
  }
  GetSelection().CursorPosition(post);
}

void EditorCell::SelectRectText(const wxPoint one, const wxPoint two) {
  SelectPointText(one);
  size_t start = GetSelection().CursorPosition();
  SelectPointText(two);
  m_paren2 = m_paren1 = -1;
  m_caretColumn = -1;
  GetSelection().Start(start);
}

// IsPointInSelection
// Return true if coordinates "point" fall into selection
// If they don't or there is no selection it returns false
bool EditorCell::IsPointInSelection(wxPoint point) {
  if ((!GetSelection().IsActive()) || (m_selectionEnd == -1) || !IsActive())
    return false;

  wxRect rect = GetRect();
  if (!rect.Contains(point))
    return false;

  wxString s;
  wxString text = m_text;
  SetFont(m_configuration->GetRecalcDC());
  // Determine the line the point would be in
  wxPoint posInCell(point);
  posInCell.x -= m_currentPoint.x - 2;
  posInCell.y -= m_currentPoint.y - 2 - m_center;
  size_t lin = posInCell.y / m_charHeight;
  size_t lineStart = XYToPosition(0, lin);
  size_t positionOfCaret = lineStart;

  // Find the text snippet the line we search for begins with for determining
  // the indentation needed.
  size_t currentLine = 1;
  wxCoord indentPixels = 0;

  for (const auto &textSnippet : m_styledText) {
    if (currentLine >= lin)
      break;
    if ((textSnippet.GetText() == '\n') || (textSnippet.GetText() == '\r')) {
      indentPixels = textSnippet.GetIndentPixels();
      currentLine++;
    }
  }

  // Handle indentation
  posInCell.x -= indentPixels;

  while (positionOfCaret < (signed)text.Length() &&
         text.GetChar(positionOfCaret) != '\n' &&
         text.GetChar(positionOfCaret) != '\r') {
    wxCoord width;
    width =
      GetTextSize(text.SubString(lineStart, GetSelection().CursorPosition())).GetWidth();
    if (width > posInCell.x)
      break;
    positionOfCaret++;
  }
  positionOfCaret = wxMin(positionOfCaret, (signed)text.Length());
  if (positionOfCaret < -1)
    positionOfCaret = -1;
  return !((GetSelection().Start() >= positionOfCaret) ||
           (m_selectionEnd <= positionOfCaret));
}

wxString EditorCell::DivideAtCaret() {
  wxString original = m_text;
  m_containsChanges = true;
  wxString newText = m_text.SubString(0, GetSelection().CursorPosition() - 1);

  // Remove an eventual newline from the end of the old cell
  // that would appear if the cell is divided at the beginning of a line.
  if (newText.Length() > 0) {
    // Search for the end of whitespace at the end of the new cell
    size_t whiteSpaceEnd = newText.Length() - 1;
    while ((whiteSpaceEnd < newText.Length()) &&
           ((newText[whiteSpaceEnd] == wxS(' ')) ||
            (newText[whiteSpaceEnd] == wxS('\t'))))
      whiteSpaceEnd++;

    if ((newText[whiteSpaceEnd] == wxS('\n')) ||
        (newText[whiteSpaceEnd] == wxS('\r')))
      newText = newText.SubString(0, whiteSpaceEnd - 1);
  }

  SetValue(newText);
  wxString retval = original.SubString(GetSelection().CursorPosition(), original.Length());
  // Remove an eventual newline from the beginning of a new cell
  // that would appear if the cell is divided at the end of a line.
  if (retval.Length() > 0) {
    // Search for the end of whitespace at the beginning of the new cell
    size_t whiteSpaceEnd = 0;
    while ((whiteSpaceEnd < retval.Length()) &&
           ((retval[whiteSpaceEnd] == wxS(' ')) ||
            (retval[whiteSpaceEnd] == wxS('\t'))))
      whiteSpaceEnd++;

    if ((retval[whiteSpaceEnd] == wxS('\n')) ||
        (retval[whiteSpaceEnd] == wxS('\r')))
      retval = retval.SubString(whiteSpaceEnd + 1, retval.Length());
    m_containsChanges = true;
  }
  return retval;
}

void EditorCell::SetSelection(long start, long end) {
  if ((start != m_oldSelectionStart) || (end != m_oldSelectionEnd)) {
    m_oldSelectionStart = start;
    m_oldSelectionEnd = end;
    m_selectionChanged = true;
    GetSelection().SetSelection(start, end);
    if (!GetSelection().IsActive() || m_selectionEnd == -1)
      m_cellPointers->m_selectionString = wxEmptyString;
    else
      m_cellPointers->m_selectionString =
	m_text.SubString(GetSelection().Left(),
			 GetSelection().Right() - 1);
    m_cellPointers->m_selectionString.Replace(wxS('\r'), wxS(' '));
  }
}

void EditorCell::CommentSelection() {
  if (!GetSelection().IsActive())
    return;
  m_containsChanges = true;
  m_isDirty = true;
  SetValue(m_text.SubString(0, m_selectionStart - 1) + wxS("/*") +
           m_text.SubString(m_selectionStart, m_selectionEnd - 1) + wxS("*/") +
           m_text.SubString(m_selectionEnd, m_text.Length()));
  GetSelection().CursorPosition(wxMin(m_selectionEnd + 4, m_text.Length()));
  ClearSelection();
}

wxString EditorCell::GetWordUnderCaret() {
  size_t start = GetSelection().CursorPosition();
  if (start >= m_text.Length())
    start = m_text.Length();

  wxString retval;
  size_t pos = 0;
  for (wxString::const_iterator it = m_text.begin(); it != m_text.end(); ++it) {
    if (!wxIsalnum(*it) && !(*it == '\\') && !(*it == '_') && !(*it == '&') &&
        !(*it == '%') && !(*it == '?')) {
      if (pos >= start)
        break;
      else
        retval = wxEmptyString;
    } else
      retval += *it;

    pos++;

    if (*it == '\\') {
      ++it;
      if (it != m_text.end()) {
        retval += *it;
        pos++;
      }
    }
  }
  if (retval.IsEmpty())
    if (!m_text.IsEmpty() && start < m_text.size())
      retval = wxString(m_text.GetChar(start));

  return retval;
}

/***
 * SelectWordUnderCaret
 * - called from MathCtrl::OnDoubleClick, MathCtrl::Autocomplete and
 * wxMaxima::HelpMenu Selects word under cursor (aA-zZ, 0-9, %, _, count) or the
 * inside of brackets using m_paren1 and m_paren2 if available and selectParens
 * is true. Returns the selected string if selected a word successfully - used
 * for F1 help and MathCtrl::Autocomplete.
 */
wxString EditorCell::SelectWordUnderCaret(bool WXUNUSED(selectParens),
                                          bool toRight,
                                          bool includeDoubleQuotes) {
  size_t start = 0;
  size_t pos = 0;
  for (wxString::const_iterator it = m_text.begin(); it != m_text.end(); ++it) {
    if (*it == '\\') {
      pos++;
      if (it != m_text.end()) {
        ++it;
        pos++;
      }
      continue;
    }
    if (!wxIsalnum(*it) && !(*it == '\\') && !(*it == '_') && !(*it == '?') &&
        !(*it == '%') && !((*it == '\"') && includeDoubleQuotes)) {
      // !!toRight is 0, if toRight is false or guaranteed to be 1, if toRight
      // is true
      if (pos >= GetSelection().CursorPosition() + !!toRight)
        break;
      else
        start = pos + 1;
    }
    pos++;
  }
  if (pos > 0)
    SetSelection(start, pos);

  if (pos > 0 && start != pos)
    return m_cellPointers->m_selectionString;
  else
    return wxS("%");
}

bool EditorCell::CopyToClipboard() const {
  if ((m_selectionStart < 0) && (m_selectionEnd < 0))
    return false;
  wxASSERT_MSG(!wxTheClipboard->IsOpened(),
               _("Bug: The clipboard is already opened"));
  size_t start = wxMax(wxMin(m_selectionStart, m_selectionEnd), 0);
  size_t end = wxMax(m_selectionStart, m_selectionEnd) - 1;
  if (end > m_text.Length())
    end = m_text.Length();
  wxString s = m_text.SubString(start, end);
  // Copying non-breakable spaces in code to external applications is likely to
  // cause problems
  if (GetType() == MC_TYPE_INPUT)
    s.Replace(wxS("\u00a0"), wxS(" "));
  if (!s.IsEmpty() && (wxTheClipboard->Open())) {
    if (!wxTheClipboard->SetData(new wxTextDataObject(s))) {
      wxLogMessage(_("Cannot put the copied text on the clipboard (1st try)"));
      wxMicroSleep(500000);
      if (!wxTheClipboard->SetData(new wxTextDataObject(s))) {
        wxLogMessage(
		     _("Cannot put the copied text on the clipboard (2nd try)"));
        wxMicroSleep(500000);
        if (!wxTheClipboard->SetData(new wxTextDataObject(s)))
          wxLogMessage(_("Cannot put the copied text on the clipboard"));
      }
    }
    wxTheClipboard->Close();
    return true;
  } else
    return false;
}

bool EditorCell::CutToClipboard() {
  if (!GetSelection().IsActive())
    return false;

  SaveValue();
  m_saveValue = true;
  m_containsChanges = true;
  if (!CopyToClipboard())
    return false;

  size_t start = wxMin(m_selectionStart, m_selectionEnd);
  size_t end = wxMax(m_selectionStart, m_selectionEnd);
  GetSelection().CursorPosition(start);

  // We cannot use SetValue() here, since SetValue() tends to move the cursor.
  m_text =
    m_text.SubString(0, start - 1) + m_text.SubString(end, m_text.Length());
  StyleText();

  ClearSelection();
  m_paren1 = m_paren2 = -1;
  m_width = m_height = m_center = -1;
  InvalidateMaxDrop();

  return true;
}

void EditorCell::InsertText(wxString text) {
  SaveValue();
  m_saveValue = true;
  m_containsChanges = true;

  text =
    TabExpand(text, GetSelection().CursorPosition() - BeginningOfLine(GetSelection().CursorPosition()));

  ReplaceSelection(GetSelectionString(), text);

  if (GetType() == MC_TYPE_INPUT)
    FindMatchingParens();

  m_text.Replace(wxS("\u2028"), "\n");
  m_text.Replace(wxS("\u2029"), "\n");

  //  m_width = m_height = m_center = -1;
  //  InvalidateMaxDrop();
  StyleText();
}

void EditorCell::PasteFromClipboard(const bool primary) {
  wxTheClipboard->UsePrimarySelection(primary);
  wxASSERT_MSG(
	       wxTheClipboard->IsOpened(),
	       _("Bug: The clipboard isn't open on pasting into an editor cell"));
  if (wxTheClipboard->IsSupported(wxDF_TEXT) ||
      wxTheClipboard->IsSupported(wxDF_UNICODETEXT)) {
    wxTextDataObject obj;
    wxTheClipboard->GetData(obj);
    InsertText(obj.GetText());
    m_containsChanges = true;
    StyleText();
  }
  if (primary)
    wxTheClipboard->UsePrimarySelection(false);
}

size_t EditorCell::GetLineWidth(size_t line, size_t pos) {
  // Find the text snippet the line we search for begins with for determining
  // the indentation needed.
  size_t currentLine = 1;
  wxCoord indentPixels = 0;

  for (const auto &textSnippet : m_styledText) {
    if (currentLine > line)
      break;
    if ((textSnippet.GetText() == '\n') || (textSnippet.GetText() == '\r')) {
      indentPixels = textSnippet.GetIndentPixels();
      currentLine++;
    }
  }

  if (pos == 0) {
    return indentPixels;
  }

  size_t i = 0;

  std::vector<StyledText>::const_iterator textSnippet;
  for (textSnippet = m_styledText.begin();
       (textSnippet < m_styledText.end()) && (i < line); ++textSnippet) {
    wxString text = textSnippet->GetText();
    if ((text.Right(1) == '\n') || (text.Right(1) == '\r'))
      i++;
  }

  if (i < line)
    return 0;

  SetFont(m_configuration->GetRecalcDC());
  wxCoord width = 0;
  wxString text;
  wxCoord textWidth = 0;
  pos--;
  for (; (textSnippet < m_styledText.end()) && (pos >= 0); ++textSnippet) {
    text = textSnippet->GetText();
    textWidth = GetTextSize(text).GetWidth();
    width += textWidth;
    pos -= text.Length();
  }

  if (pos < 0) {
    width -= textWidth;
    textWidth = GetTextSize(text.SubString(0, text.Length() + pos)).GetWidth();
    width += textWidth;
  }

  // Handle indentation
  width += indentPixels;

  return width;
}

void EditorCell::SetState(const HistoryEntry &state) {
  m_text = state.text;
  StyleText();
  GetSelection.CursorPosition(state.caretPosition);
  SetSelection(state.selStart, state.selEnd);
}

void EditorCell::AppendStateToHistory() {
  m_history.emplace_back(m_text, GetSelection.CursorPosition(), GetSelection.Start(),
                         GetSelection.End());
}

bool EditorCell::IsActive() const {
  return this == m_cellPointers->m_activeCell;
}

bool EditorCell::CanUndo() const {
  return !m_history.empty() && m_historyPosition != 0;
}

void EditorCell::Undo() {
  if (m_historyPosition == -1) {
    m_historyPosition = m_history.size() - 1;
    AppendStateToHistory();
  } else
    m_historyPosition--;

  if (m_historyPosition == -1)
    return;

  // We cannot use SetValue() here, since SetValue() tends to move the cursor.
  SetState(m_history[m_historyPosition]);

  m_paren1 = m_paren2 = -1;
  m_isDirty = true;
  m_width = m_height = m_center = -1;
  InvalidateMaxDrop();
}

bool EditorCell::CanRedo() const {
  return !m_history.empty() && m_historyPosition >= 0 &&
    m_historyPosition < (m_history.size()) - 1;
}

void EditorCell::Redo() {
  if (m_historyPosition == -1)
    return;

  m_historyPosition++;

  if (m_historyPosition >= m_history.size())
    return;

  // We cannot use SetValue() here, since SetValue() tends to move the cursor.
  SetState(m_history[m_historyPosition]);

  m_paren1 = m_paren2 = -1;
  m_isDirty = true;
  m_width = m_height = m_center = -1;
  InvalidateMaxDrop();
}

void EditorCell::SaveValue() {
  if (!m_history.empty() && m_history.back().text == m_text)
    return;

  if (m_historyPosition != -1) {
    m_history.erase(m_history.begin() + m_historyPosition, m_history.end());
  }

  AppendStateToHistory();
  m_historyPosition = -1;
}

void EditorCell::ClearUndo() {
  m_history.clear();
  m_historyPosition = -1;
}

void EditorCell::HandleSoftLineBreaks_Code(
					   StyledText *&lastSpace, wxCoord &lineWidth, const wxString &token,
					   size_t charInCell, wxString &text, size_t const &lastSpacePos,
					   wxCoord &indentationPixels) {
  // If we don't want to autowrap code we don't do nothing here.
  // cppcheck-suppress knownConditionTrueFalse
  if (!m_configuration->GetAutoWrapCode())
    return;

  // If this token contains spaces and is followed by a space we will do the
  // line break in the next token.
  if ((charInCell + 1 < text.Length()) && (token.StartsWith(wxS(" "))) &&
      (text[charInCell + 1] == ' '))
    return;

  SetFont(m_configuration->GetRecalcDC());

  wxCoord width;
  //  Does the line extend too much to the right to fit on the screen /
  //   // to be easy to read?
  width = GetTextSize(token).GetWidth();
  lineWidth += width;

  if ((lineWidth + indentationPixels >= m_configuration->GetLineWidth()) &&
      (lastSpace != NULL) && (lastSpace->GetText() != "\r")) {
    wxCoord charWidth;
    charWidth = GetTextSize(" ").GetWidth();
    indentationPixels = charWidth * GetIndentDepth(m_text, lastSpacePos);
    lineWidth = width + indentationPixels;
    lastSpace->SetText("\r");
    lastSpace->SetIndentation(indentationPixels);
    text[lastSpacePos] = '\r';
    lastSpace = NULL;
  }
}

void EditorCell::StyleTextCode() {
  // We have to style code
  StyledText *lastSpace = NULL;
  size_t lastSpacePos = 0;
  // If a space is part of the initial spaces that do the indentation of a cell
  // it is not eligible for soft line breaks: It would add a soft line break
  // that causes the same indentation to be introduced in the new line again and
  // therefore would not help at all.
  wxCoord indentationPixels = 0;
  wxString textToStyle = m_text;
  SetFont(m_configuration->GetRecalcDC());
  wxString suppressedLinesInfo;

  // Handle folding of EditorCells
  if (m_firstLineOnly) {
    size_t newlinepos = textToStyle.Find(wxS("\n"));
    if (newlinepos != wxNOT_FOUND) {
      size_t lines = textToStyle.Freq(wxS('\n'));
      textToStyle = textToStyle.Left(newlinepos);
      if (lines > 1)
        suppressedLinesInfo = 
	  wxString::Format(_(" ... + %i hidden lines"), lines);
      else
        suppressedLinesInfo = _(" ... + 1 hidden line");
    }
  }

  // Split the line into commands, numbers etc.
  m_tokens = MaximaTokenizer(textToStyle, m_configuration).PopTokens();

  // Now handle the text pieces one by one
  wxString lastTokenWithText;
  size_t pos = 0;
  wxCoord lineWidth = 0;

  for (auto const &token : m_tokens) {
    pos += token.GetText().Length();
    auto &tokenString = token.GetText();
    if (tokenString.IsEmpty())
      continue;
    wxChar Ch = tokenString[0];

    // Handle Spaces
    if (Ch == wxS(' ')) {
      // All spaces except the last one (that could cause a line break)
      // share the same token
      if (tokenString.Length() > 1)
        m_styledText.push_back(
			       StyledText(tokenString.Right(tokenString.Length() - 1)));

      // Now we push the last space to the list of tokens and remember this
      // space as the space that potentially serves as the next point to
      // introduce a soft line break.
      m_styledText.push_back(StyledText(wxS(" ")));
      lastSpace = &m_styledText.back();
      lastSpacePos = pos + tokenString.Length() - 1;
      continue;
    }

    // Most of the other item types can contain Newlines - that we want as
    // separate tokens
    wxString txt = tokenString;
    wxString line;
    for (wxString::const_iterator it2 = txt.begin(); it2 < txt.end(); ++it2) {
      if (*it2 != '\n')
        line += wxString(*it2);
      else {
        if (line != wxEmptyString)
          m_styledText.push_back(StyledText(token.GetTextStyle(), line));
        m_styledText.push_back(StyledText(token.GetTextStyle(), "\n"));
        line = wxEmptyString;
      }
    }
    if (line != wxEmptyString)
      m_styledText.push_back(StyledText(token.GetTextStyle(), line));
    HandleSoftLineBreaks_Code(lastSpace, lineWidth, token, pos, m_text,
                              lastSpacePos, indentationPixels);
    if ((token.GetTextStyle() == TS_CODE_VARIABLE) ||
        (token.GetTextStyle() == TS_CODE_FUNCTION)) {
      m_wordList.push_back(token);
      continue;
    }
  }
  std::sort(m_wordList.begin(), m_wordList.end());
  if(!suppressedLinesInfo.IsEmpty())
    m_styledText.push_back(StyledText(TS_CODE_COMMENT, suppressedLinesInfo));
}

void EditorCell::StyleTextTexts() {
  // Remove all bullets of item lists as we will introduce them again in the
  // next step, as well.
  m_text.Replace(wxS("\u2022"), wxS("*"));

  // Insert new soft line breaks where we hit the right border of the worksheet,
  // if this has been requested in the config dialogue
  if (m_configuration->GetAutoWrap()) {
    SetFont(m_configuration->GetRecalcDC());
    wxString line;
    size_t lastSpacePos = 0;
    wxString::const_iterator lastSpaceIt;
    size_t lastLineStart = 0;
    wxCoord width;

    // Is this a new line - or the remainder of the line after a soft break?
    bool newLine = true;
    std::vector<wxString> prefixes;
    std::vector<wxCoord> indentPixels;
    wxString indentChar;

    size_t i = 0;
    wxCoord indent;
    wxString::const_iterator it = m_text.begin();
    while (it < m_text.end()) {
      // Extract a line inserting a soft linebreak if necessary
      while (it < m_text.end()) {
        wxString::const_iterator nextChar(it);
        ++nextChar;
        // Handle hard linebreaks or indent a soft linebreak if necessary
        if ((*it == '\n') || (nextChar >= m_text.end())) {
          // Can we introduce a soft line break?
          // One of the next questions will be: Do we need to?
          if (lastSpacePos > 0) {
            // How far has the current line to be indented?
            if ((!indentPixels.empty()) && (!newLine))
              indent = indentPixels.back();
            else
              indent = 0;

            // How long is the current line already?
            width = GetTextSize(m_text.SubString(lastLineStart, i)).GetWidth();
            // Do we need to introduce a soft line break?
            if (width + indent >= m_configuration->GetLineWidth()) {
              // We need a line break in front of the last space
              m_text[lastSpacePos] = wxS('\r');
              line = m_text.SubString(lastLineStart, lastSpacePos - 1);
              i = lastSpacePos;
              it = lastSpaceIt;
              lastLineStart = i + 1;
              lastSpacePos = 0;
              break;
            }
          }
          if ((*it == '\n') || (*it == '\r')) {
            if (i > 0)
              line = m_text.SubString(lastLineStart, i - 1);
            else
              line = wxEmptyString;
          } else
            line = m_text.SubString(lastLineStart, i);

          lastLineStart = i + 1;
          lastSpacePos = 0;
          break;
        } else {
          // We cannot introduce soft linebreaks since there were no spaces we
          // could break at.
          //
          // TODO: If we handled spaces before we handled soft line breaks this
          // branch would be unnecessary, right?

          // Spaces, newlines and reaching the end of the text all trigger
          // auto-wrapping
          if ((*it == ' ') || (nextChar >= m_text.end())) {
            // Determine the current line's length
            width = GetTextSize(m_text.SubString(lastLineStart, i)).GetWidth();
            // Determine the current indentation
            if ((!indentPixels.empty()) && (!newLine))
              indent = indentPixels.back();
            else
              indent = 0;

            // Does the line extend too much to the right to fit on the screen /
            // to be easy to read?
            if (width + indent >= m_configuration->GetLineWidth()) {
              // We need a line break. Does the current line contain a space we
              // can break the line at?
              if (lastSpacePos > 0) {
                // Introduce a soft line break
                m_text[lastSpacePos] = wxS('\r');
                line = m_text.SubString(lastLineStart, lastSpacePos - 1);
                i = lastSpacePos + 1;
                it = lastSpaceIt;
                ++it;
                lastLineStart = i;
                lastSpacePos = 0;
                break;
              } else {
                if (*it == wxS(' ')) {
                  m_text[i] = wxS('\r');
                  line = m_text.SubString(lastLineStart, i - 1);
                  lastLineStart = i + 1;
                  lastSpacePos = 0;
                  break;
                }
              }
            }
          }
        }

        // Remember the current space as a point we potentially can break lines
        // at
        if (*it == ' ') {
          lastSpacePos = i;
          lastSpaceIt = it;
        }

        // Go to the next character if we actually had a string in front of this
        // newline.
        if ((i > 0) || (*it != wxS('\n'))) {
          ++it;
          ++i;
        }
      }

      // If this is the last line of the text we still need to extract it.
      if (i == m_text.Length())
        line = m_text.SubString(lastLineStart, i - 1);

      // If we fold the cell we only show the first line of text.
      if (m_firstLineOnly) {
        m_styledText.push_back(
			       StyledText(line + wxString::Format(_(" ... + %i hidden lines"),
								  m_text.Freq(wxS('\n')))));
        break;
      }

      // Determine how much which line has to be indented for bullet lists
      // or citations

      // Handle the start of new lines
      if (newLine) {
        // Let's see if the line begins with a "begin indenting" marker:
        wxString line_trimmed(line);
        line_trimmed.Trim(false);
        if ((line_trimmed.StartsWith(wxS("* "))) ||
            (line_trimmed.StartsWith(wxS("\u2022 "))) ||
            (line_trimmed.StartsWith(wxS("\u00B7 "))) ||
            (line_trimmed.StartsWith(wxS("> ")))) {
          // An "begin indenting" marker

          // Remember what a line that is part of this indentation level has to
          // begin with
          wxCoord w;

          indentChar = line.Left(line.Length() - line_trimmed.Length() + 2);

          // Remember how far to indent subsequent lines
          w = GetTextSize(indentChar).GetWidth();

          // Every line of a Quote begins with a ">":
          if (!line_trimmed.StartsWith(wxS("> ")))
            indentChar = wxEmptyString;

          // Equip bullet lists with real bullets
          if (line_trimmed.StartsWith(wxS("* ")))
            line[line.find("*")] = L'\u2022';
          if (line_trimmed.StartsWith(L"\u00B7 "))
            line[line.find(L"\u00B7")] = L'\u2022';

          // Remember what a continuation for this indenting object would begin
          // with
          prefixes.push_back(wxS("  ") +
                             line.Left(line.Length() - line_trimmed.Length()));
          indentPixels.push_back(w);
        } else {
          // No "begin indenting" marker => Let's see if this is a continuation
          // of an indentation
          while (!prefixes.empty()) {
            if (line.StartsWith(prefixes.back()))
              break;
            prefixes.pop_back();
            indentPixels.pop_back();
          }
          // We don't need indentation as this line was indented
          // by spaces already.
        }
      }

      if (prefixes.empty())
        indentChar = wxEmptyString;

      if ((!indentPixels.empty()) && (!newLine))
        indent = indentPixels.back();
      else
        indent = 0;

      // Equip the last soft linebreak with indentation.
      if (m_styledText.size() > 0) {
        if (m_styledText.back().GetText() == wxS("\r"))
          m_styledText.back().SetIndentation(indent);
      }
      // Store the indented line in the list of styled text snippets
      m_styledText.push_back(StyledText(line, 0, indentChar));

      if (it < m_text.end()) {
        // If the cell doesn't end with the last char of this line we have to
        // add a line ending to the list of styled text snippets
        if ((i + 1 < m_text.Length()) || (m_text[i] == wxS('\n'))) {
          // Store the line ending in the list of styled text snippets
          if (*it == wxS('\n'))
            m_styledText.push_back(StyledText(wxS("\n"), 0, indentChar));
          else
            m_styledText.push_back(StyledText(wxS("\r"), 0, indentChar));
        }
      }

      // Is this a real new line of comment - or did we insert a soft linebreak?
      newLine = ((i + 1 >= m_text.Length()) || (*it == wxS('\n')));

      ++i;
      ++it;
    } // The loop that loops over all lines
  }   // Do we want to autowrap lines?
  else {
    m_text.Replace(wxS("\r"), wxS("\n"));
    wxStringTokenizer lines(m_text, wxS("\n"), wxTOKEN_RET_EMPTY_ALL);
    while (lines.HasMoreTokens()) {
      wxString line = lines.GetNextToken();
      if (m_firstLineOnly) {
        m_styledText.push_back(
			       StyledText(line + wxString::Format(_(" ... + %i hidden lines"),
								  m_text.Freq(wxS('\n'))),
					  0, wxEmptyString));
        break;
      }

      m_styledText.push_back(StyledText(line, 0, wxEmptyString));
      if ((lines.HasMoreTokens()))
        m_styledText.push_back(StyledText(wxS("\n"), 0, wxEmptyString));
    }
  }
} // Style text, not code?

const MaximaTokenizer::TokenList &EditorCell::GetAllTokens() {
  if(m_firstLineOnly)
    {
      if(!m_tokens_including_hidden_valid)
	{
	  m_tokens_including_hidden =
	    MaximaTokenizer(m_text, m_configuration).PopTokens();
	  m_tokens_including_hidden_valid = true;
	}
      return m_tokens_including_hidden;
    }
  else
    {
      if(!m_tokens_valid)
	StyleText();
      return m_tokens;
    }
}

void EditorCell::StyleText() {
  wxASSERT(m_configuration->GetRecalcDC() != NULL);
  if(!m_configuration->GetRecalcDC())
    {
      wxLogMessage(_("Bug: dc == NULL"));
      return;
    }
  ResetSize();
  // We will need to determine the width of text and therefore need to set
  // the font type and size.
  SetFont(m_configuration->GetRecalcDC());

  m_wordList.clear();
  m_styledText.clear();

  if (m_text == wxEmptyString)
    return;

  // Remove all soft line breaks. They will be re-added in the right places
  // in the next step
  m_text.Replace(wxS("\r"), wxS(" "));
  // Do we need to style code or text?
  if (m_type == MC_TYPE_INPUT)
    StyleTextCode();
  else
    StyleTextTexts();
  m_tokens_valid = true;
}

void EditorCell::SetValue(const wxString &text) {
  if (m_type == MC_TYPE_INPUT) {
    if (m_configuration->GetMatchParens()) {
      if (text == wxS("(")) {
        m_text = wxS("()");
        GetSelection.CursorPosition(0);
      } else if (text == wxS("[")) {
        m_text = wxS("[]");
        GetSelection.CursorPosition(1);
      } else if (text == wxS("{")) {
        m_text = wxS("{}");
        GetSelection.CursorPosition(1);
      } else if (text == wxS("\"")) {
        m_text = wxS("\"\"");
        m_positionOfCaret = 1;
      } else {
        m_text = text;
        GetSelection.CursorPosition(m_text.Length());
      }
    } else {
      m_text = text;
        GetSelection.CursorPosition(m_text.Length());
    }

    if (m_configuration->GetInsertAns()) {
      if (m_text == wxS("+") || m_text == wxS("*") || m_text == wxS("/") ||
          m_text == wxS("^") || m_text == wxS("=") || m_text == wxS(",")) {
        m_text = wxS("%") + m_text;
        GetSelection.CursorPosition(m_text.Length());
      }
    }
  } else {
    m_text = text;
        GetSelection.CursorPosition(m_text.Length());
  }

  FindMatchingParens();
  m_containsChanges = true;

  m_text.Replace(wxS("\u2028"), "\n");
  m_text.Replace(wxS("\u2029"), "\n");

  // Style the text.
  StyleText();
}

bool EditorCell::CheckChanges() {
  if (m_containsChanges != m_containsChangesCheck) {
    m_containsChangesCheck = m_containsChanges;
    return true;
  }

  return false;
}

size_t EditorCell::ReplaceAll(wxString oldString, const wxString &newString,
                           bool ignoreCase) {
  if (oldString == wxEmptyString)
    return 0;

  SaveValue();
  wxString newText;
  size_t count = 0;
  if (!ignoreCase) {
    newText = m_text;
    newText.Replace(wxS("\r"), wxS(" "));
    count = newText.Replace(oldString, newString);
  } else {
    size_t pos;
    wxString src = m_text;
    src.Replace(wxS("\r"), wxS(" "));
    wxString src_LowerCase = src;
    src_LowerCase.MakeLower();
    oldString.MakeLower();
    pos = src_LowerCase.Find(oldString);
    while (pos >= 0) {
      newText += src.Left(pos);
      newText += newString;
      size_t charsToCopy = src.Length() - pos - oldString.Length();
      src_LowerCase = src_LowerCase.Right(charsToCopy);
      src = src.Right(charsToCopy);
      count++;
      pos = src_LowerCase.Find(oldString);
    }
    newText += src;
  }
  if (count > 0) {
    m_text = newText;
    m_containsChanges = true;
    ClearSelection();
    StyleText();
  }

  // If text is selected setting the selection again updates m_selectionString
  if (m_selectionStart > 0)
    SetSelection(m_selectionStart, m_selectionEnd);

  m_text.Replace(wxS("\u2028"), "\n");
  m_text.Replace(wxS("\u2029"), "\n");

  return count;
}

size_t EditorCell::ReplaceAll_RegEx(wxString oldString, const wxString &newString) {
  if (oldString == wxEmptyString)
    return 0;

  SaveValue();
  wxString newText;
  size_t count = 0;
  RegexSearch regexsearch(oldString);
  newText = m_text;
  newText.Replace(wxS("\r"), wxS(" "));
  count = regexsearch.ReplaceAll(&newText, newString);
  if(count > 0)
    {
      m_text = newText;
      m_containsChanges = true;
      ClearSelection();
      StyleText();
      if (m_selectionStart > 0)
	SetSelection(m_selectionStart, m_selectionEnd);
    }
  if (count > 0) {
    m_text = newText;
    m_containsChanges = true;
    ClearSelection();
    StyleText();
  }
  
  // If text is selected setting the selection again updates m_selectionString
  if (m_selectionStart > 0)
    SetSelection(m_selectionStart, m_selectionEnd);
  
  m_text.Replace(wxS("\u2028"), "\n");
  m_text.Replace(wxS("\u2029"), "\n");
return count;
}


bool EditorCell::FindNext(wxString str, const bool &down,
                          const bool &ignoreCase) {
  // If the search string is empty we prepare everything for a new search
  if (str.IsEmpty()) {
    if (down)
      GetSelection.CursorPosition(0);
    else
      GetSelection.CursorPosition(m_text.Length());
    return false;
  }

  // Default to start the search at the right end of the cell
  size_t start;
  if (down)
    start = 0;
  else
    start = m_text.Length();

  // Handle soft line breaks and ignore-case
  wxString text(m_text);
  text.Replace(wxS('\r'), wxS(' '));
  if (ignoreCase) {
    str.MakeLower();
    text.MakeLower();
  }

  // If this cell is already active we might already be at a suitable
  // start position for the search or within a search.
  if (IsActive()) {
    // If the last search already has marked a match for our word we want
    // to search for the next match.
    if ((static_cast<size_t>(abs(m_selectionStart - m_selectionEnd)) == str.Length()) &&
        (text.Right(text.Length() - wxMin(m_selectionStart, m_selectionEnd))
	 .StartsWith(str))) {
      if (down)
        start = GetSelection().Left() + 1;
      else
        start = GetSelection().Right() - 1;
      if((start < 0) || (start >= m_text.Length()))
	return false;
    } else {
      // We are at the start of a match, but the search expression has changed
      if (GetSelection().Start() > 0) {
        if (down)
          start = GetSelection().Left() + 1;
        else
          start = GetSelection().Right() - 1;
	if((start < 0) || (start >= m_text.Length()))
	  return false;
      } else {
	start = GetSelection().CursorPosition();
      }
    }
  } else { // Inactive cell => try to make sure we start at a sane position
    if (down) {
      GetSelection().CursorPosition(0);
    } else {
      GetSelection().CursorPosition(m_text.Length());
    }
  }
  size_t strStart = wxNOT_FOUND;
  if (down)
    strStart = text.find(str, start);
  else
    strStart = text.rfind(str, start);

  if (strStart != wxNOT_FOUND) {
    if (down)
      GetSelection().SetSelection(strStart, strStart + str.Length());
    else
      GetSelection().SetSelection(strStart + str.Length(), strStart);
    return true;
  }
  if (IsActive()) {
    if (down) {
      GetSelection().CursorPosition(0);
    } else {
      GetSelection().CursorPosition(m_text.Length());
    }
  }
  return false;
}


bool EditorCell::FindNext_RegEx(wxString str, const bool &down) {
  wxString text(m_text);
  text.Replace(wxS('\r'), wxS(' '));

  RegexSearch regexSearch(str);
  RegexSearch::Match match;

  size_t start;
  if (down)
    start = 0;
  else
    start = m_text.Length();
  
  // If this cell is already active we might already be at a suitable
  // start position for the search or within a search.
  if (IsActive()) {
    // If the last search already has marked a match for our word we want
    // to search for the next match.
    if (
        (GetSelection().SelectionLength() == str.Length()) &&
        (text.Right(text.Length() - GetSelection().GetLeft())
	 .StartsWith(str))) {
      if (down)
        start = wxMin(m_selectionStart, m_selectionEnd) + 1;
      else
        start = wxMax(m_selectionStart, m_selectionEnd) - 1;
      if((start < 0) || (start >= m_text.Length()))
	return false;
    } else {
      // We are at the start of a match, but the search expression has changed
      if (m_selectionStart > 0) {
        if (down)
          start =GetSelection().Left() + 1;
        else
          start = GetSelection().Right() - 1;
	if((start < 0) || (start >= m_text.Length()))
	  return false;
      } else {
	start = GetSelection().CursorPosition();
      }
    }
  }
  
  if (down)
    match =  regexSearch.FindNext(text, start);
  else
    match =  regexSearch.FindNext_Reverse(text, start);
  if(match.GetStart() != wxNOT_FOUND)
    {
      SetSelection(match.GetStart(), match.GetEnd());
      return true;
    }
  if (IsActive()) {
    if (down) {
      GetSelection().CursorPosition(0);
    } else {
      GetSelection().CursorPosition(m_text.Length());
    }
  }
  return false;
}

bool EditorCell::ReplaceSelection(const wxString &oldStr,
                                  const wxString &newString, bool keepSelected,
                                  bool ignoreCase, bool replaceMaximaString) {
  wxString text(m_text);
  text.Replace(wxS("\r"), wxS(" "));

  auto start = GetSelection().Left();
  auto end = GetSelection().Right();
  if (m_selectionStart < 0) {
    if (oldStr == wxEmptyString)
      GetSelection().ClearSelection();
    else
      return false;
  }

  if (ignoreCase) {
    if (text.SubString(start, end - 1).Upper() != wxString(oldStr).Upper())
      return false;
  } else {
    if (text.SubString(start, end - 1) != oldStr)
      return false;
  }

  // We cannot use SetValue() here, since SetValue() tends to move the cursor.
  wxString text_left = text.SubString(0, start - 1);
  wxString text_right = text.SubString(end, text.Length());
  m_text = text_left + newString + text_right;
  StyleText();

  m_containsChanges = true;
  GetSelection().CursorPosition(start + newString.Length());

  if (replaceMaximaString) {
    if ((newString.EndsWith("\"") || (text_right.StartsWith("\"")))) {
      if (!((newString.EndsWith("\"") && (text_right.StartsWith("\"")))))
        GetSelection().StepCursor(-1);
    }
  }

  if (keepSelected)
    GetSelection().Start(start);
  else
    GetSelection().ClearSelection();

  if (GetType() == MC_TYPE_INPUT)
    FindMatchingParens();

  StyleText();
  return true;
}


bool EditorCell::ReplaceSelection_RegEx(const wxString &oldStr,
					const wxString &newString) {
  wxString text(m_text);
  text.Replace(wxS("\r"), wxS(" "));

  auto start = GetSelection().Left();
  auto end   = GetSelection().Right();

  RegexSearch regexSearch(oldStr);
  RegexSearch::Match match;
  match =  regexSearch.Replace(&text, start, newString);
  if(match.GetStart() == wxNOT_FOUND)
    return false;
  m_text = text;
  GetSelection().CursorPosition(match.GetEnd());
  
  if (GetType() == MC_TYPE_INPUT)
    FindMatchingParens();

  StyleText();
  return true;
}

wxString EditorCell::GetSelectionString() const {
  if (m_selectionStart >= 0)
    return m_cellPointers->m_selectionString;
  else
    return wxEmptyString;
}

TextStyle EditorCell::GetSelectionStyle() const {
  size_t pos = 0;

  if (SelectionActive()) {
    for (const auto &textSnippet: m_styledText) {
      wxString text = textSnippet.GetText();
      if ((wxMin(m_selectionStart, m_selectionEnd) <= pos) &&
          (pos + text.Length() < wxMax(m_selectionStart, m_selectionEnd)) &&
          (wxMax(m_selectionStart, m_selectionEnd) >= 0)) {
        if (textSnippet.IsStyleSet())
          return textSnippet.GetTextStyle();
      }
      if (pos > m_selectionEnd)
        return TS_INVALID;
      pos += text.Length();
    }
  } else {
    for (const auto &textSnippet: m_styledText) {
      wxString text = textSnippet.GetText();
      if ((GetSelection().CursorPosition() >= pos) &&
	  (GetSelection().CursorPosition() < pos + text.Length()) &&
	  (GetSelection().CursorPosition() >= 0) &&
	  (pos + text.Length() >= 0)) {
        if (textSnippet.IsStyleSet())
          return textSnippet.GetTextStyle();
      }
      if (pos > m_selectionEnd)
        return TS_INVALID;
      pos += text.Length();
    }
  }
  return TS_INVALID;
}

void EditorCell::ClearSelection() {
  if (SelectionActive()) {
    m_selectionChanged = true;
    m_cellPointers->m_selectionString = wxEmptyString;
    m_oldSelectionStart = m_oldSelectionEnd = m_selectionStart =
      m_selectionEnd = -1;
  }
}

/***
 * FindNextTemplate selects the next template
 * of moves the cursor behind the first closing
 * paren in the current line.
 */
bool EditorCell::FindNextTemplate(bool left) {
  static wxRegEx leftVarsRegex, rightVarsRegex;
  static const bool leftVarsRegexOk =
    leftVarsRegex.Compile(wxS("(<[^> \n]+>)[^>]*$"));
  static const bool rightVarsRegexOk =
    rightVarsRegex.Compile(wxS("(<[^> \n]+>)"));

  wxASSERT(leftVarsRegexOk);
  wxASSERT(rightVarsRegexOk);

  const wxRegEx &varsRegex = left ? leftVarsRegex : rightVarsRegex;

  size_t positionOfCaret = GetSelection().CursorPosition();
  if (!left && m_selectionEnd != -1)
    positionOfCaret = m_selectionEnd;

  // Splits the string into first (from caret in the direction of search)
  // and second (the rest of the string)
  wxString first, second;
  if (left) {
    first = m_text.Mid(0, positionOfCaret);
    second = m_text.Mid(positionOfCaret);
  } else {
    first = m_text.Mid(positionOfCaret);
    second = m_text.Mid(0, positionOfCaret);
  }

  size_t start, length;

  // First search in the direction of search
  if (varsRegex.Matches(first)) {
    varsRegex.GetMatch(&start, &length, 1);
    if (left) {
      GetSelection.Start(start);
    }
    GetSelection.SelectionLength(length);
    return true;
  }

  // Then in the rest of the string
  if (varsRegex.Matches(second)) {
    varsRegex.GetMatch(&start, &length, 1);
    if (!left) {
      GetSelection().Start(start);
    }
    GetSelection().SelectionLength(length);
    return true;
  }

  return false;
}

void EditorCell::CaretToEnd() {
  GetSelection().CursorPosition(m_text.Length());
  if (GetType() == MC_TYPE_INPUT)
    FindMatchingParens();
}

void EditorCell::CaretToStart() {
  GetSelection().CursorPosition(m_text.Length());
  if (GetType() == MC_TYPE_INPUT)
    FindMatchingParens();
}

void EditorCell::CaretToPosition(size_t pos) {
  GetSelection().CursorPosition(pos);
  if (GetType() == MC_TYPE_INPUT)
    FindMatchingParens();
}

#if wxUSE_ACCESSIBILITY
wxAccStatus EditorCell::GetDescription(int childId,
                                       wxString *description) const {
  if (childId != 0)
    return wxACC_FAIL;

  if (description == NULL)
    return wxACC_FAIL;

  switch (GetType()) {
  case MC_TYPE_INPUT:
    *description = _("Maxima code");
    break;
    //  case MC_TYPE_CHAPTER:
    //    *description = _("A chapter heading");
    //    break;
  case MC_TYPE_SECTION:
    *description = _("A section heading");
    break;
  case MC_TYPE_SUBSECTION:
    *description = _("A subsection heading");
    break;
  case MC_TYPE_SUBSUBSECTION:
    *description = _("A sub-subsection heading");
    break;
  case MC_TYPE_HEADING5:
    *description = _("A sub-sub-subsection heading");
    break;
  case MC_TYPE_HEADING6:
    *description = _("A sub-sub-sub-subsection heading");
    break;
  case MC_TYPE_TEXT:
    *description =
      _("Comment (ordinary worksheet text that isn't fed to maxima)");
    break;
  default:
    *description = _("Bug: Unknown type of text");
    break;
  }
  return wxACC_OK;
}

wxAccStatus EditorCell::GetDefaultAction(int WXUNUSED(childId),
                                         wxString *actionName) const {
  if (actionName != NULL) {
    *actionName = _("Type in text");
    return wxACC_OK;
  }
  return wxACC_FAIL;
}

wxAccStatus EditorCell::GetValue(int WXUNUSED(childId),
                                 wxString *strValue) const {
  wxString retval = ToString();

  // If the blinking caret is currently visible we hide the char under the caret
  if ((m_displayCaret) && (GetSelection().CursorPosition() > 0)) {
    if ((GetSelection().CursorPosition() < retval.Length())) {
      if (retval[GetSelection().CursorPosition()] == wxS(' '))
        retval[GetSelection().CursorPosition()] = wxS('%');
      else
        retval[GetSelection().CursorPosition()] = wxS(' ');
    } else
      retval += wxS("%");
  }
  *strValue = retval;
  return wxACC_OK;
}

wxAccStatus EditorCell::GetFocus(int *childId, Cell **child) const {
  if (IsActive()) {
    if (child != NULL)
      *child = const_cast<EditorCell *>(this);
    if (childId != NULL)
      *childId = 0;
    return wxACC_OK;
  } else {
    if (child != NULL)
      *child = NULL;
    if (childId != NULL)
      *childId = 0;
    return wxACC_FAIL;
  }
}

wxAccStatus EditorCell::GetRole(int childId, wxAccRole *role) const {
  if ((childId == 0) && (role != NULL)) {
    *role = wxROLE_SYSTEM_TEXT;
    return wxACC_OK;
  } else {
    return wxACC_FAIL;
  }
}

#endif

/* const std::vector<EditorCell::StyledText> &EditorCell::GetStyledText()
{
  if(!m_tokens_valid);
  {
    StyleText();
    m_tokens_valid = true;
  }
  return m_styledText;
}
*/
