#include <boost/ut.hpp>

#include <memory_pool/Allocator.hpp>

class Person {
    public:
    std::string firstName;
    uint16_t age{0U};

    Person() { std::cout << "Person Ctor\n";}
    ~Person() { std::cout << "Person Dctor name[" << firstName << "] age[" << age << "]\n";}
};

int main() {
    using namespace boost::ut;

    mp::Allocator<Person, 7> alloc;

    auto status = alloc.status();

    
    expect(status.used == 0);

    if (auto init = alloc.initialize(); !init.has_value()){
        if (init.error().code == mp::error::Code::UnableToAllocateMemory) {

        }
    } 

    Person* p{nullptr};
    if (auto result = alloc.allocate(); result.has_value()){
        p = *result;

        p->firstName = "miro";
        p->age = 48;
    }

    if (p) {
        std::cout << p->firstName << std::endl;
        std::cout << p->age << std::endl;
    }

    Person* p2{nullptr};
    if (auto result = alloc.allocate(); result.has_value()){
        p2 = *result;

        p2->firstName = "jana";
        p2->age = 49;
    }

    // status = alloc.status();
    // expect(status.used == 2);
    // std::ignore = alloc.deallocate(p);
    //
    // status = alloc.status();
    // expect(status.used == 1);
    // std::ignore = alloc.deallocate(p2);
    //
    // status = alloc.status();
    // expect(status.used == 0);
    // expect(true);

    auto x = alloc.allocate<3>();
    expect(x.has_value());
    auto bucket = *x;

    auto b0 = *bucket[0];
    b0->firstName = "A";
    b0->age = 1;

    auto b1 = *bucket[1];
    b1->firstName = "B";
    b1->age = 2;

    auto b2 = *bucket[2];
    b2->firstName = "C";
    b2->age = 3;

    expect(bucket.size() == 3);

    for (auto i : bucket) {
        std::cout << i->firstName << " " << i->age << std::endl;
    }

    auto oops = bucket[3];
    expect(oops.has_value() == false);
    expect(oops.error().code == mp::error::Code::BucketIndexOutOfBounds);
}

