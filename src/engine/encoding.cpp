/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2026                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "encoding.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "big5_table.h"
#include "utf8.h"

namespace
{
    constexpr char replacementCharacter{ '?' };

    bool isBig5LeadByte( const uint8_t value )
    {
        return value >= Encoding::detail::big5LeadFirst && value <= Encoding::detail::big5LeadLast;
    }

    // Returns the index of the trail byte within the table row or -1 if the byte is not a valid trail byte.
    int32_t getBig5TrailIndex( const uint8_t value )
    {
        using namespace Encoding::detail;

        if ( value >= big5TrailLowFirst && value <= big5TrailLowLast ) {
            return value - big5TrailLowFirst;
        }

        if ( value >= big5TrailHighFirst && value <= big5TrailHighLast ) {
            return static_cast<int32_t>( big5TrailLowCount ) + ( value - big5TrailHighFirst );
        }

        return -1;
    }
}

namespace Encoding
{
    std::string big5ToUtf8( const std::string_view text )
    {
        std::string output;
        // Every Big5 character (2 bytes) becomes at most 3 bytes in UTF-8.
        output.reserve( text.size() + text.size() / 2 );

        const size_t size = text.size();
        size_t pos = 0;

        while ( pos < size ) {
            const uint8_t lead = static_cast<uint8_t>( text[pos] );

            if ( lead < 0x80 ) {
                output += static_cast<char>( lead );
                ++pos;
                continue;
            }

            if ( !isBig5LeadByte( lead ) || pos + 1 >= size ) {
                // An invalid lead byte or a lead byte at the very end of the text.
                output += replacementCharacter;
                ++pos;
                continue;
            }

            const int32_t trailIndex = getBig5TrailIndex( static_cast<uint8_t>( text[pos + 1] ) );
            if ( trailIndex < 0 ) {
                // The next byte cannot be a trail byte. Consume only the lead byte so that the next byte
                // (for example, an ASCII character) is processed on its own.
                output += replacementCharacter;
                ++pos;
                continue;
            }

            const size_t tableIndex = static_cast<size_t>( lead - detail::big5LeadFirst ) * detail::big5TrailCount + static_cast<size_t>( trailIndex );
            const uint16_t codePoint = detail::big5Table[tableIndex];

            if ( codePoint == 0 ) {
                output += replacementCharacter;
            }
            else {
                fheroes2::appendUtf8Character( output, codePoint );
            }

            pos += 2;
        }

        return output;
    }
}
