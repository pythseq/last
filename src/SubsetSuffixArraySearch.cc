// Copyright 2008, 2009, 2010, 2013, 2014 Martin C. Frith

#include "SubsetSuffixArray.hh"
//#include <iostream>  // for debugging

using namespace cbrc;

// use past results to speed up long matches?
// could & probably should return the match depth
void SubsetSuffixArray::match( const indexT*& begPtr, const indexT*& endPtr,
                               const uchar* queryPtr, const uchar* text,
                               indexT maxHits,
                               indexT minDepth, indexT maxDepth ) const{
  // the next line is unnecessary, but makes it faster in some cases:
  if( maxHits == 0 && minDepth < maxDepth ) minDepth = maxDepth;

  indexT depth = 0;
  const uchar* subsetMap = seed.firstMap();

  // match using buckets:
  indexT bucketDepth = maxBucketPrefix();
  indexT startDepth = std::min( bucketDepth, maxDepth );
  const indexT* bucketPtr = &buckets[0];

  while( depth < startDepth ){
    uchar subset = subsetMap[ queryPtr[depth] ];
    if( subset == CyclicSubsetSeed::DELIMITER ) break;
    ++depth;
    bucketPtr += subset * bucketSteps[depth];
    subsetMap = seed.nextMap( subsetMap );
  }

  indexT beg = *bucketPtr;
  indexT end = depth ? *(bucketPtr + bucketSteps[depth]) : suffixArray.size();

  while( depth > minDepth && end - beg < maxHits ){
    // maybe we lengthened the match too far: try shortening it again
    const uchar* oldMap = seed.prevMap( subsetMap );
    uchar subset = oldMap[ queryPtr[depth-1] ];
    bucketPtr -= subset * bucketSteps[depth];
    indexT oldBeg = *bucketPtr;
    indexT oldEnd = *(bucketPtr + bucketSteps[depth-1]);
    if( oldEnd - oldBeg > maxHits ) break;
    subsetMap = oldMap;
    beg = oldBeg;
    end = oldEnd;
    --depth;
  }

  // match using binary search:

  if( depth < minDepth ){
    indexT d = depth;
    const uchar* s = subsetMap;
    while( depth < minDepth ){
      uchar subset = subsetMap[ queryPtr[depth] ];
      if( subset == CyclicSubsetSeed::DELIMITER ){
	beg = end;
	break;
      }
      ++depth;
      subsetMap = seed.nextMap( subsetMap );
    }
    equalRange2( beg, end, queryPtr + d, queryPtr + depth, text + d, s );
  }

  while( end - beg > maxHits && depth < maxDepth ){
    uchar subset = subsetMap[ queryPtr[depth] ];
    if( subset == CyclicSubsetSeed::DELIMITER ){
      beg = end;
      break;
    }
    fastEqualRange( beg, end, text+depth, subsetMap, subset );
    ++depth;
    subsetMap = seed.nextMap( subsetMap );
  }

  begPtr = &suffixArray[0] + beg;
  endPtr = &suffixArray[0] + end;
}

void SubsetSuffixArray::countMatches( std::vector<unsigned long long>& counts,
				      const uchar* queryPtr,
				      const uchar* text,
				      indexT maxDepth ) const{
  indexT depth = 0;
  const uchar* subsetMap = seed.firstMap();

  // match using buckets:
  indexT bucketDepth = maxBucketPrefix();
  const indexT* bucketPtr = &buckets[0];
  indexT beg = 0;
  indexT end = suffixArray.size();

  while( depth < bucketDepth ){
    if( beg == end ) return;
    if( counts.size() <= depth ) counts.resize( depth+1 );
    counts[depth] += end - beg;
    if( depth >= maxDepth ) return;
    uchar subset = subsetMap[ queryPtr[depth] ];
    if( subset == CyclicSubsetSeed::DELIMITER ) return;
    ++depth;
    indexT step = bucketSteps[depth];
    bucketPtr += subset * step;
    beg = *bucketPtr;
    end = *(bucketPtr + step);
    subsetMap = seed.nextMap( subsetMap );
  }

  // match using binary search:
  while( beg < end ){
    if( counts.size() <= depth ) counts.resize( depth+1 );
    counts[depth] += end - beg;
    if( depth >= maxDepth ) return;
    uchar subset = subsetMap[ queryPtr[depth] ];
    if( subset == CyclicSubsetSeed::DELIMITER ) return;
    fastEqualRange( beg, end, text+depth, subsetMap, subset );
    ++depth;
    subsetMap = seed.nextMap( subsetMap );
  }
}

void SubsetSuffixArray::equalRange( indexT& beg, indexT& end,
				    const uchar* textBase,
				    const uchar* subsetMap,
				    uchar subset ) const{
  while( beg < end ){
    indexT mid = beg + (end - beg) / 2;
    uchar s = subsetMap[ textBase[ suffixArray[mid] ] ];
    if( s < subset ){
      beg = mid + 1;
    }else if( s > subset ){
      end = mid;
    }else{
      beg = lowerBound( beg, mid, textBase, subsetMap, subset );
      end = upperBound( mid + 1, end, textBase, subsetMap, subset );
      return;
    }
  }
}

