#pragma once
#include <stdint.h>

// Duplicate check (ignores -1) using counts
template<int ID>
struct _led_count {
    static constexpr int value =
        ((LED_CHANNEL_MAP[0]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[1]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[2]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[3]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[4]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[5]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[6]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[7]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[8]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[9]  == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[10] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[11] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[12] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[13] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[14] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[15] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[16] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[17] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[18] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[19] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[20] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[21] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[22] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[23] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[24] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[25] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[26] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[27] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[28] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[29] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[30] == ID) ? 1:0) +
        ((LED_CHANNEL_MAP[31] == ID) ? 1:0);
};

// Uniqueness asserts
static_assert(_led_count<0>::value  <= 1, "Duplicate logical LED id 0");
static_assert(_led_count<1>::value  <= 1, "Duplicate logical LED id 1");
static_assert(_led_count<2>::value  <= 1, "Duplicate logical LED id 2");
static_assert(_led_count<3>::value  <= 1, "Duplicate logical LED id 3");
static_assert(_led_count<4>::value  <= 1, "Duplicate logical LED id 4");
static_assert(_led_count<5>::value  <= 1, "Duplicate logical LED id 5");
static_assert(_led_count<6>::value  <= 1, "Duplicate logical LED id 6");
static_assert(_led_count<7>::value  <= 1, "Duplicate logical LED id 7");
static_assert(_led_count<8>::value  <= 1, "Duplicate logical LED id 8");
static_assert(_led_count<9>::value  <= 1, "Duplicate logical LED id 9");
static_assert(_led_count<10>::value <= 1, "Duplicate logical LED id 10");
static_assert(_led_count<11>::value <= 1, "Duplicate logical LED id 11");
static_assert(_led_count<12>::value <= 1, "Duplicate logical LED id 12");
static_assert(_led_count<13>::value <= 1, "Duplicate logical LED id 13");
static_assert(_led_count<14>::value <= 1, "Duplicate logical LED id 14");
static_assert(_led_count<15>::value <= 1, "Duplicate logical LED id 15");
static_assert(_led_count<16>::value <= 1, "Duplicate logical LED id 16");
static_assert(_led_count<17>::value <= 1, "Duplicate logical LED id 17");
static_assert(_led_count<18>::value <= 1, "Duplicate logical LED id 18");
static_assert(_led_count<19>::value <= 1, "Duplicate logical LED id 19");
static_assert(_led_count<20>::value <= 1, "Duplicate logical LED id 20");
static_assert(_led_count<21>::value <= 1, "Duplicate logical LED id 21");
static_assert(_led_count<22>::value <= 1, "Duplicate logical LED id 22");
static_assert(_led_count<23>::value <= 1, "Duplicate logical LED id 23");
static_assert(_led_count<24>::value <= 1, "Duplicate logical LED id 24");
static_assert(_led_count<25>::value <= 1, "Duplicate logical LED id 25");
static_assert(_led_count<26>::value <= 1, "Duplicate logical LED id 26");
static_assert(_led_count<27>::value <= 1, "Duplicate logical LED id 27");
static_assert(_led_count<28>::value <= 1, "Duplicate logical LED id 28");
static_assert(_led_count<29>::value <= 1, "Duplicate logical LED id 29");
static_assert(_led_count<30>::value <= 1, "Duplicate logical LED id 30");
static_assert(_led_count<31>::value <= 1, "Duplicate logical LED id 31");

// Compile-time generated inverse (logical -> physical) using template recursion

template<int Logical, int Phys>
struct _phys_finder {
    static constexpr int16_t value = (Phys >= LED_CHANNEL_COUNT) ? -1 :
        (LED_CHANNEL_MAP[Phys] == Logical ? Phys : _phys_finder<Logical, Phys+1>::value);
};

template<int Logical>
struct _phys_finder<Logical, LED_CHANNEL_COUNT> { static constexpr int16_t value = -1; };

template<int Logical>
struct _phys_for_logical { static constexpr int16_t value = _phys_finder<Logical,0>::value; };

template<int... I> struct _idx_seq { };

template<int N, int... I>
struct _make_idx_seq : _make_idx_seq<N-1, N-1, I...> { };

template<int... I>
struct _make_idx_seq<0, I...> { typedef _idx_seq<I...> type; };

template<typename Seq> struct _inv_builder;

template<int... I>
struct _inv_builder<_idx_seq<I...>> { static constexpr int16_t data[sizeof...(I)] = { _phys_for_logical<I>::value... }; };

template<int... I>
constexpr int16_t _inv_builder<_idx_seq<I...>>::data[sizeof...(I)];

using _inv_seq = typename _make_idx_seq<LED_CHANNEL_COUNT>::type;
using _inv_data = _inv_builder<_inv_seq>;
static constexpr int16_t const (&LED_CHANNEL_INV)[LED_CHANNEL_COUNT] = _inv_data::data;
