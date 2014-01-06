/*
    Copyright (C) 2011-2014  Povilas Kanapickas <povilas@radix.lt>

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

#include <algorithm>
#include <string>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <stdint.h>

#include "cformat.h"

inline void fill_impl(std::ostream& out, unsigned count,
                      const char* spbuf, unsigned spbuf_len)
{
    while (count > 0) {
        out.write(spbuf, spbuf_len);
        count -= spbuf_len;
    }
}

static const char* fill_spaces_spbuf =
        "                                        "
        "                                        "
        "                                        "
        "                                        "
        "                                        ";

static const char* fill_zeros_spbuf =
        "0000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000";

inline char* fill_spaces(std::ostream& out, unsigned count)
{
    fill_impl(out, count, fill_spaces_spbuf, 200);
}

inline char* fill_zeros(std::ostream& out, unsigned count)
{
    fill_impl(out, count, fill_zeros_spbuf, 200);
}

enum FloatFlags {
    FLOAT_NONE = 0,
    FLOAT_NEG = 1 << 0,         // negative
    FLOAT_NAN = 1 << 1,         // not a number
    FLOAT_INF = 1 << 2,         // positive or negative infinity
};

#define USE_INT128 0

struct Uint96 {
    uint64_t hi;
    uint32_t lo;
};

// Division is performed by multiplication.

#if USE_INT128
inline uint64_t mulhi(uint64_t a, uint64_t b)
{
    return ((unsigned __int128)(a) * b) >> 64;
}
#else
inline uint64_t mulhi(uint64_t a, uint64_t b)
{
    uint32_t ah, al, bh, bl;
    ah = a >> 32;
    al = a & 0xffffffff;
    bh = b >> 32;
    bl = b & 0xffffffff;
    uint64_t r;
    //uint32_t l;

    r = uint64_t(ah) * bh;
    r += (uint64_t(bh) * al) >> 32;
    r += (uint64_t(bl) * ah) >> 32;
    return r;
}
#endif

#if USE_INT128
inline Uint96 mulhi(Uint96 a, Uint96 b)
{
    uint64_t rh, rl;
    rh = ((unsigned __int128)(a.hi) * b.hi) >> 64;
    rl = ((unsigned __int128)(a.hi) * b.hi);

    uint64_t ah32, bh32;
    ah32 = a.hi >> 32;
    bh32 = b.hi >> 32;
    rl += (uint64_t(ah32) * b.lo);
    rl += (uint64_t(bh32) * a.lo);
    rl >>= 32;
    a.hi = rh;
    a.lo = rl;
}
#else
inline Uint96 mulhi(Uint96 a, Uint96 b)
{
    /*        a1a2a3
            * b1b2b3
        r1r2r3
        a1b1
          a1b2
            a1b3
          a2b1
            a2b2
            a3b1

    */
    uint32_t a1, a2, a3;
    uint32_t b1, b2, b3;
    a1 = a.hi >> 32;
    a2 = a.hi & 0xffffffff;
    a3 = a.lo;
    b1 = b.hi >> 32;
    b2 = b.hi & 0xffffffff;
    b3 = b.lo;
    uint64_t rh, rl, i1, i2;
    rh = uint64_t(a1) * b1;
    i1 = uint64_t(a1) * b2;
    i2 = uint64_t(a2) * b1;
    rh += i1 >> 32;
    rh += i2 >> 32;
    rl = i1 & 0xffffffff;
    rl += i2 & 0xffffffff;
    rl += (uint64_t(a1) * b3) >> 32;
    rl += (uint64_t(a2) * b2) >> 32;
    rl += (uint64_t(a3) * b1) >> 32;
    rh += rl >> 32;
    rl &= 0xffffffff;
    a.hi = rh;
    a.lo = rl;
    return a;
}
#endif

struct DivDesc {
    unsigned exp2;
    unsigned exp5;
    uint64_t cf1;
    uint32_t cf2;
};

inline uint64_t muldiv(uint64_t d, const DivDesc& desc)
{
    return mulhi(d, desc.cf1);
}
inline Uint96 muldiv(Uint96 d, const DivDesc& desc)
{
    Uint96 c; c.hi = desc.cf1; c.lo = desc.cf2;
    return mulhi(d, c);
}

inline uint64_t mul(uint64_t d, unsigned i)
{
    return d*i;
}

