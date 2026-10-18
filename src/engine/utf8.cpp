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

#include "utf8.h"

#include <cassert>

namespace
{
    bool isContinuationByte( const uint8_t value )
    {
        return ( value & 0xC0 ) == 0x80;
    }
}

namespace fheroes2
{
    int32_t decodeUtf8Character( const uint8_t * data, const uint8_t * dataEnd, uint32_t & character )
    {
        assert( data != nullptr && data < dataEnd );

        const uint8_t firstByte = *data;
        if ( firstByte < 0x80 ) {
            character = firstByte;
            return 1;
        }

        int32_t length = 0;
        uint32_t minValue = 0;

        if ( ( firstByte & 0xE0 ) == 0xC0 ) {
            length = 2;
            minValue = 0x80;
            character = firstByte & 0x1F;
        }
        else if ( ( firstByte & 0xF0 ) == 0xE0 ) {
            length = 3;
            minValue = 0x800;
            character = firstByte & 0x0F;
        }
        else if ( ( firstByte & 0xF8 ) == 0xF0 ) {
            length = 4;
            minValue = 0x10000;
            character = firstByte & 0x07;
        }
        else {
            character = unicodeReplacementCharacter;
            return 1;
        }

        if ( dataEnd - data < length ) {
            character = unicodeReplacementCharacter;
            return 1;
        }

        for ( int32_t i = 1; i < length; ++i ) {
            if ( !isContinuationByte( data[i] ) ) {
                character = unicodeReplacementCharacter;
                return 1;
            }

            character = ( character << 6 ) | ( data[i] & 0x3F );
        }

        // Reject overlong encodings, UTF-16 surrogates and values beyond the Unicode range.
        if ( character < minValue || ( character >= 0xD800 && character <= 0xDFFF ) || character > 0x10FFFF ) {
            character = unicodeReplacementCharacter;
            return 1;
        }

        return length;
    }

    int32_t getPreviousUtf8CharacterLength( const uint8_t * dataBegin, const uint8_t * data )
    {
        assert( dataBegin != nullptr && dataBegin < data );

        // A UTF-8 character is at most 4 bytes long: look for its first byte among the preceding bytes.
        for ( int32_t length = 1; length <= 4 && data - length >= dataBegin; ++length ) {
            const uint8_t * characterBegin = data - length;
            if ( isContinuationByte( *characterBegin ) ) {
                continue;
            }

            uint32_t character = 0;
            if ( decodeUtf8Character( characterBegin, data, character ) == length ) {
                return length;
            }

            break;
        }

        return 1;
    }

    void appendUtf8Character( std::string & output, uint32_t character )
    {
        if ( ( character >= 0xD800 && character <= 0xDFFF ) || character > 0x10FFFF ) {
            character = unicodeReplacementCharacter;
        }

        if ( character < 0x80 ) {
            output.push_back( static_cast<char>( character ) );
        }
        else if ( character < 0x800 ) {
            output.push_back( static_cast<char>( 0xC0 | ( character >> 6 ) ) );
            output.push_back( static_cast<char>( 0x80 | ( character & 0x3F ) ) );
        }
        else if ( character < 0x10000 ) {
            output.push_back( static_cast<char>( 0xE0 | ( character >> 12 ) ) );
            output.push_back( static_cast<char>( 0x80 | ( ( character >> 6 ) & 0x3F ) ) );
            output.push_back( static_cast<char>( 0x80 | ( character & 0x3F ) ) );
        }
        else {
            output.push_back( static_cast<char>( 0xF0 | ( character >> 18 ) ) );
            output.push_back( static_cast<char>( 0x80 | ( ( character >> 12 ) & 0x3F ) ) );
            output.push_back( static_cast<char>( 0x80 | ( ( character >> 6 ) & 0x3F ) ) );
            output.push_back( static_cast<char>( 0x80 | ( character & 0x3F ) ) );
        }
    }

    bool isValidUtf8( const std::string_view text )
    {
        const uint8_t * data = reinterpret_cast<const uint8_t *>( text.data() );
        const uint8_t * dataEnd = data + text.size();

        while ( data != dataEnd ) {
            uint32_t character = 0;
            const int32_t length = decodeUtf8Character( data, dataEnd, character );
            if ( character == unicodeReplacementCharacter && length == 1 && *data >= 0x80 ) {
                return false;
            }

            data += length;
        }

        return true;
    }
}
