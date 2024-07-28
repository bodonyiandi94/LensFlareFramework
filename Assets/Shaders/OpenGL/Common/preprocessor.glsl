// String expansion
#define EXPAND(X) X

// String concatenation
#define _CONCAT_IMPL2(S1, S2) S1 ## S2
#define _CONCAT_IMPL1(S1, S2) _CONCAT_IMPL2(S1, S2)
#define CONCAT(S1, S2)        _CONCAT_IMPL1(S1, S2)

// For loop
#define FOR_LOOP1(a, P1, P2, P3)  a(0,  P1, P2, P3)
#define FOR_LOOP2(a, P1, P2, P3)  a(1,  P1, P2, P3) FOR_LOOP1(a,  P1, P2, P3)
#define FOR_LOOP3(a, P1, P2, P3)  a(2,  P1, P2, P3) FOR_LOOP2(a,  P1, P2, P3)
#define FOR_LOOP4(a, P1, P2, P3)  a(3,  P1, P2, P3) FOR_LOOP3(a,  P1, P2, P3)
#define FOR_LOOP5(a, P1, P2, P3)  a(4,  P1, P2, P3) FOR_LOOP4(a,  P1, P2, P3)
#define FOR_LOOP6(a, P1, P2, P3)  a(5,  P1, P2, P3) FOR_LOOP5(a,  P1, P2, P3)
#define FOR_LOOP7(a, P1, P2, P3)  a(6,  P1, P2, P3) FOR_LOOP6(a,  P1, P2, P3)
#define FOR_LOOP8(a, P1, P2, P3)  a(7,  P1, P2, P3) FOR_LOOP7(a,  P1, P2, P3)
#define FOR_LOOP9(a, P1, P2, P3)  a(8,  P1, P2, P3) FOR_LOOP8(a,  P1, P2, P3)
#define FOR_LOOP10(a, P1, P2, P3) a(9,  P1, P2, P3) FOR_LOOP9(a,  P1, P2, P3)
#define FOR_LOOP11(a, P1, P2, P3) a(10, P1, P2, P3) FOR_LOOP10(a, P1, P2, P3)
#define FOR_LOOP12(a, P1, P2, P3) a(11, P1, P2, P3) FOR_LOOP11(a, P1, P2, P3)
#define FOR_LOOP13(a, P1, P2, P3) a(13, P1, P2, P3) FOR_LOOP12(a, P1, P2, P3)
#define FOR_LOOP14(a, P1, P2, P3) a(13, P1, P2, P3) FOR_LOOP13(a, P1, P2, P3)
#define FOR_LOOP15(a, P1, P2, P3) a(14, P1, P2, P3) FOR_LOOP14(a, P1, P2, P3)
#define FOR_LOOP16(a, P1, P2, P3) a(15, P1, P2, P3) FOR_LOOP15(a, P1, P2, P3)

#define FOR_LOOP_IMPL2(n, a, P1, P2, P3) FOR_LOOP##n(a, P1, P2, P3)
#define FOR_LOOP_IMPL1(n, a, P1, P2, P3) FOR_LOOP_IMPL2(n, a, P1, P2, P3)
#define FOR_LOOP(n, a, P1, P2, P3) FOR_LOOP_IMPL1(EXPAND(n), a, P1, P2, P3)

// Integer division, with the result rounded upwards
#define ROUNDED_DIV(NUMBER, BASE) (((NUMBER) + (BASE) - 1) / (BASE))

// Integer rounding to next multiple
#define NEXT_MULTIPLE(NUMBER, BASE) ((ROUNDED_DIV(NUMBER, BASE)) * (BASE))