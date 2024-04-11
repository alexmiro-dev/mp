#include "memory_pool/types.hpp"

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

    "Fetch - One - status"_test = [] {
        mp::slot_status_registry<10> slot;

        const auto fetched = slot.fetch();
        expect(fetched.has_value());
        const auto indexes = *fetched;
        expect(fatal(indexes.size() == 1u));
        expect(indexes[0] == 0u);

        const auto status = slot.status();

        expect(status.used == 1u);
        expect(status.free == 9u);
    };

    "Fetch - One consecutively - status"_test = [] {
        mp::slot_status_registry<10> slot;

        auto fetched = slot.fetch();
        expect(fetched.has_value());
        auto indexes = *fetched;
        expect(fatal(indexes.size() == 1u));
        expect(indexes[0] == 0u);

        fetched = slot.fetch();
        expect(fetched.has_value());
        indexes = *fetched;
        expect(fatal(indexes.size() == 1u));
        expect(indexes[0] == 1u);

        fetched = slot.fetch();
        expect(fetched.has_value());
        indexes = *fetched;
        expect(fatal(indexes.size() == 1u));
        expect(indexes[0] == 2u);

        const auto status = slot.status();

        expect(status.used == 3u);
        expect(status.free == 7u);
    };

    "Fetch - Bucket - fail"_test = [] {
        mp::slot_status_registry<10> slot;

        auto fetched = slot.fetch(11u);
        expect(!fetched.has_value());

        expect(fetched.error().code == mp::error::code_e::not_enough_space_in_allocator);

        fetched = slot.fetch(6u);
        expect(fetched.has_value());

        fetched = slot.fetch(5u);
        expect(!fetched.has_value());
    };

    "Fetch Bucket not contiguous"_test = [] {
        mp::slot_status_registry<10> slot;

        auto fetched = slot.fetch(); // i[0]
        std::ignore = slot.fetch();  // i[1]
        std::ignore = slot.fetch();  // i[2]
        std::ignore = slot.fetch();  // i[3]
        std::ignore = slot.fetch();  // i[4]
        std::ignore = slot.fetch();  // i[5]
        std::ignore = slot.fetch();  // i[6]
        std::ignore = slot.fetch();  // i[7]
        std::ignore = slot.fetch();  // i[8]
        std::ignore = slot.fetch();  // i[9]

        auto status = slot.status();
        expect(status.used == 10u);
        expect(status.free == 0u);

        slot.release(2);
        slot.release(3);
        slot.release(5);
        slot.release(7);
        slot.release(9);

        status = slot.status();
        expect(status.used == 5u);
        expect(status.free == 5u);

        fetched = slot.fetch(4);
        expect(fetched.has_value());
        auto indexes = *fetched;

        status = slot.status();
        expect(status.used == 9u);
        expect(status.free == 1u);

        expect(indexes[0] == 2u);
        expect(indexes[1] == 3u);
        expect(indexes[2] == 5u);
        expect(indexes[3] == 7u);
    };

    "Fetch Bucket contiguous"_test = [] {
        mp::slot_status_registry<10> slot;

        auto fetched = slot.fetch();
        fetched = slot.fetch();
        fetched = slot.fetch(5u);
        auto indexes = *fetched;
        expect(fatal(indexes.size() == 5u));

        expect(indexes[0] == 2u);
        expect(indexes[1] == 3u);
        expect(indexes[2] == 4u);
        expect(indexes[3] == 5u);
        expect(indexes[4] == 6u);

        const auto status = slot.status();

        expect(status.used == 7u);
        expect(status.free == 3u);
    };

    "Reset"_test = [] {
        mp::slot_status_registry<10> slot;

        auto fetched = slot.fetch(10);
        auto status = slot.status();
        expect(status.used == 10u);
        expect(status.free == 0u);

        fetched = slot.fetch();
        expect(!fetched.has_value());

        slot.reset();
        status = slot.status();
        expect(status.used == 0u);
        expect(status.free == 10u);

        fetched = slot.fetch();
        expect(fetched.has_value());

        auto indexes = *fetched;
        expect(indexes[0] == 0u);

        status = slot.status();
        expect(status.used == 1u);
        expect(status.free == 9u);
    };
}
