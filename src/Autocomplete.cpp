///
///  Copyright (C) 2009 Andrej Vodopivec <andrejv@users.sourceforge.net>
///
///  This program is free software; you can redistribute it and/or modify
///  it under the terms of the GNU General Public License as published by
///  the Free Software Foundation; either version 2 of the License, or
///  (at your option) any later version.
///
///  This program is distributed in the hope that it will be useful,
///  but WITHOUT ANY WARRANTY; without even the implied warranty of
///  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///  GNU General Public License for more details.
///
///
///  You should have received a copy of the GNU General Public License
///  along with this program; if not, write to the Free Software
///  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
///

#include "Autocomplete.h"

#include <wx/textfile.h>

/// This functions loads functions/variables from maxima documentation.
/// It reads the file index.hhk and looks for lines like
/// |   <param name="Name" value="function"></object>|
bool AutoComplete::LoadFunctions(wxString file)
{
  wxString line;
  wxString rest, function;
  wxTextFile index(file);

  index.Open();

  for(line = index.GetFirstLine(); !index.Eof(); line = index.GetNextLine())
  {
    if (!line.StartsWith(wxT("   <param name=\"Name\" value=\""), &rest))
      continue;

    if (!rest.EndsWith(wxT("\"></object>"), &function))
      continue;

    m_functionList.Add(function);
  }

  m_functionList.Sort();

  index.Close();

  return false;
}

/// Returns a string array with functions which start with partial.
wxArrayString AutoComplete::CompleteFunction(wxString partial)
{
  wxArrayString completions;

  for (int i=0; i<m_functionList.GetCount(); i++)
  {
    if (m_functionList[i].StartsWith(partial))
      completions.Add(m_functionList[i]);
  }

  return completions;
}
