// Copyright 2008 Martin C. Frith

#include "SegmentPairPot.hh"
#include <algorithm>
#include <cassert>

namespace cbrc{

void SegmentPairPot::sort(){
  assert( iters.empty() );  // could just clear it

  std::sort( items.begin(), items.end(), itemLess );

  for( const_iterator i = items.begin(); i < items.end(); ++i ){
    iters.push_back(i);
  }

  std::sort( iters.begin(), iters.end(), iterLess );
}

void SegmentPairPot::markOverlaps( const SegmentPair& sp ){
  iterator i = std::lower_bound( items.begin(), items.end(), sp, itemLess );

  if( i > items.begin() &&
      (i-1)->diagonal() == sp.diagonal() &&
      (i-1)->end1() > sp.beg1() ){
    (i-1)->score = 0;
  }

  while( i < items.end() &&
	 i->diagonal() == sp.diagonal() &&
	 i->beg1() < sp.end1() ){
    i->score = 0;
    ++i;
  }
}

void SegmentPairPot::markAllOverlaps( const std::vector<SegmentPair>& sps ){
  for( const_iterator i = sps.begin(); i < sps.end(); ++i ){
    markOverlaps( *i );
  }
}

void SegmentPairPot::markTandemRepeats( const SegmentPair& sp,
					indexT maxDistance ){
  assert( !items.empty() );

  iterator i = std::lower_bound( items.begin(), items.end(), sp, itemLess );
  if( i == items.end() )  i = items.begin();

  iterator j = i;
  do{  // funny loop to deal with wrap-around
    if( j->diagonal() - sp.diagonal() > maxDistance )  break;
    if( j->beg2() >= sp.beg2() && j->end1() <= sp.end1() )  j->score = 0;
    ++j;
    if( j == items.end() )  j = items.begin();
  }while( j != i );

  iterator k = i;
  do{  // funny loop to deal with wrap-around
    if( k == items.begin() )  k = items.end();
    --k;
    if( sp.diagonal() - k->diagonal() > maxDistance )  break;
    if( k->beg1() >= sp.beg1() && k->end2() <= sp.end2() )  k->score = 0;
  }while( k != i );
}

}  // end namespace cbrc
