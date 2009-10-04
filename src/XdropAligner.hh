// Copyright 2008, 2009 Martin C. Frith

// X-drop algorithm for gapped extension of alignments.  We fill in
// the dynamic programming matrix antidiagonal-by-antidiagonal, as
// described in section 2 of: "A greedy algorithm for aligning DNA
// sequences", Z Zhang, S Schwartz, L Wagner, W Miller, J Comput
// Biol. 2000 7(1-2):203-14.  Unlike that description, we do not use
// "half nodes".

#ifndef XDROPALIGNER_HH
#define XDROPALIGNER_HH
#include <vector>
#include <cassert>
#include <climits>  // INT_MAX

namespace cbrc{

class GeneralizedAffineGapCosts;
class SegmentPair;

class XdropAligner{
  friend class Centroid;

public:
  typedef unsigned char uchar;

  enum { MAT = 64 };
  enum direction{ FORWARD, REVERSE };

  // Extend an alignment, from the given start point, in the given direction
  // Return the score (bestScore)
  int fill( const uchar* seq1, const uchar* seq2,
	    size_t start1, size_t start2, direction dir,
	    const int sm[MAT][MAT], int smMax, int maxDrop,
	    const GeneralizedAffineGapCosts& gap,
	    const int pssm2[][MAT] );

  // Get the ungapped segments of the extension, putting them in "chunks"
  // The extension might be empty!
  void traceback( std::vector< SegmentPair >& chunks,
		  const uchar* seq1, const uchar* seq2,
		  size_t start1, size_t start2, direction dir,
		  const int sm[MAT][MAT], const int pssm2[][MAT],
		  const GeneralizedAffineGapCosts& gap ) const;

  // Since fill() is one of the most speed-critical routines, we have
  // two (almost-identical) versions: one using an ordinary score
  // matrix, the other using a Position-Specific-Score-Matrix.

  int fill( const uchar* seq1, const uchar* seq2,
	    size_t start1, size_t start2, direction dir,
	    const int sm[MAT][MAT], int smMax, int maxDrop,
	    const GeneralizedAffineGapCosts& gap );

  int fillPssm( const uchar* seq1, const uchar* seq2,
		size_t start1, size_t start2, direction dir,
		const int sm[MAT][MAT], int smMax, int maxDrop,
		const GeneralizedAffineGapCosts& gap,
		const int pssm2[][MAT] );

protected:
  typedef std::vector< std::vector< int > > matrix_t;

  enum { INF = INT_MAX / 3 };  // big, but try to avoid overflow

  int bestScore;
  size_t bestAntiDiagonal;
  size_t bestPos1;

  matrix_t x;
  matrix_t y;
  matrix_t z;
  std::vector< size_t > offsets;

  static int drop( int score, int minScore );
  static bool isDelimiter( uchar c, const int sm[MAT][MAT] );
  static int match( const uchar* seq1, const uchar* seq2,
		    size_t start1, size_t start2, direction dir,
		    const int sm[MAT][MAT], const int pssm2[][MAT],
		    size_t antiDiagonal, size_t seq1pos );

  size_t fillBeg( size_t antiDiagonal ) const;
  size_t fillEnd( size_t antiDiagonal ) const;
  void reset( const GeneralizedAffineGapCosts& gap );
  void initScores( size_t antiDiagonal, size_t reservation );
  void updateBest( int score, size_t antiDiagonal, const int* x0 );
  size_t newFillBeg( size_t k1, size_t k2, size_t off1, size_t end1 ) const;
  size_t newFillEnd( size_t k1, size_t k2, size_t off1, size_t end1 ) const;

  // get a pointer into a sequence, taking start and direction into account
  template < typename T >
  static const T* seqPtr( const T* seq, size_t start,
			  direction dir, size_t pos ){
    //assert( pos > 0 );  // not always true for 3-frame alignment
    if( dir == FORWARD ) return &seq[ start + pos - 1 ];
    else                 return &seq[ start - pos ];
  }

  // mhamada: changed cell, hori, vert, diag to template methods

  // get DP matrix value at the given position, taking offsets into account
  template < typename T >
  T cell( const std::vector< std::vector< T > >& matrix,
	  size_t antiDiagonal, size_t seq1pos ) const{
    assert( seq1pos >= fillBeg( antiDiagonal ) );
    assert( seq1pos < fillEnd( antiDiagonal ) );
    return matrix[ antiDiagonal ][ seq1pos - offsets[ antiDiagonal ] ];
  }

  // get DP matrix value "left of" the given position
  template < typename T >
  T hori( const std::vector< std::vector< T > >& matrix,
	  size_t antiDiagonal, size_t seq1pos ) const{
    assert( antiDiagonal > 0 );
    if( seq1pos > fillBeg( antiDiagonal-1 ) ){
      return cell( matrix, antiDiagonal-1, seq1pos-1 );
    }
    else return -INF;
  }

  // get DP matrix value "above" the given position
  template < typename T >
  T vert( const std::vector< std::vector< T > >& matrix,
	  size_t antiDiagonal, size_t seq1pos ) const{
    assert( antiDiagonal > 0 );
    if( seq1pos < fillEnd( antiDiagonal-1 ) ){
      return cell( matrix, antiDiagonal-1, seq1pos );
    }
    else return -INF;
  }

  // get DP matrix value "diagonal from" the given position
  template < typename T >
  T diag( const std::vector< std::vector< T > >& matrix,
	  size_t antiDiagonal, size_t seq1pos ) const{
    if( antiDiagonal > 1 &&
	seq1pos > fillBeg( antiDiagonal-2 ) &&
	seq1pos <= fillEnd( antiDiagonal-2 ) ){
      return cell( matrix, antiDiagonal-2, seq1pos-1 );
    }
    else return -INF;
  }
};

// mhamada: changed these to templates:

template< typename T > T max3( T a, T b, T c ){
  return a > b ? std::max(a, c) : std::max(b, c);
}

template< typename T > T max4( T a, T b, T c, T d ){
  return a > b ? max3(a, c, d) : max3(b, c, d);
}

template< typename T > int maxIndex2( T a, T b ){
  return b > a ? 1 : 0;
}

template< typename T > int maxIndex3( T a, T b, T c ){
  return c > a ? maxIndex2( b, c ) + 1 : maxIndex2( a, b );
}

template< typename T > int maxIndex4( T a, T b, T c, T d ){
  return d > a ? maxIndex3( b, c, d ) + 1 : maxIndex3( a, b, c );
}

}  // end namespace cbrc
#endif  // XDROPALIGNER_HH
