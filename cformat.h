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

#ifndef CFORMAT_H
#define CFORMAT_H

/// Flags for standard format string flags
enum FormatFlags {
    FLAG_NONE  = 0,
    FLAG_LEFT  = 1 << 0,
    FLAG_SIGN  = 1 << 1,
    FLAG_SPACE = 1 << 2,
    FLAG_ZERO  = 1 << 3,
    FLAG_ALT   = 1 << 4,
    FLAG_SET_WIDTH = 1 << 5,
    FLAG_SET_PREC = 1 << 6,

    FLAG_UPPERCASE = 1 << 16,       // uppercase
    FLAG_DEC = 1 << 16,             // decimal radix
    FLAG_OCT = 1 << 17,             // octal radix
    FLAG_HEX = 1 << 18,             // hexadecimal radix
    FLAG_FLT_NOR = 1 << 19,         // floating-point without exponent (f format)
    FLAG_FLT_EXP = 1 << 20,         // floating-point with exponent (e format)
    FLAG_FLT_G = 1 << 21,           // floating-point with or without exponent (g format)
};

struct FmtInfo {
    char dot;
    char sep;
    const char* groups;
    unsigned grouplen;
};

void print_float10(std::ostream& ostr, unsigned flags, int width,
                        int prec, const FmtInfo& fmt, float val, const char* fallback);
void print_float10(std::ostream& ostr, unsigned flags, int width,
                   int prec, const FmtInfo& fmt, double val, const char* fallback);

#endif
