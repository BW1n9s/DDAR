#include "../src/SlotPolicy.h"
#include <iostream>
#include <stdexcept>

int main()
{
    constexpr auto belt = 1u << 19;
    constexpr auto extra52 = 1u << 22;
    constexpr auto body = 1u << 2;
    auto check = [](bool condition, const char* name) { if (!condition) throw std::runtime_error(name); };
    check(DDAR::BeltSlotsAllowed(belt, belt, belt, belt, false), "matching belt rejected");
    check(!DDAR::BeltSlotsAllowed(belt, belt, belt, belt | extra52, false), "strict mode accepted local extra 52");
    check(DDAR::BeltSlotsAllowed(belt, belt, belt, belt | extra52, true), "explicit extra 52 experiment rejected");
    check(!DDAR::BeltSlotsAllowed(belt, belt | extra52, belt, belt, true), "exception leaked to source addon");
    check(!DDAR::BeltSlotsAllowed(belt, belt, belt | extra52, belt, true), "exception changed armor slot policy");
    check(!DDAR::BeltSlotsAllowed(belt, belt, belt, belt | extra52 | body, true), "unknown target partition accepted");
    check(!DDAR::BeltSlotsAllowed(0, 0, 0, 0, true), "empty masks accepted");
    check(!DDAR::BeltSlotsAllowed(body, body, body, body, false), "non-belt scope accepted");
    std::cout << "8 slot-policy cases passed\n";
}