inline Uint96 mul(Uint96 d, unsigned i)
{
    uint64_t rl, rh;
    rl = uint64_t(d.lo) * i;
    rh = d.hi * i;
    rh += rl >> 32;
    rl &= 0xffffffff;
    d.hi = rh;
    d.lo = rl;
    return d;
}

const DivDesc mul2div5_desc[] = { //FIXME: not tuned, i.e. more entries needed
    {202, 87, 0xfea126b7d78186bc, 0xe2f610c8 },
    { 65, 28, 0xfd87b5f28300ca0d, 0x8bca9d6e },
    { 23, 10, 0xdbe6fecebdedd5be, 0xb573440e },
    {  6,  3, 0x83126e978d4fdf3b, 0x645a1cac },
    {  2,  1, 0xcccccccccccccccc, 0xcccccccc },
};

const DivDesc mul5div2_desc[] = { //FIXME: not tuned, i.e. more entries needed
    {209, 90, 0xfb5878494ace3a5f, 0x04ab48a0 },
    { 72, 31, 0xfc6f7c4045812296, 0x4d000000 },
    { 28, 12, 0xe8d4a51000000000, 0x00000000 },
    { 14,  6, 0xf424000000000000, 0x00000000 },
    {  7,  3, 0xfa00000000000000, 0x00000000 },
    {  3,  1, 0xa000000000000000, 0x00000000 }
};

// shift - at most 32 bits
inline uint64_t shift_r(uint64_t d, unsigned i) { return d >> i; }
inline uint64_t shift_l(uint64_t d, unsigned i) { return d << i; }
inline Uint96 shift_r(Uint96 d, unsigned i)
{
    d.lo >>= i;
    d.lo |= uint32_t(d.hi & 0xffffffff) << 32-i;
    d.hi >>= i;
    return d;
}
inline Uint96 shift_l(Uint96 d, unsigned i)
{
    d.hi <<= i;
    d.hi |= d.lo >> 32-i;
    d.lo <<= i;
    return d;
}

/*  IEEE754 32-bit float
    1 bit: sign
    8 bits: exponent
    23 bits: significand (24th bit is implicitly set, but only if the number is
        not denormal

    IEEE754 64-bit float
    1 bit: sign
    11 bits: exponent
    52 bits: significand (53th bit is implicitly set, but only if the number is
        not denormal

    The function sets @a signif and @a exp only if the number not infinity or
    NaN.

    Significand is set in such a way that the (implicit) MSB of the significand
    is shifted to the most significand position of signif.
*/
void decompose(float f, unsigned& fltflags, uint64_t& signif, int& exp)
{
    uint32_t fi;
    std::memcpy(&fi, &f, sizeof(f));
    fltflags = ((fi >> 31) == 1) ? FLOAT_NEG : FLOAT_NONE;
    fi &= 0x7fffffff;
    if (fi == 0x7fffffff) {
        fltflags |= FLOAT_INF;
        return;
    }
    exp = fi >> 24;
    if (exp == 0xff) {
        fltflags |= FLOAT_NAN;
        return;
    }
    signif = ((uint64_t) (fi | 0x00800000)) << 8+32;
    exp -= 127;
}

void decompose(double f, unsigned& fltflags, Uint96& signif, int& exp)
{
    uint64_t fi;
    std::memcpy(&fi, &f, sizeof(f));
    fltflags = ((fi >> 63) == 1) ? FLOAT_NEG : FLOAT_NONE;
    fi &= 0x7fffffffffffffff;
    if (fi == 0x7fffffffffffffff) {
        fltflags |= FLOAT_INF;
        return;
    }
    exp = fi >> 52;
    if (exp == 0xfff) {
        fltflags |= FLOAT_NAN;
        return;
    }
    signif.hi = (fi | 0x0010000000000000) << 11;
    signif.lo = 0;
    exp -= 1023;
}

const char* get_special(unsigned fltflags, unsigned flags)
{
    if (fltflags & FLOAT_NAN) {
        const char* sp;
        if (flags & FLAG_UPPERCASE) {
            return "NAN";
        } else {
            return "nan";
        }
    }
    if (flags & FLAG_UPPERCASE) {
        return "INF";
    } else {
        return "inf";
    }
}



