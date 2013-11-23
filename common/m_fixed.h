// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2013 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Fixed point arithemtics, implementation.
//
//-----------------------------------------------------------------------------


#ifndef __M_FIXED_H__
#define __M_FIXED_H__

#include <stdlib.h>
#include "doomtype.h"

//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS				16
#define FRACUNIT				(1<<FRACBITS)

typedef int fixed_t;				// fixed 16.16
typedef unsigned int dsfixed_t;		// fixedpt used by span drawer

//
// Fixed Point / Floating Point Conversion
//
inline float FIXED2FLOAT(fixed_t x)
{
	static const float factor = 1.0f / float(FRACUNIT);
	return x * factor;
}

inline double FIXED2DOUBLE(fixed_t x)
{
	static const double factor = 1.0 / double(FRACUNIT);
	return x * factor;
}

inline fixed_t FLOAT2FIXED(float x)
{
	return fixed_t(x * float(FRACUNIT));
}

inline fixed_t DOUBLE2FIXED(double x)
{
	return fixed_t(x * double(FRACUNIT));
}

inline int FIXED2INT(fixed_t x)
{
	return (x + FRACUNIT / 2) / FRACUNIT;
}

inline fixed_t INT2FIXED(int x)
{
	return x << FRACBITS;
}


//
// Fixed-point multiplication for 16.16 precision
//
inline static fixed_t FixedMul(fixed_t a, fixed_t b)
{
	return (fixed_t)(((int64_t)a * b) >> FRACBITS);
}

//
// Fixed-point division for 16.16 precision
//
inline static fixed_t FixedDiv(fixed_t a, fixed_t b)
{
	return (abs(a) >> 14) >= abs(b) ? ((a ^ b) >> 31) ^ MAXINT :
		(fixed_t)(((int64_t)a << FRACBITS) / b);
}

//
// Fixed-point inversion for 16.16 precision
//
inline static fixed_t FixedInvert(fixed_t a)
{
	return (fixed_t)(0xFFFFFFFFu / uint32_t(a));
}

//
// Arbitrary fixed-point division
//
// The bits_a and bits_b template parameters indicate how many bits of
// precision are used for the fractional parts of a and b respectively.
// The bits_res parameter indicates how many fractional bits of precision
// for the result.
// [SL] Note: the compiler should completely optimize away the branch because
// shift is calculated at compile time.
//
template <int bits_a, int bits_b, int bits_res>
static inline int32_t FixedMul(int32_t a, int32_t b)
{
	const int shift = bits_res - bits_a - bits_b;
	if (shift >= 0)
		return (int32_t)((int64_t(a) * b) << shift);
	else
		return (int32_t)((int64_t(a) * b) >> -shift);
}

//
// Arbitrary fixed-point division
//
// The bits_a and bits_b template parameters indicate how many bits of
// precision are used for the fractional parts of a and b respectively.
// The bits_res parameter indicates how many fractional bits of precision
// for the result.
// [SL] Note: the compiler should completely optimize away the branch because
// shift is calculated at compile time.
//
template <int bits_a, int bits_b, int bits_res>
static inline int32_t FixedDiv(int32_t a, int32_t b)
{
	int shift = bits_res - bits_a + bits_b;
	if (shift >= 0)
		return (int32_t)((int64_t(a) << shift) / b);
	else 
		return (int32_t)((int64_t(a) >> -shift) / b);
}


//
// Units representing the value 1.0 with a given number of bits of precision.
//

#define FRACUNIT1	(1 << 1)
#define FRACUNIT2	(1 << 2)
#define FRACUNIT3	(1 << 3)
#define FRACUNIT4	(1 << 4)
#define FRACUNIT5	(1 << 5)
#define FRACUNIT6	(1 << 6)
#define FRACUNIT7	(1 << 7)
#define FRACUNIT8	(1 << 8)
#define FRACUNIT9	(1 << 9)
#define FRACUNIT10	(1 << 10)
#define FRACUNIT11	(1 << 11)
#define FRACUNIT12	(1 << 12)
#define FRACUNIT13	(1 << 13)
#define FRACUNIT14	(1 << 14)
#define FRACUNIT15	(1 << 15)
#define FRACUNIT16	(1 << 16)
#define FRACUNIT17	(1 << 17)
#define FRACUNIT18	(1 << 18)
#define FRACUNIT19	(1 << 19)
#define FRACUNIT20	(1 << 20)
#define FRACUNIT21	(1 << 21)
#define FRACUNIT22	(1 << 22)
#define FRACUNIT23	(1 << 23)
#define FRACUNIT24	(1 << 24)
#define FRACUNIT25	(1 << 25)
#define FRACUNIT26	(1 << 26)
#define FRACUNIT27	(1 << 27)
#define FRACUNIT28	(1 << 28)
#define FRACUNIT29	(1 << 29)
#define FRACUNIT30	(1 << 30)
#define FRACUNIT31	(1 << 31)
#define FRACUNIT32	(1 << 32)

