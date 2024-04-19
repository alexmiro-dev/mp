#include "memory_pool/types.hpp"

#include <boost/ut.hpp>
#include <memory_pool/allocator.hpp>

struct Parameter {
    std::string id{};
    float value{0.f};
};

struct NotDefaultConstructible {
    NotDefaultConstructible() = delete;
};

int main() {
    using namespace boost::ut;

    // mp::allocator<NotDefaultConstructible, 1> oops_does_not_compile;

    "Basic creation"_test = [] {
        mp::allocator<Parameter, 7> alloc;
        auto status = alloc.status();

        expect(status.used == 0);

        if (auto init = alloc.initialize(); !init.has_value()) {
            if (init.error().code == mp::error::code_e::cannot_reserve_system_memory) {
            }
        }
    };

    "Allocate - one - success"_test = [] {
        mp::allocator<Parameter, 1> alloc;
        alloc.initialize();

        Parameter* p{nullptr};

        if (auto result = alloc.allocate(); result.has_value()) {
            p = *result;

            p->id = "A";
            p->value = 1.5f;
        }
        expect("A" == p->id);
        expect(1.5_f == p->value);
    };

    "Allocate - fail - no space left"_test = [] {
        mp::allocator<Parameter, 1> alloc;
        alloc.initialize();

        auto result = alloc.allocate();
        expect(result.has_value());

        result = alloc.allocate();
        expect(!result.has_value());
        expect(result.error().code == mp::error::code_e::not_enough_space_in_allocator);
    };

    "Allocate - Bucket - success"_test = [] {
        mp::allocator<Parameter, 5> alloc;
        auto x = alloc.allocate_bucket<3>();
        expect(x.has_value());
        auto bucket = *x;

        auto b0 = *bucket[0];
        b0->id = "A";
        b0->value = 1.f;

        auto b1 = *bucket[1];
        b1->id = "B";
        b1->value = 2.f;

        auto b2 = *bucket[2];
        b2->id = "Cx";
        b2->value = 3.f;

        expect(bucket.size() == 3_u);

        auto oops = bucket[3];
        expect(oops.has_value() == false);
        expect(oops.error().code == mp::error::code_e::out_of_bounds);

        alloc.deallocate(bucket);
    };
}