/** We must keep as many significant digits in signif as possible at all times.
    This function shifts signif by one position and updates the exponent in
    case the msb of signif becomes zero.
*/
inline uint64_t renormalize_signif(uint64_t signif, int& exp)
{
    if (exp > 0) {
        unsigned shift = (signif >> 63) ^ 1; // shift iff the top 4 bits are
        signif <<= shift;
        exp -= shift;
    }
    return signif;
}

inline Uint96 renormalize_signif(Uint96 signif, int& exp)
{
    if (signif.hi & 0xf000000000000000 == 0) {
        signif = shift_l(signif, 4);
        exp -= 4;
    }
    return signif;
}

inline unsigned extract_signif_digit(uint64_t signif) { return signif >> 60; }
inline uint64_t clear_signif_digit(uint64_t signif)   { return signif & ~0xf000000000000000; }
inline unsigned extract_signif_digit(Uint96 signif) { return signif.hi >> 60; }
inline Uint96 clear_signif_digit(Uint96 signif)     { signif.hi &= ~0xf000000000000000; return signif; }
inline bool is_zero(uint64_t d) { return d == 0; }
inline bool is_zero(Uint96 d) { return d.hi == 0 && d.lo == 0; }


/// Extracts a digit and prepares the signif for next extraction
template<class T>
inline unsigned extract_update_signif_digit(T& signif)
{
    unsigned res = extract_signif_digit(signif);
    signif = clear_signif_digit(signif);
    signif = mul(signif, 10);
    return res;
}

template<class T>
inline void print_sprintf(std::ostream& ostr, T val, const char* fmt)
{
    char buf[1024]; // FIXME
    int num = std::snprintf(buf, 1024, fmt, val);
    if (num > 0) {
        ostr.write(buf, num);
    }
}

