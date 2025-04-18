// The libMesh Finite Element Library.
// Copyright (C) 2002-2025 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef LIBMESH_HASHWORD_H
#define LIBMESH_HASHWORD_H

// ::size_t is defined in the backward compatibility header stddef.h.
// It's been part of ANSI/ISO C and ISO C++ since their very
// beginning. Every C++ implementation has to ship with stddef.h
// (compatibility) and cstddef where only the latter defines
// std::size_t and not necessarily ::size_t. See Annex D of the C++
// Standard.
//
// http://stackoverflow.com/questions/237370/does-stdsize-t-make-sense-in-c
#include <stddef.h>
#include <stdint.h> // uint32_t, uint64_t
#include <vector>

#include "libmesh_common.h" // libmesh_error_msg(), libmesh_fallthrough

// Anonymous namespace for utility functions used locally
namespace
{
/**
 * Rotate x by k bits.
 *
 * \author Bob Jenkins
 * \date May 2006
 * \copyright Public Domain
 * http://burtleburtle.net/bob/hash/index.html
 */
inline
uint32_t rot(uint32_t x, uint32_t k)
{
  return (x<<k) | (x>>(32-k));
}



/**
 * Mix 3 32-bit values reversibly.
 *
 * \author Bob Jenkins
 * \date May 2006
 * \copyright Public Domain
 * http://burtleburtle.net/bob/hash/index.html
 */
inline
void mix(uint32_t & a, uint32_t & b, uint32_t & c)
{
  a -= c;  a ^= rot(c, 4);  c += b;
  b -= a;  b ^= rot(a, 6);  a += c;
  c -= b;  c ^= rot(b, 8);  b += a;
  a -= c;  a ^= rot(c,16);  c += b;
  b -= a;  b ^= rot(a,19);  a += c;
  c -= b;  c ^= rot(b, 4);  b += a;
}


/**
 * Perform a "final" mixing of 3 32-bit numbers, result is stored in c.
 *
 * \author Bob Jenkins
 * \date May 2006
 * \copyright Public Domain
 * http://burtleburtle.net/bob/hash/index.html
 */
inline
void final_mix(uint32_t & a, uint32_t & b, uint32_t & c)
{
  c ^= b; c -= rot(b,14);
  a ^= c; a -= rot(c,11);
  b ^= a; b -= rot(a,25);
  c ^= b; c -= rot(b,16);
  a ^= c; a -= rot(c,4);
  b ^= a; b -= rot(a,14);
  c ^= b; c -= rot(b,24);
}


/**
 * fnv_64_buf - perform a 64 bit Fowler/Noll/Vo hash on a buffer.
 * http://www.isthe.com/chongo/tech/comp/fnv/index.html
 *
 * \author Phong Vo (http://www.research.att.com/info/kpv/)
 * \author Glenn Fowler (http://www.research.att.com/~gsf/)
 * \author Landon Curt Noll (http://www.isthe.com/chongo/)
 * \date 1991, 2012
 * \copyright Public Domain
 *
 * \param buf start of buffer to hash
 * \param len length of buffer in octets
 * \returns 64 bit hash as a static hash type
 */
uint64_t fnv_64_buf(const void * buf, size_t len)
{
  // Initializing hval with this value corresponds to the FNV-1 hash algorithm.
  uint64_t hval = static_cast<uint64_t>(0xcbf29ce484222325ULL);

  // start of buffer
  const unsigned char * bp = static_cast<const unsigned char *>(buf);

  // beyond end of buffer
  const unsigned char * be = bp + len;

  // FNV-1 hash each octet of the buffer
  while (bp < be)
    {
      // This line computes hval *= FNV_Prime, where
      // FNV_Prime := 1099511628211
      //            = 2^40 + 2^8 + 179
      // is the prime number used for for 64-bit hashes.
      // See also: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
      hval +=
        (hval << 1) + (hval << 4) + (hval << 5) +
        (hval << 7) + (hval << 8) + (hval << 40);

      // xor the bottom with the current octet
      hval ^= static_cast<uint64_t>(*bp++);
    }

  // return our new hash value
  return hval;
}

} // end anonymous namespace



namespace libMesh
{
namespace Utility
{
/**
 * The hashword function takes an array of uint32_t's of length 'length'
 * and computes a single key from it.
 *
 * \author Bob Jenkins
 * \date May 2006
 * \copyright Public Domain
 * http://burtleburtle.net/bob/hash/index.html
 */
inline
uint32_t hashword(const uint32_t * k, size_t length, uint32_t initval=0)
{
  uint32_t a,b,c;

  // Set up the internal state
  a = b = c = 0xdeadbeef + ((static_cast<uint32_t>(length))<<2) + initval;

  //------------------------------------------------- handle most of the key
  while (length > 3)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 3;
      k += 3;
    }

