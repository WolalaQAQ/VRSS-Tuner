#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "driver_identity.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace driver;
    assert(Select(k59602.apiHash, "UNAVAILABLE", k59602.umdHash)==&k59602);
    assert(Select("unused", k61088.apiHash, k61088.umdHash)==&k61088);
    assert(!Select(k59602.apiHash, k61088.apiHash, "changed"));
    assert(!Select(k59602.apiHash, "UNAVAILABLE", k61088.umdHash));
    assert(!Select("UNAVAILABLE", "UNAVAILABLE", k59602.umdHash));
    for(const auto* profile : {&k59602,&k61088}) {
        for(uintptr_t base : {uintptr_t{0x180000000},uintptr_t{0x7ffb12340000}}) {
            auto methods=profile->methodRvas;
            for(auto& method:methods)method+=base;
            assert(MethodsMatch(*profile,base,methods));
            for(size_t i=0;i<methods.size();++i) {
                auto wrong=methods; ++wrong[i];
                assert(!MethodsMatch(*profile,base,wrong));
            }
            const auto table=base+(profile->vtableRva?profile->vtableRva:0x400000);
            assert(TableAddressMatches(*profile,base,0x1000000,table));
            assert(!TableAddressMatches(*profile,base,0x1000000,base-1));
            assert(!TableAddressMatches(*profile,base,0x1000000,base+0x1000000-1));
        }
    }
    assert(!TableAddressMatches(k59602,0x180000000,0x1000000,0x180480281));
    assert(!TableAddressMatches(k59602,0,0x1000000,0x480280));
    assert(!AddressMatches((std::numeric_limits<uintptr_t>::max)(),1,0));
    std::cout<<"DRIVER_IDENTITY_PASS 59602 61088 missing_images mixed_versions every_slot table_bounds overflow\n";
}