/** - T is the floating-point type to convert
    - U is an uint64_t for float and Uint96 for double.
    - max_digits determines the maximum number of significant digits the
        function should attempt to write.
    - max_digits_round is similar to max_digits just determines the number of
        significant digits to analyze during rounding. If rounding decision
        can not be made from that many digits, snprintf is used.
    - fallback is a printf-style string to use for snprintf if it turns out
        that the internal precision is insufficient.
*/
template<class U, class T>
void print_float10_impl(std::ostream& ostr, unsigned flags, int width,
                        int prec, const FmtInfo& fmt, T val, unsigned max_digits,
                        unsigned max_digits_round, const char* fallback)
{
    /** The precision losses are as follows:
         * 5 bits due to range reduction until renormalization
         * 5.4 bits due to imprecise division via multiplication ( 15 operations
            each 1.5 bits max (log2(15*2^1.5))

        For double:
            96 bits total precision, 11 bits loss, 85 bits usable precision.
            That's roughly 25 decimal digits. Use 25 for max_digits_round and
            21 for max_digits.
        For float:
            64 bits total precision, 11 bits loss, 55 bits usable precision.
            Roughly 16 decimal digits. Use 16 for max_digits_round and 12 for
            max_digits
    */

    U signif;
    int exp;
    unsigned fltflags;

    decompose(val, fltflags, signif, exp);

    // infinity and nan
    if (fltflags & (FLOAT_INF | FLOAT_NAN)) {
        // return print_float_special(buf, width, fltflags, flags);
        throw std::runtime_error("Dummy");
    }

    if (((flags & FLAG_FLT_NOR) && (exp / 3 > max_digits)) ||
        ((flags & FLAG_FLT_EXP) && (prec + 1 > max_digits))) {
        print_sprintf(ostr, val, fallback);
        return;
    }

    int exp10 = 0;

    // the significand now contains single 'non-fractional' bit at MSB

    /*  The number is 1.xxx*2^n in binary.

        We need to convert it to base10 encoding. This means multiplying by 2^n,
        then dividing by such 10^m that the resulting number is within 1..10.
        Then we can extract the digits by looking at the bits left to the 'dot'
        position, zeroing them and multiplying the signif by 10.

        Note, that the first process is effectively a conversion of 2^n to
        appropriate 10^m. We pick such pairs of n,m that 2^n/5^m is as close to
        1 as possible. This way we can keep the precision of computations near
        the maximum 64 bits at all times and perform everything without needing
        multiprecision math.
    */

    // multiply by 2^exp
    if (exp >= 0) {
        const DivDesc* desc = mul2div5_desc;

        for (unsigned i = 0; i < sizeof(mul2div5_desc)/sizeof(*mul2div5_desc); ++i, ++desc) {

            unsigned step = desc->exp2 + desc->exp5;
            while (exp >= step) {
                signif = muldiv(signif, *desc);
                exp10 += desc->exp5;
                exp -= step;
                signif = renormalize_signif(signif, exp);
            }
        }
        // make space for a base-10 digit
        signif = shift_r(signif, 3-exp); // consume the remaining exponent (at most 2)
        while (extract_signif_digit(signif) == 0) {
            signif = mul(signif, 10);
            exp10 -= 1;
        }
    } else {
        exp = -exp;

        const DivDesc* desc = mul5div2_desc;
        for (unsigned i = 0; i < sizeof(mul5div2_desc)/sizeof(*mul5div2_desc); ++i, ++desc) {

            unsigned step = desc->exp2 + desc->exp5;
            while (exp >= step) {
                signif = muldiv(signif, *desc);
                exp10 -= desc->exp5;
                exp -= step;
                signif = renormalize_signif(signif, exp);
            }
        }
        // make space for a base-10 digit
        // consume the remaining exponent (at most 3). This may make the first
        // digit zero, we must fix this since it would make exp10 incorrect
        signif = shift_r(signif, 3+exp);
        while (extract_signif_digit(signif) == 0) {
            signif = mul(signif, 10);
            exp10 -= 1;
        }
    }

    /*  Calculate the estimate the space needed and various other bits.

        Here we can have three main formats:

        'f','F': [-]ddd.ddd

        Precision specifies the minimum number of digits to appear after the
        decimal point character. The default precision is 6. In the alternative
        implementation decimal point character is written even if no digits
        follow it.

        'e','E': [-]d.ddde±dd

        The exponent contains at least two digits, more digits are used only if
        necessary. If the value is ​0​, the exponent is also ​0​. Precision
        specifies the minimum number of digits to appear after the decimal
        point character. The default precision is 6. In the alternative
        implementation decimal point character is written even if no digits
        follow it.

        'g','G': Equivalent to either 'e/E' or 'f/F'

        Let P equal the precision if nonzero, 6 if the precision is not
        specified, or 1 if the precision is ​0​. Then, if a conversion with
        style E would have an exponent of X:

         * if P > X ≥ −4, the conversion is with style f or F and precision
            P − 1 − X.
         * otherwise, the conversion is with style e or E and precision P − 1.

        Unless alternative representation is requested the trailing zeros are
        removed, also the decimal point character is removed if no fractional
        part is left. For infinity and not-a-number conversion style see notes.
    */
    unsigned int_digits_max, frac_digits_min, frac_digits_max;

    int gprec;

    unsigned buf_size;

    if (flags & FLAG_FLT_NOR) {
        // normal notation [-]ddd.ddd
        prec = (prec < 0) ? 6 : prec;
        int_digits_max = (exp10 >= 0) ? exp10 + 1 : 1;
        frac_digits_min = prec;
        frac_digits_max = prec;
        buf_size = int_digits_max + 1 + frac_digits_max;

    } else if (flags & FLAG_FLT_EXP) {
        // exponential notation [-]d.ddde±dd
        prec = (prec < 0) ? 6 : prec;
        int_digits_max = 1;
        frac_digits_min = prec;
        frac_digits_max = prec;
        buf_size = int_digits_max + 1 + frac_digits_max + 6; // e[+-]\d\d\d\d

    } else {
        // 'g': either of the above with custom rules
        flags |= FLAG_FLT_G;
        prec = (prec < 0)  ? 6 : prec;
        prec = (prec == 0) ? 1 : prec;
        gprec = prec;
        if ((prec > exp10) && (exp10 >= -4)) {
            // normal notation
            flags |= FLAG_FLT_NOR;
            prec = prec - 1 - exp10;
            int_digits_max = (exp10 >= 0) ? exp10 + 1 : 1;
        } else {
            // exponential notation
            flags |= FLAG_FLT_EXP;
            prec = prec - 1;
            int_digits_max = 1;
        }
        frac_digits_max = prec;
        frac_digits_min = 0; // can erase trailing zeros
        if (flags & FLAG_ALT) {
            frac_digits_min = prec;
        }
        // Either e[+-]\d\d\d\d or four prepended zeros
        buf_size = int_digits_max + 1 + frac_digits_max + 6;
    }

    // Take thousand separators into account when estimating needed characters
    unsigned sep_count = 0;
    if (fmt.grouplen > 0) {
        // Rounding may create additional digit and an additional separator
        //sep_count = count_thousand_seps(fmt.groups, fmt.grouplen, int_digits_max+1);
        //buf_size += sep_count;
    }

    // Reserve space
    //buf.reserve(buf_size+padspace);
    bool is_buf_alloced = false;
    char static_buf[128];
    char* buf;
    if (buf_size > 128) {
        buf = new char[buf_size];   // FIXME: use RAII
        is_buf_alloced = true;
    } else {
        buf = static_buf;
    }

    //char* bufbegin = buf.get() + padspace;
    char* bufbegin = buf;

    char* outbeg = bufbegin + 2 + sep_count;// Grouping will be added right to left
                                            // Also add space for sign and rouding
    char* out = outbeg;

    unsigned idig = 0;
    char last_signif;

    // Extract the integral part, add the decimal dot
    if (exp10 >= 0 || flags & FLAG_FLT_EXP) {
        for (unsigned i = 0; i < int_digits_max; ++i, ++idig) {
            *out++ = last_signif = extract_update_signif_digit(signif) + '0';
        }

    } else {
        // The representation has no integral part
        exp10++;
        *out++ = '0';
    }

    // Always add the dot. We can remove it later.
    char* dotpos = out;
    *out++ = '.'; // just use a known value, we'll change it to fmt.dot later

    // Extract the fractional part
    for (unsigned i = 0; i < frac_digits_max; ++i, ++idig) {
        *out++ = last_signif = extract_update_signif_digit(signif) + '0';
    }

    // Round using "round to nearest" mode (halfway cases are rounded to even)
    // TODO: Use current rounding mode

    // Since the significand is not precise we must be extremely careful errors
    // do not propagate to rounding.
    bool round_away_zero = false;
    char hidden_digit = extract_signif_digit(signif) + '0';
    if (hidden_digit > '5') {
        round_away_zero = true;
    } else if (hidden_digit == '5') {
        // We consider at most decimal_dig + 2 digits.
        extract_update_signif_digit(signif); // previous extraction did not
                                             // update signif
        unsigned idig2 = idig + 1;
        char dig = '0';
        while (dig == '0' && idig2 < max_digits_round) {
            dig = extract_update_signif_digit(signif) + '0';
            idig2++;
        }

        if (dig > '0') {
            round_away_zero = true;
        } else if (idig2 == max_digits_round) {
            if (is_buf_alloced) {
                delete[] buf;
            }
            print_sprintf(ostr, val, fallback);
            return;
        }
    } else if (hidden_digit == '4') {
        unsigned idig2 = idig + 1;
        extract_update_signif_digit(signif);
        char dig = extract_update_signif_digit(signif) + '0';
        while (dig == '9' && idig2 < max_digits_round) {
            dig = extract_update_signif_digit(signif) + '0';
            idig2++;
        }
        if (dig == '9' && idig2 == max_digits_round) {
            if (is_buf_alloced) {
                delete[] buf;
            }
            print_sprintf(ostr, val, fallback);
            return;
        }
    }

    if (round_away_zero) {
        char* oi = out-1;

        // Fractional part
        bool rounded = false;
        if (*oi != '.') {

            while (*oi == '9') {
                *oi-- = '0';
            }
            if (*oi != '.') {
                *oi += 1;
                rounded = true;
            }
        }

        // Integer part
        if (!rounded) {
            // *oi == '.'
            oi--;
            while (*oi == '9') {
                *oi-- = '0';
            }
            // Either we found a non-9 digit or we are at the beginning
            if (oi >= outbeg) {
                *oi += 1;
            } else {
                // The number was 9.9999... *10^m
                // We need to add an additional digit, i.e. exp10 has changed.
                // No additional digits become 'visible' thus rounding does not
                // need to be repeated
                *oi = '1';
                outbeg--;
                exp10++;

                if (flags & FLAG_FLT_G) {
                    // As exp10 has changed, we may need to switch the format.
                    // That's easy as the number is 10^n.

                    if (flags & FLAG_FLT_NOR && gprec == exp10) {
                        flags &= ~FLAG_FLT_NOR;
                        flags |= FLAG_FLT_EXP;
                        prec = prec + exp10; // or gprec - 1

                        // Reprint the number
                        out = bufbegin;
                        out = std::strncpy(out, "1.", 2);
                        dotpos = bufbegin + 1;
                        out = std::fill_n(out, prec, '0');

                    } else if (flags & FLAG_FLT_EXP && exp10 == -4) {
                        flags &= ~FLAG_FLT_EXP;
                        flags |= FLAG_FLT_NOR;
                        prec = prec - exp10; // or gprec - 1 - exp10

                        // Reprint the number
                        out = bufbegin;
                        out = std::strncpy(out, "0.0001", 6);
                        dotpos = bufbegin + 1;
                        out = std::fill_n(out, prec - 4, '0');
                    }
                    frac_digits_max = prec;
                    frac_digits_min = 0; // can erase trailing zeros
                    if (flags & FLAG_ALT) {
                        frac_digits_min = prec;
                    }
                } else if (flags & FLAG_FLT_NOR) {
                    // nothing to do, precision did not change
                } else if (flags & FLAG_FLT_EXP) {
                    // reposition the dot
                    *dotpos = *(dotpos-1);
                    *--dotpos = '.';
                    out--;
                }
            }
        }
    }

    // Remove trailing zeros and dot if possible
    char* oi = out-1;
    while ((oi != dotpos + frac_digits_min) && (*oi == '0')) {
        oi--;
    }

    if (oi == dotpos && !(flags & FLAG_FLT_G)) {
        oi--;
    } else {
        // Using fixed value for dot no longer beneficial
        *dotpos = fmt.dot;
    }
    out = oi+1;

    // Add thousand separators
    if (fmt.grouplen > 0) {
        //outbeg = add_thousand_seps(outbeg, fmt.sep, fmt.groups, fmt.grouplen,
        //                            outbeg, dotpos);
    }

    // Add exponent
    if (flags & FLAG_FLT_EXP) {
        *out++ = (flags & FLAG_UPPERCASE) ? 'E' : 'e';
        *out++ = (exp10 < 0) ? '-' : '+';
        exp10 = std::abs(exp10);

        if (exp10 < 10) {
            *out++ = '0';
            *out++ = '0' + exp10;
        } else if (exp10 < 100) {
            *out++ = '0' + exp10 / 10;
            *out++ = '0' + exp10 % 10;
        } else {
            *out++ = '0' + exp10 / 10 / 10;
            *out++ = '0' + exp10 / 10 % 10;
            *out++ = '0' + exp10 % 10;
        }
    }

    // Write to buffer along to any padding
    char sign[2];
    sign[0] = ' ';
    unsigned sign_sz = 1;
    if (fltflags & FLOAT_NEG) {
        sign[0] = '-';
    } else if (flags & FLAG_SIGN) {
        sign[0] = '+';
    } else if (flags & FLAG_SPACE) {
        sign[0] = ' ';
    } else {
        sign_sz = 0;
    }

    width -= out - outbeg - sign_sz;

    if (flags & FLAG_LEFT) {
        // left adjusted
        if (sign_sz > 0) {
            ostr.put(sign[0]);
        }
        ostr.write(outbeg, out-outbeg);
        if (width > 0) {
            fill_spaces(ostr, width);
        }
        return;
    }

    if (flags & FLAG_ZERO) {
        // fill zeros between pre and data
        if (sign_sz > 0) {
            ostr.put(sign[0]);
        }
        if (width > 0) {
            fill_zeros(ostr, width);
        }
        ostr.write(outbeg, out-outbeg);
    }

    // right adjusted
    if (width > 0) {
        fill_spaces(ostr, width);
    }
    if (sign_sz > 0) {
        ostr.put(sign[0]);
    }
    ostr.write(outbeg, out-outbeg);
    if (is_buf_alloced) {
        delete[] buf;
    }
}

void __attribute__((noinline))
     print_float10(std::ostream& ostr, unsigned flags, int width,
                        int prec, const FmtInfo& fmt, float val, const char* fallback)
{
    print_float10_impl<uint64_t>(ostr, flags, width, prec, fmt, val, 11, 16, fallback);
}

void __attribute__((noinline))
     print_float10(std::ostream& ostr, unsigned flags, int width,
                   int prec, const FmtInfo& fmt, double val, const char* fallback)
{
    print_float10_impl<Uint96>(ostr, flags, width, prec, fmt, val, 21, 24, fallback);
}
