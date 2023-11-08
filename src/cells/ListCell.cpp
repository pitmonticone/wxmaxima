// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode:
// nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2014-2018 Gunter Königsmann <wxMaxima@physikbuch.de>
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
  This file defines the class ListCell

  ListCell is the Cell type that represents a list of math elements.
*/

#include "ListCell.h"
#include "CellImpl.h"

ListCell::ListCell(GroupCell *group, Configuration *config,
                   std::unique_ptr<Cell> &&inner)
    : Cell(group, config),
      m_open(std::make_unique<TextCell>(group, config, wxS("["))),
      m_innerCell(std::move(inner)),
      m_close(std::make_unique<TextCell>(group, config, wxS("]"))) {
    SetStyle(TS_VARIABLE);

    // It is valid to construct this cell this with a null inner cell
    if (!m_innerCell)
        m_innerCell = std::make_unique<TextCell>(group, config);
    // Tell the first of our inner cells not to begin with a multiplication dot.
    m_innerCell->SetSuppressMultiplicationDot(true);

    m_open->SetStyle(TS_FUNCTION);
    m_close->SetStyle(TS_FUNCTION);
}

// These false-positive warnings only appear in old versions of cppcheck
// that don't fully understand constructor delegation, still.
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_last1
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_print
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_numberOfExtensions
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_charWidth
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_charHeight
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_charWidth1
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_charHeight1
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_signWidth
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_signHeight
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_signTopHeight
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_signBotHeight
// cppcheck-suppress uninitMemberVar symbolName=ListCell::m_extendHeight
ListCell::ListCell(GroupCell *group, const ListCell &cell)
    : ListCell(group, cell.m_configuration,
               CopyList(group, cell.m_innerCell.get())) {
    CopyCommonData(cell);
}

DEFINE_CELL(ListCell)

void ListCell::Recalculate(AFontSize fontsize) {
    m_innerCell->RecalculateList(fontsize);
    m_open->RecalculateList(fontsize);
    m_close->RecalculateList(fontsize);

    m_signWidth = m_open->GetWidth();

    // If our font provides all the unicode chars we need we don't need
    // to bother which exotic method we need to use for drawing nice parenthesis.
    if (1.2 * m_open->GetHeight() >= m_innerCell->GetHeightList()) {
        m_drawAsAscii = true;
        m_signHeight = m_open->GetHeight();
    } else {
        m_drawAsAscii = false;
        m_signHeight = m_innerCell->GetHeightList();
    }

    if (IsBrokenIntoLines()) {
        m_width = 0;
        m_height = 0;
        m_center = 0;
    } else {
        m_width = m_innerCell->GetFullWidth() + m_signWidth * 2;
        m_height = wxMax(m_signHeight, m_innerCell->GetHeightList()) + Scale_Px(4);
        m_center = m_height / 2;
    }
    Cell::Recalculate(fontsize);
}

void ListCell::Draw(wxPoint point, wxDC *dc, wxDC *antialiassingDC) {
    Cell::Draw(point, dc, antialiassingDC);
    if (DrawThisCell(point)) {
        wxPoint innerCellPos(point);

        if (m_drawAsAscii) {
            innerCellPos.x += m_open->GetWidth();
            m_open->DrawList(point, dc, antialiassingDC);
            m_close->DrawList(wxPoint(
                                  point.x + m_open->GetWidth() + m_innerCell->GetFullWidth(),
                                  point.y), dc, antialiassingDC);
        } else {
            innerCellPos.y +=
                (m_innerCell->GetCenterList() - m_innerCell->GetHeightList() / 2);
            SetPen(antialiassingDC, 1.5);

            int signWidth = m_signWidth - Scale_Px(2);
            innerCellPos.x = point.x + m_signWidth;

            // Left bracket
            const wxPoint pointsL[4] = {
                {point.x - Scale_Px(1) + signWidth, point.y - m_center + Scale_Px(4)},
                {point.x + Scale_Px(1), point.y - m_center + Scale_Px(4)},
                {point.x + Scale_Px(1), point.y + m_center - Scale_Px(4)},
                {point.x - Scale_Px(1) + signWidth,
                 point.y + m_center - Scale_Px(4)}};
            antialiassingDC->DrawLines(4, pointsL);

            // Right bracket
            const wxPoint pointsR[4] = {
                {point.x + m_width + Scale_Px(1) - signWidth,
                 point.y - m_center + Scale_Px(4)},
                {point.x + m_width - Scale_Px(1), point.y - m_center + Scale_Px(4)},
                {point.x + m_width - Scale_Px(1), point.y + m_center - Scale_Px(4)},
                {point.x + m_width + Scale_Px(1) - signWidth,
                 point.y + m_center - Scale_Px(4)}};
            antialiassingDC->DrawLines(4, pointsR);
        }

        m_innerCell->DrawList(innerCellPos, dc, antialiassingDC);
    }
}

