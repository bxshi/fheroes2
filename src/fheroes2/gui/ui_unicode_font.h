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

#include <cstdint>

namespace fheroes2
{
    class Sprite;

    struct FontType;

    enum class SupportedLanguage : uint8_t;

    // Returns the sprite of a character which is not a part of the game fonts. Such characters are used by languages with UTF-8 encoding.
    // Returns nullptr if there is no glyph for this character for the given language.
    const Sprite * getUnicodeCharacterSprite( const uint32_t character, const FontType & fontType, const SupportedLanguage language );

    // Removes all generated sprites, for example, when the settings affecting their look are changed.
    void resetUnicodeCharacterSprites();
}
