// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2012-2013 Doug Ilijev <doug.ilijev@gmail.com>
//            (C) 2015      Gunter Königsmann <wxMaxima@physikbuch.de>
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
  This file defines the class FindReplaceDialog

  This dialog represents the "find" and "replace" dialogue.
*/

#ifndef REGEXSEARCH_H
#define REGEXSEARCH_H

#include "precomp.h"
#include <wx/regex.h>

/*! The find+replace dialog
 */
class RegexSearch : public wxRegEx
{
public:
  class Match
  {
  public:
    Match(){m_start=wxNOT_FOUND;m_length = 0;}
    void SetStart(size_t start){m_start = start;}
    void SetLength(size_t length){m_length = length;}
    long GetStart(){return m_start;}
    long GetLength(){return m_length;}
    long GetEnd(){return m_start + m_length;}
  private:
    long m_start;
    long m_length;
  };
  RegexSearch(wxString regex);
  ~RegexSearch();
  Match FindNext(wxString string, size_t start);
  Match FindNext_Reverse(wxString string, size_t start);
  Match Replace(wxString string, size_t start, wxString replacement);
  Match Replace_Reverse(wxString string, size_t start, wxString replacement);
};

#endif // REGEXSEARCH_H
