// Copyright 2008, 2009, 2010, 2012, 2013, 2014 Martin C. Frith

// This struct maps characters to codes (small integers) and back.

// We allow for both "proper" letters (e.g. ACGT for DNA), which get
// the lowest codes, and "improper" letters.  This is because real
// sequence data includes additional letters (e.g. ambiguous bases),
// so we have to handle them.  In addition, the space character
// represents a special delimiter.

#ifndef ALPHABET_HH
#define ALPHABET_HH

#include "mcf_big_seq.hh"

#include <string>
#include <iosfwd>

namespace cbrc{

typedef unsigned char uchar;

struct Alphabet{
  typedef unsigned long long countT;

  static const char* dna;
  static const char* protein;
  static const char* proteinWithStop;

  static const unsigned capacity = 256;

  // Make an Alphabet from a string containing the "proper" letters.
  // If is4bit: map N, R, Y to codes < 16 (which only makes sense for
  // DNA, and should perhaps be done always regardless of is4bit).
  void init(const std::string &mainLetters, bool is4bit);

  // Make letters other than ACGTNRY map to the same thing as N
  void set4bitAmbiguities();

  // does this alphabet start with the standard protein alphabet?
  bool isProtein() const{ return letters.find( protein ) == 0; }

  // add counts of "proper" letters to "counts" (counting lowercase too)
  void count(const uchar *beg, const uchar *end, countT *counts) const {
    for (const uchar *i = beg; i < end; ++i) {
      unsigned u = numbersToUppercase[*i];
      if (u < size) ++counts[u];
    }
  }

  size_t countNormalLetters(const uchar *beg, const uchar *end) const {
    size_t c = 0;
    for (const uchar *i = beg; i < end; ++i) {
      c += (numbersToUppercase[*i] < size);
    }
    return c;
  }

  // translate (encode) a sequence of letters to numbers, in place
  void tr( uchar* beg, uchar* end, bool isKeepLowercase=true ) const;

  // reverse-translate (decode) a sequence of numbers to letters
  // return the position after the last written position in "out"
  char *rtCopy(char *out, mcf::BigSeq seq, size_t beg, size_t end) const {
    while (beg < end) *out++ = decode[seq[beg++]];
    return out;
  }

  void makeUppercase(uchar *beg, uchar *end) const {
    for (; beg < end; ++beg) *beg = numbersToUppercase[*beg];
  }

  std::string letters;    // the "proper" letters, e.g. ACGT for DNA
  unsigned size;          // same as letters.size(): excludes delimiters
  uchar encode[capacity];  // translate ASCII letters to codes (small integers)
  uchar decode[capacity];  // translate codes to ASCII letters
  uchar numbersToUppercase[capacity];  // translate codes to uppercase codes
  uchar numbersToLowercase[capacity];  // translate codes to lowercase codes
  uchar lettersToUppercase[capacity];  // translate letters to uppercase codes
  uchar complement[capacity];  // translate DNA codes to their complements

  void addLetters( const std::string& lettersToAdd, unsigned& code );
  void initCaseConversions( unsigned codeEnd );
  void makeComplement();
};

std::ostream& operator<<( std::ostream& s, const Alphabet& a );

}

#endif