wxString ListCell::ToString() const {
    wxString s;
    if (!m_innerCell)
        return wxS("[]");

    if (!IsBrokenIntoLines())
        s = wxS("[") + m_innerCell->ListToString() + wxS("]");
    return s;
}

wxString ListCell::ToMatlab() const {
    wxString s;
    if (!IsBrokenIntoLines())
        s = wxS("[") + m_innerCell->ListToMatlab() + wxS("]");
    return s;
}

wxString ListCell::ToTeX() const {
    wxString s;
    if (!IsBrokenIntoLines()) {
        wxString innerCell = m_innerCell->ListToTeX();

        // Let's see if the cell contains anything potentially higher than a normal
        // character.
        bool needsLeftRight = false;
        for (size_t i = 0; i < innerCell.Length(); i++)
            if (!wxIsalnum(innerCell[i])) {
                needsLeftRight = true;
                break;
            }

        if (needsLeftRight)
            s = wxS("\\left[ ") + m_innerCell->ListToTeX() + wxS("\\right] ");
        else
            s = wxS("[") + m_innerCell->ListToTeX() + wxS("]");
    }
    return s;
}

wxString ListCell::ToOMML() const {
    return wxS("<m:d><m:dPr m:begChr=\"") + XMLescape(m_open->ToString()) +
        wxS("\" m:endChr=\"") + XMLescape(m_close->ToString()) +
        wxS("\" m:grow=\"1\"></m:dPr><m:e>") + m_innerCell->ListToOMML() +
        wxS("</m:e></m:d>");
}

wxString ListCell::ToMathML() const {
    wxString open = m_open->ToString();
    wxString close = m_close->ToString();
    return (wxS("<mrow><mo>") + XMLescape(std::move(open)) + wxS("</mo>") +
            m_innerCell->ListToMathML() + wxS("<mo>") + XMLescape(close) +
            wxS("</mo></mrow>\n"));
}

wxString ListCell::ToXML() const {
    wxString s = m_innerCell->ListToXML();
    wxString flags;
    if (HasHardLineBreak())
        flags += wxS(" breakline=\"true\"");
    return (wxS("<r list=\"true\"") + flags +
            wxS("><t listdelim=\"true\">[</t>") + s +
            wxS("<t listdelim=\"true\">]</t></r>"));
}

bool ListCell::BreakUp() {
    if (IsBrokenIntoLines())
        return false;

    Cell::BreakUpAndMark();
    m_open->SetNextToDraw(m_innerCell);
    m_innerCell->last()->SetNextToDraw(m_close);
    m_close->SetNextToDraw(m_nextToDraw);
    m_nextToDraw = m_open;

    ResetCellListSizes();
    m_height = 0;
    m_center = 0;
    return true;
}

void ListCell::SetNextToDraw(Cell *next) {
    if (IsBrokenIntoLines())
        m_close->SetNextToDraw(next);
    else
        m_nextToDraw = next;
}
