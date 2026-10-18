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

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace fheroes2
{
    // The character used in place of malformed UTF-8 sequences.
    constexpr uint32_t unicodeReplacementCharacter{ 0xFFFD };

    // Decodes one UTF-8 character starting from the given position and returns the number of bytes it occupies.
    // A malformed sequence is decoded as `unicodeReplacementCharacter` occupying 1 byte, so decoding always moves forward.
    // The data range must not be empty.
    int32_t decodeUtf8Character( const uint8_t * data, const uint8_t * dataEnd, uint32_t & character );

    // Returns the number of bytes occupied by the UTF-8 character which ends right before the given position.
    // The result is consistent with `decodeUtf8Character()`: a byte which is not a part of a valid sequence is counted as a separate character.
    int32_t getPreviousUtf8CharacterLength( const uint8_t * dataBegin, const uint8_t * data );

    // Appends the UTF-8 representation of the given character. Invalid code points are appended as `unicodeReplacementCharacter`.
    void appendUtf8Character( std::string & output, const uint32_t character );

    bool isValidUtf8( const std::string_view text );

    // Returns the biggest position not exceeding the given one at which the text can be cut without splitting a UTF-8 character.
    size_t getUtf8CharacterBoundary( const std::string_view text, const size_t position );
}
