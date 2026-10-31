#include <doctest/doctest.h>
#include "client/IO/Components/Textfield.h"
#include "client/IO/UI.h"
#include "client/IO/Components/ScrollingNotice.h"
#include "client/Graphics/Animation.h"
#include "client/Graphics/Texture.h"

namespace jrc
{
    Text::Layout::Layout() {}
    Text::Text(Font f, Alignment a, Color c, Background b, const std::string& t, uint16_t mw, bool fm)
        : font(f), alignment(a), color(c), background(b), maxwidth(mw), formatted(fm), text(t) {}
    Text::Text(Font f, Alignment a, Color c, const std::string& t, uint16_t mw, bool fm)
        : Text(f, a, c, NONE, t, mw, fm) {}
    Text::Text() : Text(A11M, LEFT, BLACK) {}
    void Text::change_text(const std::string& value) { text = value; }
    uint16_t Text::advance(size_t pos) const { return static_cast<uint16_t>(pos * 8); }
    void Text::draw(const DrawArgument&) const {}

    UI::UI() {}
    void UI::quit() {}
    void UI::focus_textfield(Textfield*) {}
    void UI::blur_textfield(Textfield*) {}

    Keyboard::Keyboard() {}
    Cursor::Cursor() {}
    ScrollingNotice::ScrollingNotice() {}
    Animation::Animation() {}
    Frame::Frame() {}
    ColorBox::ColorBox() {}
    Texture::~Texture() {}
}

TEST_CASE("Textfield inserts mapped characters on key press and repeat")
{
    using namespace jrc;
    Textfield field(Text::A11M, Text::LEFT, Text::LIGHTGREY, {{0, 0}, {100, 20}}, 3);

    field.send_key(KeyType::LETTER, 'a', true);
    CHECK(field.get_text() == "a");
    field.send_key(KeyType::LETTER, 'a', false);
    CHECK(field.get_text() == "a");
    field.send_key(KeyType::LETTER, 'A', true);
    CHECK(field.get_text() == "aA");
    field.send_key(KeyType::NUMBER, '2', true);
    CHECK(field.get_text() == "aA2");
    field.send_key(KeyType::LETTER, 'A', true);
    CHECK(field.get_text() == "aA2");
    field.send_key(KeyType::ACTION, KeyAction::BACK, true);
    CHECK(field.get_text() == "aA");
    field.send_key(KeyType::ACTION, KeyAction::BACK, false);
    CHECK(field.get_text() == "aA");
}
