#   Copyright (C) 2014  Povilas Kanapickas <povilas@radix.lt>
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# The programs are as follows:
#
# libc - Uses standard glibc printf
# libc_long - Uses standard glibc printf with precision of 40 decimal digits
# cf - Optimized printf
# null_libc - Same as 'libc', just with the actual 'printf' removed. Useful for
#   estimation of how much the printf call itself costs
# null_cf - Similar to 'null_libc' except that the infrastructure of 'cf' is
#   tested.
# cmp - Compares libc and cf results. Prints only when the formatted strings do
#   not match

# The 'test' target runs 'libc', 'null_libc', 'cf' and 'null_cf', pipes the
# output to /dev/null and prints the amount of *user* time used by each
# program. Be sure to disable CPU frequency scaling before running this.

PROGRAMS=libc libc_long cf null_libc null_cf cmp
all: $(PROGRAMS)

SOURCES=test.cc cformat.h cformat.cc
CFLAGS=-O3 -fno-lto
# WANT_ASM= -masm=intel --save-temps

libc: $(SOURCES)
	g++ $(CFLAGS) -DTEST_LIBC=1 cformat.cc test.cc -o libc
libc_long: $(SOURCES)
	g++ $(CFLAGS) -DTEST_LIBC_LONG=1 cformat.cc test.cc -o libc_long
cf: $(SOURCES)
	g++ $(CFLAGS) -DTEST_CF=1 $(WANT_ASM) cformat.cc test.cc -o cf
null_libc: $(SOURCES)
	g++ $(CFLAGS) -DTEST_NULL_LIBC=1 cformat.cc test.cc -o null_libc
null_cf: $(SOURCES)
	g++ $(CFLAGS) -DTEST_NULL_CF=1 cformat.cc test.cc -o null_cf
cmp: $(SOURCES)
	g++ $(CFLAGS) -DTEST_CMP=1 cformat.cc test.cc -o cmp

clean:
	rm -f $(PROGRAMS)

test: all
	@time -f " cf        time: %U " ./cf > /dev/null
	@time -f " libc      time: %U " ./libc > /dev/null
	@time -f " null_cf   time: %U " ./null_cf > /dev/null
	@time -f " null_libc time: %U " ./null_libc > /dev/null

testout: all
	./cf > out.cf
	./libc > out.libc
	./libc_long > out.long