template <int bits = 16>
class fixed_generic_t
{
public:
	fixed_generic_t() { }
	inline fixed_generic_t(int32_t v) : value(v) { }
	inline fixed_generic_t<bits>& operator=(int32_t v) { value = v; return *this; }
	inline fixed_generic_t<bits>& operator=(const fixed_generic_t<bits>& other) { value = other.value; return *this; }
	inline operator int32_t() { return value; }
	
private:
	int32_t		value;
		
	template <int dest_bits, int a_bits, int b_bits>
	friend fixed_generic_t<dest_bits> FixedMul(fixed_generic_t<a_bits> a, fixed_generic_t<b_bits> b);

	template <int dest_bits, int a_bits, int b_bits>
	friend fixed_generic_t<dest_bits> FixedDiv(fixed_generic_t<a_bits> a, fixed_generic_t<b_bits> b);
};

template <int dest_bits, int a_bits, int b_bits>
inline fixed_generic_t<dest_bits> FixedMul(fixed_generic_t<a_bits> a, fixed_generic_t<b_bits> b)
{
	int shift = dest_bits - a_bits - b_bits;
	if (shift >= 0)
		return fixed_generic_t<dest_bits>((int32_t)((int64_t(a.value) * b.value) << shift));
	else
		return fixed_generic_t<dest_bits>((int32_t)((int64_t(a.value) * b.value) >> -shift));
}

template <int dest_bits, int a_bits, int b_bits>
inline fixed_generic_t<dest_bits> FixedDiv(fixed_generic_t<a_bits> a, fixed_generic_t<b_bits> b)
{
	int shift = dest_bits - a_bits + b_bits;
	if (shift >= 0)
		return fixed_generic_t<dest_bits>((int32_t)((int64_t(a.value) << shift) / b.value));
	else
		return fixed_generic_t<dest_bits>((int32_t)((int64_t(a.value) >> -shift) / b.value));
}

template <int a_bits, int b_bits>
inline fixed_generic_t<16> FixedMul(fixed_generic_t<a_bits> a, fixed_generic_t<b_bits> b)
{
	return FixedMul<16>(a, b);
}

template <int a_bits, int b_bits>
inline fixed_generic_t<16> FixedDiv(fixed_generic_t<a_bits> a, fixed_generic_t<b_bits> b)
{
	return FixedDiv<16>(a, b);
}


typedef fixed_generic_t<1>		fixed1_t;
typedef fixed_generic_t<2>		fixed2_t;
typedef fixed_generic_t<3>		fixed3_t;
typedef fixed_generic_t<4>		fixed4_t;
typedef fixed_generic_t<5>		fixed5_t;
typedef fixed_generic_t<6>		fixed6_t;
typedef fixed_generic_t<7>		fixed7_t;
typedef fixed_generic_t<8>		fixed8_t;
typedef fixed_generic_t<9>		fixed9_t;
typedef fixed_generic_t<10>		fixed10_t;
typedef fixed_generic_t<11>		fixed11_t;
typedef fixed_generic_t<12>		fixed12_t;
typedef fixed_generic_t<13>		fixed13_t;
typedef fixed_generic_t<14>		fixed14_t;
typedef fixed_generic_t<15>		fixed15_t;
typedef fixed_generic_t<16>		fixed16_t;
typedef fixed_generic_t<17>		fixed17_t;
typedef fixed_generic_t<18>		fixed18_t;
typedef fixed_generic_t<19>		fixed19_t;
typedef fixed_generic_t<20>		fixed20_t;
typedef fixed_generic_t<21>		fixed21_t;
typedef fixed_generic_t<22>		fixed22_t;
typedef fixed_generic_t<23>		fixed23_t;
typedef fixed_generic_t<24>		fixed24_t;
typedef fixed_generic_t<25>		fixed25_t;
typedef fixed_generic_t<26>		fixed26_t;
typedef fixed_generic_t<27>		fixed27_t;
typedef fixed_generic_t<28>		fixed28_t;
typedef fixed_generic_t<29>		fixed29_t;
typedef fixed_generic_t<30>		fixed30_t;
typedef fixed_generic_t<31>		fixed31_t;
typedef fixed_generic_t<32>		fixed32_t;

#endif	// __M_FIXED_H__


