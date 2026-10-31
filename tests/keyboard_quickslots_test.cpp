#include <doctest/doctest.h>

#include "client/IO/Keyboard.h"

#include <GLFW/glfw3.h>

TEST_CASE("Quickslot layout defaults, changes, and reflects current mappings")
{
    jrc::Keyboard keyboard;

    CHECK(keyboard.get_quickslots() == jrc::Keyboard::DEFAULT_QUICKSLOTS);
    CHECK_FALSE(keyboard.set_quickslots(jrc::Keyboard::DEFAULT_QUICKSLOTS));

    jrc::Keyboard::Quickslots custom{ 30, 82, 71, 73, 29, 83, 79, 81 };
    CHECK(keyboard.set_quickslots(custom));
    CHECK(keyboard.get_quickslots() == custom);
    CHECK_FALSE(keyboard.set_quickslots(custom));

    const auto key = keyboard.get_quickslots().front();
    CHECK(keyboard.get_maple_mapping(key) == jrc::Keyboard::Mapping{});

    keyboard.assign(key, static_cast<uint8_t>(jrc::KeyType::SKILL), 1001000);
    CHECK(keyboard.get_maple_mapping(key) == jrc::Keyboard::Mapping(jrc::KeyType::SKILL, 1001000));

    custom[0] = 255;
    CHECK(keyboard.set_quickslots(custom));
    CHECK(keyboard.get_quickslots().front() == 255);
    CHECK(keyboard.get_maple_mapping(255) == jrc::Keyboard::Mapping{});

    keyboard.assign(255, static_cast<uint8_t>(jrc::KeyType::SKILL), 2001000);
    keyboard.remove(255);
    CHECK(keyboard.get_maple_mapping(key) == jrc::Keyboard::Mapping(jrc::KeyType::SKILL, 1001000));
}

TEST_CASE("GLFW key lookup excludes unsupported and empty table entries")
{
    const jrc::Keyboard keyboard;

    CHECK(keyboard.get_maple_key(GLFW_KEY_A) == 30);
    CHECK_FALSE(keyboard.get_maple_key(GLFW_KEY_LEFT).has_value());
    CHECK_FALSE(keyboard.get_maple_key(0).has_value());
    CHECK(jrc::Keyboard::is_bindable_key(30));
    CHECK_FALSE(jrc::Keyboard::is_bindable_key(0));
    CHECK_FALSE(jrc::Keyboard::is_bindable_key(14));
    CHECK_FALSE(jrc::Keyboard::is_bindable_key(255));
}
