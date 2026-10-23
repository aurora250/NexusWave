#ifndef NEXUSWAVE_UI_GLYPHS_HPP__
#define NEXUSWAVE_UI_GLYPHS_HPP__
#include <NeForce/core/string/string_view.hpp>
namespace newave::glyphs {
    inline constexpr neforce::string_view pointer{"\xe2\x9d\xaf"};          // ❯
    inline constexpr neforce::string_view assistant{"\xe2\x97\x8f"};        // ●
    inline constexpr neforce::string_view settled{"\xe2\x80\xa2"};          // •
    inline constexpr neforce::string_view failed{"\xe2\x9c\x97"};           // ✗
    inline constexpr neforce::string_view success{"\xe2\x9c\x93"};          // ✓
    inline constexpr neforce::string_view thinking_settled{"\xe2\x9a\x93"}; // ⚓
    inline constexpr neforce::string_view rail{"\xe2\x96\x8e"};             // ▎
    inline constexpr neforce::string_view fold{"\xe2\x96\xb8"};             // ▸
    inline constexpr neforce::string_view unfold{"\xe2\x96\xbe"};           // ▾
    inline constexpr neforce::string_view warning{"\xe2\x9a\xa0"};          // ⚠
    inline constexpr neforce::string_view session{"\xe2\x8c\xb8"};          // ⌸
    inline constexpr neforce::string_view fullscreen{"\xe2\x9b\xb6"};       // ⛶
} // namespace newave::glyphs
#endif
