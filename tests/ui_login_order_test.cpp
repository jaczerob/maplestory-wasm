#include <doctest/doctest.h>
#include "client/IO/UIStateLogin.h"
#include "client/IO/UI.h"
#include "client/IO/UITypes/UILogin.h"
#include "client/IO/Components/ScrollingNotice.h"
#include "client/Graphics/Animation.h"
#include "client/Graphics/Texture.h"

#include <string>
#include <vector>

namespace
{
    std::vector<std::string> drawn;

    class Probe : public jrc::UIElement
    {
    public:
        Probe(jrc::UIElement::Type type, std::string name) : name(std::move(name))
        {
            set_type(type);
        }

        void draw(float) const override { drawn.push_back(name); }

    private:
        std::string name;
    };

    Probe* add(jrc::UIStateLogin& state, jrc::UIElement::Type type,
        const std::string& name, bool focused = false)
    {
        auto iter = state.pre_add(type, false, focused);
        auto element = std::make_unique<Probe>(type, name);
        Probe* result = element.get();
        iter.second() = std::move(element);
        return result;
    }
}

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
    Texture::Texture() {}
    Texture::~Texture() {}

    UIElement::UIElement() : position(), dimension(), active(true), type(NONE), handled_button_press_id(0) {}
    void UIElement::draw(float) const {}
    void UIElement::draw_checked(float alpha) const { draw(alpha); }
    void UIElement::update() {}
    void UIElement::update_screen(int16_t, int16_t) {}
    void UIElement::makeactive() { active = true; }
    void UIElement::deactivate() { active = false; }
    bool UIElement::is_active() const { return active; }
    void UIElement::toggle_active() { active = !active; }
    Button::State UIElement::button_pressed(uint16_t) { return Button::NORMAL; }
    void UIElement::send_icon(const Icon&, Point<int16_t>) {}
    void UIElement::doubleclick(Point<int16_t>) {}
    void UIElement::rightclick(Point<int16_t>) {}
    bool UIElement::is_in_range(Point<int16_t>) const { return false; }
    bool UIElement::remove_cursor(bool, Point<int16_t>) { return false; }
    UIElement::CursorResult UIElement::send_cursor(bool, Point<int16_t>) { return {}; }
    void UIElement::send_scroll(double) {}
    void UIElement::send_key(int32_t, bool, bool) {}
    UIElement::Type UIElement::get_type() const { return type; }
    void UIElement::set_type(Type value) { type = value; }

    UILogin::UILogin() : UIElement() {}
    void UILogin::draw(float) const {}
    void UILogin::update() {}
    UIElement::CursorResult UILogin::send_cursor(bool, Point<int16_t>) { return {}; }
    Button::State UILogin::button_pressed(uint16_t) { return Button::NORMAL; }
}

TEST_CASE("Focused login notice draws above character creation")
{
    using namespace jrc;
    UIStateLogin state;
    Probe* creation = add(state, UIElement::CHARCREATION, "creation");
    Probe* notice = add(state, UIElement::LOGINNOTICE, "notice", true);

    state.draw(1.0f, {});
    CHECK((drawn == std::vector<std::string>{"creation", "notice"}));

    drawn.clear();
    notice->deactivate();
    state.draw(1.0f, {});
    CHECK((drawn == std::vector<std::string>{"creation"}));
    CHECK(creation->is_active());
}
