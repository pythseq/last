// Copyright 2008, 2009, 2010 Martin C. Frith

// This struct holds segment-pairs, and allows us to find
// (near-)overlaps between sets of segment-pairs efficiently.  To find
// overlaps, we use binary search, after sorting the segment-pairs by
// position.  To mark a segment-pair that is overlapped, we set its
// score to zero.

#ifndef SEGMENTPAIRPOT_HH
#define SEGMENTPAIRPOT_HH
#include "SegmentPair.hh"
#include <vector>

namespace cbrc{

struct SegmentPairPot{
  typedef SegmentPair::indexT indexT;
  typedef std::vector<SegmentPair>::iterator iterator;
  typedef std::vector<SegmentPair>::const_iterator const_iterator;

  // add a SegmentPair to the pot
  void add( const SegmentPair& sp ) { items.push_back(sp); }

  // the number of SegmentPairs in the pot
  std::size_t size() const { return items.size(); }

  // this must be called before using the following methods
  void sort();

  // get the i-th SegmentPair, sorted by score
  const SegmentPair& get( std::size_t i ) const { return *iters[i]; }

  // set the score of all items that overlap sp to zero
  void markOverlaps( const SegmentPair& sp );

  // set the score of all items that overlap anything in sps to zero
  void markAllOverlaps( const std::vector<SegmentPair>& sps );

  // mark (near-)tandem repeats within sp
  // to avoid death by dynamic programming when self-aligning a large sequence
  void markTandemRepeats( const SegmentPair& sp, indexT maxDistance );

  // data:
  std::vector<SegmentPair> items;
  std::vector<const_iterator> iters;

  // sort criterion for sorting by position
  static bool itemLess( const SegmentPair& x, const SegmentPair& y ){
    return x.diagonal() != y.diagonal() ? x.diagonal() < y.diagonal()
      : x.beg1() < y.beg1();
  }

  // sort criterion for sorting by score (in descending order)
  static bool iterLess( const const_iterator& x, const const_iterator& y ){
    // break ties in an arbitrary way, to make the results more reproducible:
    return (x->score  != y->score ) ? (x->score  > y->score )
      :    (x->start1 != y->start1) ? (x->start1 < y->start1)
      :                               (x->start2 < y->start2);
  }
};

}  // end namespace cbrc
#endif  // SEGMENTPAIRPOT_HH
