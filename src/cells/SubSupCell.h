// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2007-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2014-2016 Gunter Königsmann <wxMaxima@physikbuch.de>
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

#ifndef SUBSUPCELL_H
#define SUBSUPCELL_H

#include "precomp.h"
#include <memory>
#include "Cell.h"

class SubSupCell final : public Cell
{
public:
    SubSupCell(GroupCell *group, Configuration *config, std::unique_ptr<Cell> &&base);
    SubSupCell(GroupCell *group, const SubSupCell &cell);
    const CellTypeInfo &GetInfo() override;
    std::unique_ptr<Cell> Copy(GroupCell *group) const override;

    size_t GetInnerCellCount() const override { return 5; }
    // cppcheck-suppress objectIndex
    Cell *GetInnerCell(size_t index) const override { return (&m_baseCell)[index].get(); }

    void SetIndex(std::unique_ptr<Cell> &&index);
    void SetExponent(std::unique_ptr<Cell> &&expt);
    void SetPreSub(std::unique_ptr<Cell> &&index);
    void SetPreSup(std::unique_ptr<Cell> &&index);
    void SetPostSub(std::unique_ptr<Cell> &&index);
    void SetPostSup(std::unique_ptr<Cell> &&index);

    void Recalculate(AFontSize fontsize) override;

    void Draw(wxPoint point, wxDC *dc, wxDC *antialiassingDC) override;

    wxString ToMathML() const override;
    wxString ToMatlab() const override;
    wxString ToOMML() const override;
    wxString ToString() const override;
    wxString ToTeX() const override;
    wxString ToXML() const override;

    wxString GetDiffPart() const override;

    void SetAltCopyText(const wxString &text) override { m_altCopyText = text; }
    const wxString &GetAltCopyText() const override { return m_altCopyText; }

private:
    //! Text that should end up on the clipboard if this cell is copied as text.
    wxString m_altCopyText;

    //! The inner cells set via SetPre* or SetPost*, but not SetBase nor SetIndex
    //! nor SetExponent.
    std::vector<Cell *> m_scriptCells;

    // The pointers below point to inner cells and must be kept contiguous.
    // ** All pointers must be the same: either Cell * or std::unique_ptr<Cell>.
    // ** NO OTHER TYPES are allowed.
    std::unique_ptr<Cell> m_baseCell;
    std::unique_ptr<Cell> m_postSubCell;
    std::unique_ptr<Cell> m_postSupCell;
    std::unique_ptr<Cell> m_preSubCell;
    std::unique_ptr<Cell> m_preSupCell;
    // The pointers above point to inner cells and must be kept contiguous.
};

#endif // SUBSUPCELL_H
