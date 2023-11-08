// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//  Copyright (C) 2014-2016 Gunter Königsmann <wxMaxima@physikbuch.de>
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
  This file declares the class ParenCell

  ParenCell is the Cell type that represents a math element that is kept
  between parenthesis.
*/

#ifndef PARENCELL_H
#define PARENCELL_H

#include "Cell.h"

/*! The class that represents parenthesis that are wrapped around text

  In the case that this cell is broken into two lines in the order of
  m_nextToDraw this cell is represented by the following individual
  cells:

  - The ParenCell itself
  - The opening "("
  - The contents
  - The closing ")".

  If it isn't broken into multiple cells m_nextToDraw points to the
  cell that follows this Cell.
*/
class ParenCell final : public Cell
{
public:
    ParenCell(GroupCell *group, Configuration *config, std::unique_ptr<Cell> &&inner);
    ParenCell(GroupCell *group, const ParenCell &cell);
    const CellTypeInfo &GetInfo() override;
    std::unique_ptr<Cell> Copy(GroupCell *group) const override;

    size_t GetInnerCellCount() const override { return 3; }
    // cppcheck-suppress objectIndex
    Cell *GetInnerCell(size_t index) const override { return (&m_open)[index].get(); }

    Cell *GetInner() const { return m_innerCell.get(); }
    void SetInner(std::unique_ptr<Cell> inner, CellType type = MC_TYPE_DEFAULT);

    void SetPrint(bool print) { m_print = print; }

    //! \todo m_open and m_close are recalculated in handdrawn mode, too.
    void Recalculate(AFontSize fontsize) override;

    void Draw(wxPoint point, wxDC *dc, wxDC *antialiassingDC) override;

    bool BreakUp() override;

    wxString ToMathML() const override;
    wxString ToMatlab() const override;
    wxString ToOMML() const override;
    wxString ToString() const override;
    wxString ToTeX() const override;
    wxString ToXML() const override;

    void SetNextToDraw(Cell *next) override;

private:
    // The pointers below point to inner cells and must be kept contiguous.
    // ** This is the draw list order. All pointers must be the same:
    // ** either Cell * or std::unique_ptr<Cell>. NO OTHER TYPES are allowed.
    std::unique_ptr<Cell> m_open;
    std::unique_ptr<Cell> m_innerCell;
    std::unique_ptr<Cell> m_close;
    // The pointers above point to inner cells and must be kept contiguous.

    //! How to create a big parenthesis sign?
    Configuration::drawMode m_bigParenType = Configuration::ascii;
    int m_charWidth1 = 12, m_charHeight1 = 12;
    int m_signWidth = 12, m_signHeight = 50;

    bool m_print CPP20BITFIELD(1) = true;;
};

#endif // PARENCELL_H
