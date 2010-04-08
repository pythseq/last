// Copyright 2008, 2009, 2010 Martin C. Frith

// This struct holds a gapped, pair-wise alignment.

#ifndef ALIGNMENT_HH
#define ALIGNMENT_HH
#include "Xdrop3FrameAligner.hh"
#include "SegmentPair.hh"
#include <string>
#include <vector>
#include <iosfwd>

namespace cbrc{

class GeneralizedAffineGapCosts;
class MultiSequence;
class Alphabet;
class Centroid;

struct Alignment{
  typedef SegmentPair::indexT indexT;
  typedef unsigned char uchar;

  enum { MAT = 64 };

  // make a single-block alignment:
  void fromSegmentPair( const SegmentPair& sp );

  // Make an Alignment by doing gapped X-drop extension in both
  // directions starting from a seed SegmentPair.  The resulting
  // Alignment might not be "optimal" (see below).
  // If outputType > 3: calculates match probabilities.
  // If outputType > 4: does gamma-centroid alignment.
  void makeXdrop( Xdrop3FrameAligner& aligner, Centroid& centroid,
		  const uchar* seq1, const uchar* seq2,
		  const int scoreMatrix[MAT][MAT], int smMax,
		  const GeneralizedAffineGapCosts& gap, int maxDrop,
		  int frameshiftCost, indexT frameSize,
		  const int pssm2[][MAT] = 0,
		  double gamma = 0, int outputType = 0 );

  // Check that the Alignment has no prefix with score <= 0, no suffix
  // with score <= 0, and no sub-segment with score < -maxDrop.
  // Alignments that pass this test may be non-optimal in other ways.
  bool isOptimal( const uchar* seq1, const uchar* seq2,
                  const int scoreMatrix[MAT][MAT], int maxDrop,
                  const GeneralizedAffineGapCosts& gap,
		  int frameshiftCost, indexT frameSize,
		  const int pssm2[][MAT] = 0 );

  void write( const MultiSequence& seq1, const MultiSequence& seq2,
	      char strand, bool isTranslated, const Alphabet& alph,
	      int format, std::ostream& os ) const;

  // data:
  std::vector<SegmentPair> blocks;  // the gapless blocks of the alignment
  int score;
  SegmentPair seed;  // the alignment remembers its seed
  std::vector<double> matchProbabilities;

  indexT beg1() const{ return blocks.front().beg1(); }
  indexT beg2() const{ return blocks.front().beg2(); }
  indexT end1() const{ return blocks.back().end1(); }
  indexT end2() const{ return blocks.back().end2(); }

  void extend( std::vector< SegmentPair >& chunks,
	       std::vector< double >& probs,
	       Xdrop3FrameAligner& aligner, Centroid& centroid,
	       const uchar* seq1, const uchar* seq2,
	       indexT start1, indexT start2,
	       XdropAligner::direction dir,
	       const int sm[MAT][MAT], int smMax, int maxDrop,
	       const GeneralizedAffineGapCosts& gap,
	       int frameshiftCost, indexT frameSize,
	       const int pssm2[][MAT],
	       double gamma, int outputType );

  void writeTab( const MultiSequence& seq1, const MultiSequence& seq2,
		 char strand, bool isTranslated, std::ostream& os ) const;

  void writeMaf( const MultiSequence& seq1, const MultiSequence& seq2,
		 char strand, bool isTranslated, const Alphabet& alph,
		 std::ostream& os ) const;

  std::string topString( const uchar* seq, const Alphabet& alph,
                         indexT frameSize ) const;

  std::string botString( const uchar* seq, const Alphabet& alph,
                         indexT frameSize ) const;

  std::string qualityString( const uchar* qualities,
			     std::size_t qualsPerBase ) const;

  static std::string qualityBlock( const uchar* qualities,
				   indexT beg, indexT end,
				   std::size_t qualsPerBase );
};

}  // end namespace cbrc
#endif  // ALIGNMENT_HH