SubsetSuffixArray::indexT
SubsetSuffixArray::lowerBound( indexT beg, indexT end, const uchar* textBase,
			       const uchar* subsetMap, uchar subset ) const{
  while( beg < end ){
    indexT mid = beg + (end - beg) / 2;
    if( subsetMap[ textBase[ suffixArray[mid] ] ] < subset ){
      beg = mid + 1;
    }else{
      end = mid;
    }
  }
  return beg;
}

SubsetSuffixArray::indexT
SubsetSuffixArray::upperBound( indexT beg, indexT end, const uchar* textBase,
			       const uchar* subsetMap, uchar subset ) const{
  while( beg < end ){
    indexT mid = beg + (end - beg) / 2;
    if( subsetMap[ textBase[ suffixArray[mid] ] ] <= subset ){
      beg = mid + 1;
    }else{
      end = mid;
    }
  }
  return end;
}

void SubsetSuffixArray::equalRange2( indexT& beg, indexT& end,
				     const uchar* queryBeg,
				     const uchar* queryEnd,
				     const uchar* textBase,
				     const uchar* subsetMap ) const{
  const uchar* qBeg = queryBeg;
  const uchar* qEnd = qBeg;
  const uchar* tBeg = textBase;
  const uchar* tEnd = tBeg;
  const uchar* sBeg = subsetMap;
  const uchar* sEnd = sBeg;

  while( beg < end ){
    indexT mid = beg + (end - beg) / 2;
    indexT offset = suffixArray[mid];
    const uchar* q;
    const uchar* t;
    const uchar* s;
    if( qBeg < qEnd ){
      q = qBeg;
      t = tBeg + offset;
      s = sBeg;
    }else{
      q = qEnd;
      t = tEnd + offset;
      s = sEnd;
    }
    uchar x, y;
    for( ; ; ){  // loop over consecutive letters
      x = s[ *t ];  // this text letter's subset
      y = s[ *q ];  // this query letter's subset
      if( x != y ) break;
      ++q;  // next query letter
      if( q == queryEnd ){  // we found a full match to [queryBeg, queryEnd)
	beg = lowerBound2( beg, mid, qBeg, queryEnd, tBeg, sBeg );
	end = upperBound2( mid + 1, end, qEnd, queryEnd, tEnd, sEnd );
	return;
      }
      ++t;  // next text letter
      s = seed.nextMap( s );  // next mapping from letters to subsets
    }
    if( x < y ){
      beg = mid + 1;
      // the next 3 lines are unnecessary, but make it faster:
      qBeg = q;
      tBeg = t - offset;
      sBeg = s;
    }else{
      end = mid;
      // the next 3 lines are unnecessary, but make it faster:
      qEnd = q;
      tEnd = t - offset;
      sEnd = s;
    }
  }
}

SubsetSuffixArray::indexT
SubsetSuffixArray::lowerBound2( indexT beg, indexT end,
				const uchar* queryBeg, const uchar* queryEnd,
				const uchar* textBase,
				const uchar* subsetMap ) const{
  while( beg < end ){
    indexT mid = beg + (end - beg) / 2;
    indexT offset = suffixArray[mid];
    const uchar* t = textBase + offset;
    const uchar* q = queryBeg;
    const uchar* s = subsetMap;
    for( ; ; ){  // loop over consecutive letters
      if( s[ *t ] < s[ *q ] ){
	beg = mid + 1;
	// the next 3 lines are unnecessary, but make it faster:
	queryBeg = q;
	textBase = t - offset;
	subsetMap = s;
	break;
      }
      ++q;  // next query letter
      if( q == queryEnd ){  // we found a full match to [queryBeg, queryEnd)
	end = mid;
	break;
      }
      ++t;  // next text letter
      s = seed.nextMap( s );  // next mapping from letters to subsets
    }
  }
  return beg;
}

SubsetSuffixArray::indexT
SubsetSuffixArray::upperBound2( indexT beg, indexT end,
				const uchar* queryBeg, const uchar* queryEnd,
				const uchar* textBase,
				const uchar* subsetMap ) const{
  while( beg < end ){
    indexT mid = beg + (end - beg) / 2;
    indexT offset = suffixArray[mid];
    const uchar* t = textBase + offset;
    const uchar* q = queryBeg;
    const uchar* s = subsetMap;
    for( ; ; ){  // loop over consecutive letters
      if( s[ *t ] > s[ *q ] ){
	end = mid;
	// the next 3 lines are unnecessary, but make it faster:
	queryBeg = q;
	textBase = t - offset;
	subsetMap = s;
        break;
      }
      ++q;  // next query letter
      if( q == queryEnd ){  // we found a full match to [queryBeg, queryEnd)
	beg = mid + 1;
	break;
      }
      ++t;  // next text letter
      s = seed.nextMap( s );  // next mapping from letters to subsets
    }
  }
  return end;
}
