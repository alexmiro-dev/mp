#include <boost/ut.hpp>
#include <memory_pool/slot_status_registry.hpp>

int main() {
    using namespace boost::ut;

    "Creation - One slot - status"_test = [] {
        mp::slot_status_registry<1> slot;

        const auto status = slot.status();

        expect(status.used == 0u);
        expect(status.free == 1u);
    };

    "Creation - N slots - status"_test = [] {
        mp::slot_status_registry<10> slot;

        const auto status = slot.status();

        expect(status.used == 0u);
        expect(status.free == 10u);
    };

    "Fetch - One slot - status"_test = [] {
        mp::slot_status_registry<1> slot;

        const auto fetched = slot.fetch();
        expect(fetched.has_value());
        const auto indexes = *fetched;
        expect(fatal(indexes.size() == 1u));
        expect(indexes[0] == 0u);

        const auto status = slot.status();

        expect(status.used == 1u);
        expect(status.free == 0u);
    };
}