  //------------------------------------------- handle the last 3 uint32_t's
  switch(length)                     // all the case statements fall through
    {
    case 3 : c+=k[2];
      libmesh_fallthrough();
    case 2 : b+=k[1];
      libmesh_fallthrough();
    case 1 : a+=k[0];
      final_mix(a,b,c);
      libmesh_fallthrough();
    default:     // case 0: nothing left to add
      break;
    }

  //------------------------------------------------------ report the result
  return c;
}



/**
 * Calls function above with slightly more convenient std::vector interface.
 */
inline
uint32_t hashword(const std::vector<uint32_t> & keys, uint32_t initval=0)
{
  return hashword(keys.data(), keys.size(), initval);
}


/**
 * This is a hard-coded version of hashword for hashing exactly 2 numbers.
 *
 * \author Bob Jenkins
 * \date May 2006
 * \copyright Public Domain
 * http://burtleburtle.net/bob/hash/index.html
 */
inline
uint32_t hashword2(const uint32_t & first, const uint32_t & second, uint32_t initval=0)
{
  uint32_t a,b,c;

  // Set up the internal state
  a = b = c = 0xdeadbeef + 8 + initval;

  b+=second;
  a+=first;
  final_mix(a,b,c);

  return c;
}

/**
 * Computes the same hash as calling fnv_64_buf() with exactly two entries.
 * This function allows the compiler to optimize by unrolling loops whose
 * number of iterations are known at compile time. By inspecting the assembly
 * generated for different optimization levels, we observed that the compiler
 * sometimes chooses to unroll only the outer loop, but may also choose to
 * unroll both the outer and inner loops.
 */
inline
uint64_t hashword2(const uint64_t first, const uint64_t second)
{
  // Initializing hval with this value corresponds to the FNV-1 hash algorithm.
  uint64_t hval = static_cast<uint64_t>(0xcbf29ce484222325ULL);

  for (int i=0; i!=2; ++i)
    {
      // char pointers to (start, end) of either "first" or "second". We interpret
      // the 64 bits of each input 64-bit integer as 8 8-byte characters.
      auto beg = reinterpret_cast<const unsigned char *>(i==0 ? &first : &second);
      auto end = beg + sizeof(uint64_t)/sizeof(unsigned char); // beg+8

      // FNV-1 hash each octet of the buffer
      while (beg < end)
        {
          hval +=
            (hval << 1) + (hval << 4) + (hval << 5) +
            (hval << 7) + (hval << 8) + (hval << 40);

          // xor the bottom with the current octet
          hval ^= static_cast<uint64_t>(*beg++);
        }
    }

  return hval;
}

inline
uint16_t hashword2(const uint16_t first, const uint16_t second)
{
  return static_cast<uint16_t>(first%65449 + (second<<5)%65449);
}

/**
 * Call the 64-bit FNV hash function.
 */
inline
uint64_t hashword(const uint64_t * k, size_t length)
{
  return fnv_64_buf(k, 8*length);
}



/**
 * In a personal communication from Bob Jenkins, he recommended using
 * "Probably final_mix() from lookup3.c... You could hash up to 6 16-bit
 * integers that way.  The output is c, or the top or bottom 16 bits
 * of c if you only need 16 bit hash values." [JWP]
 */
inline
uint16_t hashword(const uint16_t * k, size_t length)
{
  // Three values that will be passed to final_mix() after they are initialized.
  uint32_t a = 0;
  uint32_t b = 0;
  uint32_t c = 0;

  switch (length)
    {
    case 3:
      {
        // Cast the inputs to 32 bit integers and call final_mix().
        a = k[0];
        b = k[1];
        c = k[2];
        break;
      }
    case 4:
      {
        // Combine the 4 16-bit inputs, "w, x, y, z" into two 32-bit
        // inputs "wx" and "yz" using bit operations and call final_mix.
        a = ((k[0]<<16) | (k[1] & 0xffff)); // wx
        b = ((k[2]<<16) | (k[3] & 0xffff)); // yz
        break;
      }
    default:
      libmesh_error_msg("Unsupported length: " << length);
    }

  // Result is returned in c
  final_mix(a,b,c);
  return static_cast<uint16_t>(c);
}


/**
 * Calls functions above with slightly more convenient
 * std::vector/array compatible interface.
 */
template <typename Container>
inline
typename Container::value_type hashword(const Container & keys)
{
  return hashword(keys.data(), keys.size());
}



} // end Utility namespace
} // end libMesh namespace

#endif // LIBMESH_HASHWORD_H
