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

#include "ui_unicode_font.h"

#include <cassert>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "game_language.h"
#include "h2d.h"
#include "image.h"
#include "logging.h"
#include "ui_font.h"
#include "ui_text.h"

namespace
{
    struct Glyph
    {
        // The position of the glyph bitmap in the glyph set data.
        size_t bitmapOffset{ 0 };

        int32_t advance{ 0 };
        int32_t offsetX{ 0 };
        // The number of glyph pixel rows above the baseline.
        int32_t offsetTop{ 0 };
        int32_t width{ 0 };
        int32_t height{ 0 };
    };

    struct GlyphSet
    {
        std::vector<uint8_t> data;
        std::map<uint32_t, Glyph> glyphs;
    };

    struct LanguageFont
    {
        GlyphSet smallGlyphs;
        GlyphSet normalGlyphs;

        // Generated sprites for every font type.
        std::map<std::pair<fheroes2::FontSize, fheroes2::FontColor>, std::map<uint32_t, fheroes2::Sprite>> sprites;
    };

    // Returns the name of the H2D archive with glyphs for the language or an empty string if the language does not need it.
    std::string getFontArchiveName( const fheroes2::SupportedLanguage language )
    {
        if ( language == fheroes2::SupportedLanguage::TraditionalChinese ) {
            return "zh_hant_font.h2d";
        }

        if ( language == fheroes2::SupportedLanguage::SimplifiedChinese ) {
            return "zh_hans_font.h2d";
        }

        return {};
    }

    int32_t readUint32( const uint8_t * data )
    {
        return static_cast<int32_t>( static_cast<uint32_t>( data[0] ) | ( static_cast<uint32_t>( data[1] ) << 8 ) | ( static_cast<uint32_t>( data[2] ) << 16 )
                                     | ( static_cast<uint32_t>( data[3] ) << 24 ) );
    }

    // Reads glyphs in the format produced by script/tools/generate_cjk_font.py:
    // "FH2F" magic, 16-bit version, 8-bit ascent and descent, 32-bit glyph count, then for every glyph:
    // 32-bit character code, 8-bit advance, signed 8-bit horizontal offset and number of rows above the baseline,
    // 8-bit width and height followed by the bitmap with rows aligned to bytes. All values are little-endian.
    bool loadGlyphSet( std::vector<uint8_t> data, GlyphSet & glyphSet )
    {
        const size_t headerSize = 4 + 2 + 1 + 1 + 4;
        if ( data.size() < headerSize || data[0] != 'F' || data[1] != 'H' || data[2] != '2' || data[3] != 'F' || data[4] != 1 || data[5] != 0 ) {
            return false;
        }

        const uint32_t glyphCount = static_cast<uint32_t>( readUint32( data.data() + 8 ) );

        size_t offset = headerSize;
        for ( uint32_t i = 0; i < glyphCount; ++i ) {
            const size_t glyphHeaderSize = 4 + 5;
            if ( offset + glyphHeaderSize > data.size() ) {
                return false;
            }

            const uint32_t character = static_cast<uint32_t>( readUint32( data.data() + offset ) );

            Glyph glyph;
            glyph.advance = data[offset + 4];
            glyph.offsetX = static_cast<int8_t>( data[offset + 5] );
            glyph.offsetTop = static_cast<int8_t>( data[offset + 6] );
            glyph.width = data[offset + 7];
            glyph.height = data[offset + 8];
            glyph.bitmapOffset = offset + glyphHeaderSize;

            offset = glyph.bitmapOffset + static_cast<size_t>( ( glyph.width + 7 ) / 8 ) * glyph.height;
            if ( offset > data.size() ) {
                return false;
            }

            glyphSet.glyphs.emplace( character, glyph );
        }

        glyphSet.data = std::move( data );

        return true;
    }

    std::map<fheroes2::SupportedLanguage, LanguageFont> languageFonts;

    LanguageFont * getLanguageFont( const fheroes2::SupportedLanguage language )
    {
        auto [iter, isEmplaced] = languageFonts.try_emplace( language );
        if ( isEmplaced ) {
            const std::string archiveName = getFontArchiveName( language );
            if ( !archiveName.empty() ) {
                if ( !loadGlyphSet( fheroes2::h2d::readFile( archiveName, "small.font" ), iter->second.smallGlyphs )
                     || !loadGlyphSet( fheroes2::h2d::readFile( archiveName, "normal.font" ), iter->second.normalGlyphs ) ) {
                    ERROR_LOG( "Failed to load glyphs from '" << archiveName << "'." )
                    iter->second.smallGlyphs = {};
                    iter->second.normalGlyphs = {};
                }
            }
        }

        return &iter->second;
    }

    fheroes2::LetterShape getLetterShape( const GlyphSet & glyphSet, const Glyph & glyph, const fheroes2::FontSize fontSize )
    {
        // Glyphs are placed relative to their baseline which is chosen to fit the full height glyphs into text lines of the game fonts.
        int32_t baseline = 0;
        switch ( fontSize ) {
        case fheroes2::FontSize::SMALL:
            baseline = 7;
            break;
        case fheroes2::FontSize::NORMAL:
        case fheroes2::FontSize::LARGE:
            baseline = 9;
            break;
        case fheroes2::FontSize::BUTTON_RELEASED:
        case fheroes2::FontSize::BUTTON_PRESSED:
            baseline = 10;
            break;
        default:
            // Did you add a new font size? Add the corresponding logic for it!
            assert( 0 );
            break;
        }

        fheroes2::LetterShape shape;
        shape.width = glyph.width;
        shape.height = glyph.height;
        shape.offsetX = glyph.offsetX;
        shape.offsetY = baseline + 1 - glyph.offsetTop;
        shape.advance = glyph.advance;
        shape.pixels.resize( static_cast<size_t>( glyph.width ) * glyph.height, 0 );

        const size_t rowSize = static_cast<size_t>( ( glyph.width + 7 ) / 8 );
        const uint8_t * row = glyphSet.data.data() + glyph.bitmapOffset;
        uint8_t * pixel = shape.pixels.data();

        for ( int32_t y = 0; y < glyph.height; ++y, row += rowSize ) {
            for ( int32_t x = 0; x < glyph.width; ++x, ++pixel ) {
                *pixel = ( row[x / 8] >> ( 7 - x % 8 ) ) & 1;
            }
        }

        return shape;
    }
}

namespace fheroes2
{
    const Sprite * getUnicodeCharacterSprite( const uint32_t character, const FontType & fontType, const SupportedLanguage language )
    {
        LanguageFont * font = getLanguageFont( language );
        assert( font != nullptr );

        // Small and button fonts have similar letter heights.
        const bool useSmallGlyphs = ( fontType.size == FontSize::SMALL || fontType.size == FontSize::BUTTON_RELEASED || fontType.size == FontSize::BUTTON_PRESSED );
        const GlyphSet & glyphSet = useSmallGlyphs ? font->smallGlyphs : font->normalGlyphs;

        const auto glyphIter = glyphSet.glyphs.find( character );
        if ( glyphIter == glyphSet.glyphs.end() ) {
            return nullptr;
        }

        std::map<uint32_t, Sprite> & sprites = font->sprites[{ fontType.size, fontType.color }];

        auto [spriteIter, isEmplaced] = sprites.try_emplace( character );
        if ( isEmplaced ) {
            spriteIter->second = generateLetter( getLetterShape( glyphSet, glyphIter->second, fontType.size ), fontType );
        }

        return &spriteIter->second;
    }

    void resetUnicodeCharacterSprites()
    {
        for ( auto & [language, font] : languageFonts ) {
            font.sprites.clear();
        }
    }
}
