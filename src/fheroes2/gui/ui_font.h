/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2022 - 2026                                             *
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

#include <cstdint>
#include <string>
#include <vector>

namespace fheroes2
{
    class Image;
    class Sprite;

    struct FontType;

    enum class SupportedLanguage : uint8_t;

    // A monochrome shape of a character which is not a part of the game fonts.
    struct LetterShape
    {
        // Pixels of the shape row by row: 1 for a pixel of the character and 0 for a transparent pixel.
        std::vector<uint8_t> pixels;

        int32_t width{ 0 };
        int32_t height{ 0 };

        // The position of the shape relative to the left side of the character and to the top of a text line.
        int32_t offsetX{ 0 };
        int32_t offsetY{ 0 };

        // The horizontal distance from the beginning of this character to the beginning of the next one.
        int32_t advance{ 0 };
    };

    // Generates a letter in the style of the given font type from the shape of a character.
    // Large font letters are upscaled normal font letters, therefore the shape for them must be the same as for the normal font.
    Sprite generateLetter( const LetterShape & shape, const FontType & fontType );

    // Generates a letter with a vertical gradient and contours from a letter of a white font.
    Sprite generateGradientLetter( const Sprite & letter, const uint8_t gradientInnerColor, const uint8_t gradientOuterColor, const uint8_t contourInnerColor,
                                   const uint8_t contourOuterColor );

    void generateAlphabet( const SupportedLanguage language, std::vector<std::vector<Sprite>> & icnVsSprite );

    void generateButtonAlphabet( const SupportedLanguage language, std::vector<std::vector<Sprite>> & icnVsSprite );

    void modifyBaseNormalFont( std::vector<fheroes2::Sprite> & icnVsSprite );

    void modifyBaseSmallFont( std::vector<fheroes2::Sprite> & icnVsSprite );

    void applyFontVerticalGradient( Image & image, const uint8_t insideColor, const uint8_t outsideColor );

    // The original French version replaces several ASCII special characters with language-specific characters.
    // In the engine we use CP1252 for the French translation.
    // This function replaces the special ASCII characters with the language-specific characters from CP1252
    // to properly display texts for maps from French assets.
    void fixFrenchCharactersForMP2Map( std::string & str );

    // Maps made for Traditional Chinese with the original editor store texts in Big5 encoding.
    // This function converts such texts to UTF-8 used by the engine. A text which is already valid UTF-8 or is not valid Big5 is not changed.
    void convertBig5MP2MapText( std::string & str );
}
