#include <doctest/doctest.h>

#include "client/IO/Components/Icon.h"

namespace
{
    class ProbeIconType : public jrc::Icon::Type
    {
    public:
        explicit ProbeIconType(jrc::Keyboard::Mapping mapping) : mapping(mapping) {}

        void drop_on_stage() const override {}
        void drop_on_equips(jrc::Equipslot::Id) const override {}
        void drop_on_items(jrc::InventoryType::Id, jrc::Equipslot::Id, int16_t, bool) const override {}
        jrc::Keyboard::Mapping get_key_mapping() const override { return mapping; }

    private:
        jrc::Keyboard::Mapping mapping;
    };
}

TEST_CASE("Icons expose only mappings supported by their drag type")
{
    jrc::Icon unbound;
    CHECK(unbound.get_key_mapping() == jrc::Keyboard::Mapping{});

    const jrc::Keyboard::Mapping skill{ jrc::KeyType::SKILL, 1001000 };
    jrc::Icon bound(std::make_unique<ProbeIconType>(skill), {}, -1);
    CHECK(bound.get_key_mapping() == skill);
}
