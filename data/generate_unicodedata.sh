#!/bin/sh

# convert wxMathml.txt to C Sourcecode (using xxd -i)

    cat >UnicodeData.h <<END
/* Automatically generated file using generate_wxmathml.sh                              */
/* This file is part of wxMaxima.                                                       */

/* Copyright (C) 2018 wxMaxima Team (https://wxMaxima-developers.github.io/wxmaxima/)               */

/* This program is free software; you can redistribute it and/or modify                 */
/* it under the terms of the GNU General Public License as published by                 */
/* the Free Software Foundation; either version 2 of the License, or                    */
/* (at your option) any later version.                                                  */

/* This program is distributed in the hope that it will be useful,                      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of                       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                        */
/* GNU General Public License for more details.                                         */

/* You should have received a copy of the GNU General Public License                    */
/* along with this program; if not, write to the Free Software                          */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA            */
END

echo "Converting UnicodeData.txt to embeddable C code"
cat UnicodeData.txt |cut -d ";" -f 1-2 |gzip -c > UnicodeData.txt.gz
xxd -i "UnicodeData.txt.gz" >> "UnicodeData.h"
rm -f UnicodeData.txt.gz
