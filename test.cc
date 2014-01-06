/*
    Copyright (C) 2014  Povilas Kanapickas <povilas@radix.lt>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <iostream>
#include <cstdio>
#include <sstream>

#include "cformat.h"

#define TEST_DOUBLE 1
unsigned long long g_test_count = 20000000;

/* The values for testing are computed effectively as follows:
    for (loop = 0; need_more? ; ++loop) {
        for (T v = g_min * g_mul_loop*loop; v < g_max; v *= g_mul) {
            test(v);
        }
        for (T v = -g_min * g_mul_loop*loop; v < -g_max; v *= g_mul) {
            test(v);
        }
    }
*/

#if TEST_DOUBLE
typedef double T;
T g_min = 1e-30;
T g_max = 1e30;
#else
typedef float T;
T g_min = 1e-30;
T g_max = 1e30;
#endif

// The following values are arbitrary
T g_start = 1.123123123123123123123;
T g_mul = 1.5000203040203040203020304;
T g_mul_loop = 1.00001000000015656562;

volatile T g_dummy_store;

#define BUFSIZE 1024000
char g_buf[BUFSIZE];

int main()
{
    // Ensure that the same buffer size is used in all cases
    std::cout.sync_with_stdio(false);
    std::setvbuf(stdout, g_buf, _IOFBF, sizeof(g_buf));
    std::cout.rdbuf()->pubsetbuf(g_buf, sizeof(g_buf));

    FmtInfo fmt;
    fmt.dot = '.';
    fmt.groups = NULL;
    fmt.grouplen = 0;
    fmt.sep = '\'';

    unsigned flags = FLAG_FLT_EXP;
    unsigned long long succ = 0;
    unsigned long long fail = 0;
    unsigned long long loop = 0;

    T val = g_start;
    for (unsigned long long i = 0; i < g_test_count; ++i) {
#if TEST_LIBC
        std::printf("%.17e\n", val);
#elif TEST_LIBC_LONG
        std::printf("%.40e\n", val);
#elif TEST_CF
        print_float10(std::cout, flags, -1, 17, fmt, val, "%.17e");
        std::cout << '\n';
#elif TEST_NULL_LIBC
        std::printf("zzzzzzzzzz");
#elif TEST_NULL_CF
        std::cout.write("zzzzzzzzzz", 10);
#elif TEST_CMP
        char buf[1024];
        // The body of the following loop is run once if the formatting results
        // are the same and two times if they are not. This allows to easily
        // debug the failed cases

        for (unsigned j = 0; j < 2; ++j) {
            std::string a;
            unsigned count = std::snprintf(buf, 1024, "%.17e", val);
            a.assign(buf, count);
            std::ostringstream os;
            print_float10(os, flags, -1, 17, fmt, val, "%.17e");
            std::string b = os.str();
            if (a == b) {
                succ++;
                break;
            } else if (j == 0) {
                count = std::snprintf(buf, 1024, "%.40e\n", val);
                std::cout << a << "\n" << b << "\n";
                std::cout.write(buf, count);
                std::cout << '\n';
                std::cout.flush();
                fail++;
            }
        }
        if (i % (1024*1024) == 0) {
            std::cout << " -- " << i << "\n";
            std::cout.flush();
        }
#endif
        g_dummy_store = val;
        val *= g_mul;
        if (val > g_max) {
            loop++;
            val = -g_min * loop * g_mul_loop;
        }
        if (val < -g_max) {
            val = g_min * loop * g_mul_loop;
        }
    }
#if TEST_CMP
    std::cout << "Fail: " << fail << "\n"
              << "Success: " << succ << "\n";
#endif
}
