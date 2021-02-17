// Copyright 2008, 2009, 2010, 2013, 2014 Martin C. Frith

#include "SubsetSuffixArray.hh"
//#include <iostream>  // for debugging

using namespace cbrc;

static size_t offGet(const OffPart *p) {
  size_t x = 0;
  for (int i = 0; i < offParts; ++i) {
    size_t y = p[i];  // must convert to size_t before shifting!
    x += y << (i * sizeof(OffPart) * CHAR_BIT);
  }
  return x;
}

// use past results to speed up long matches?
// could & probably should return the match depth
void SubsetSuffixArray::match(const PosPart *&begPtr, const PosPart *&endPtr,
			      const uchar *queryPtr, const uchar *text,
			      unsigned seedNum, size_t maxHits,
			      size_t minDepth, size_t maxDepth) const {
  // the next line is unnecessary, but makes it faster in some cases:
  if( maxHits == 0 && minDepth < maxDepth ) minDepth = maxDepth;

  size_t depth = 0;
  const CyclicSubsetSeed &seed = seeds[seedNum];
  const uchar* subsetMap = seed.firstMap();

  // match using buckets:
  size_t bucketDepth = maxBucketPrefix(seedNum);
  size_t startDepth = std::min( bucketDepth, maxDepth );
  const OffPart *bucketPtr = bucketEnds[seedNum];
  const indexT* myBucketSteps = bucketStepEnds[seedNum];

  while( depth < startDepth ){
    uchar subset = subsetMap[ queryPtr[depth] ];
    if( subset == CyclicSubsetSeed::DELIMITER ) break;
    ++depth;
    bucketPtr += subset * myBucketSteps[depth];
    subsetMap = seed.nextMap( subsetMap );
  }

  indexT beg = offGet(bucketPtr);
  indexT end = offGet(bucketPtr + myBucketSteps[depth]);

  while( depth > minDepth && end - beg < maxHits ){
    // maybe we lengthened the match too far: try shortening it again
    const uchar* oldMap = seed.prevMap( subsetMap );
    uchar subset = oldMap[ queryPtr[depth-1] ];
    bucketPtr -= subset * myBucketSteps[depth];
    indexT oldBeg = offGet(bucketPtr);
    indexT oldEnd = offGet(bucketPtr + myBucketSteps[depth-1]);
    if( oldEnd - oldBeg > maxHits ) break;
    subsetMap = oldMap;
    beg = oldBeg;
    end = oldEnd;
    --depth;
  }

  // match using binary search:

  if( depth < minDepth ){
    size_t d = depth;
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
    equalRange2( beg, end, queryPtr + d, queryPtr + depth, text + d, seed, s );
  }

  ChildDirection childDirection = UNKNOWN;

  while( end - beg > maxHits && depth < maxDepth ){
    uchar subset = subsetMap[ queryPtr[depth] ];
    if( subset == CyclicSubsetSeed::DELIMITER ){
      beg = end;
      break;
    }
    const uchar *textSubsetMap = seed.originalSubsetMap(subsetMap);
    childRange(beg, end, childDirection, text + depth, textSubsetMap, subset);
    ++depth;
    subsetMap = seed.nextMap( subsetMap );
  }

  begPtr = &suffixArray[0] + beg;
  endPtr = &suffixArray[0] + end;
}

void SubsetSuffixArray::countMatches(std::vector<unsigned long long> &counts,
				     const uchar *queryPtr, const uchar *text,
				     unsigned seedNum, size_t maxDepth) const {
  size_t depth = 0;
  const CyclicSubsetSeed &seed = seeds[seedNum];
  const uchar* subsetMap = seed.firstMap();

  // match using buckets:
  size_t bucketDepth = maxBucketPrefix(seedNum);
  const OffPart *bucketPtr = bucketEnds[seedNum];
  const indexT* myBucketSteps = bucketStepEnds[seedNum];
  indexT beg = offGet(bucketPtr);
  indexT end = offGet(bucketPtr + myBucketSteps[depth]);

  while( depth < bucketDepth ){
    if( beg == end ) return;
    if( counts.size() <= depth ) counts.resize( depth+1 );
    counts[depth] += end - beg;
    if( depth >= maxDepth ) return;
    uchar subset = subsetMap[ queryPtr[depth] ];
    if( subset == CyclicSubsetSeed::DELIMITER ) return;
    ++depth;
    indexT step = myBucketSteps[depth];
    bucketPtr += subset * step;
    beg = offGet(bucketPtr);
    end = offGet(bucketPtr + step);
    subsetMap = seed.nextMap( subsetMap );
  }

  // match using binary search:
  ChildDirection childDirection = UNKNOWN;

  while( beg < end ){
    if( counts.size() <= depth ) counts.resize( depth+1 );
    counts[depth] += end - beg;
    if( depth >= maxDepth ) return;
    uchar subset = subsetMap[ queryPtr[depth] ];
    if( subset == CyclicSubsetSeed::DELIMITER ) return;
    const uchar *textSubsetMap = seed.originalSubsetMap(subsetMap);
    childRange(beg, end, childDirection, text + depth, textSubsetMap, subset);
    ++depth;
    subsetMap = seed.nextMap( subsetMap );
  }
}

void SubsetSuffixArray::childRange( indexT& beg, indexT& end,
				    ChildDirection& childDirection,
				    const uchar* textBase,
				    const uchar* subsetMap,
				    uchar subset ) const{
  if( childDirection == UNKNOWN ){
    indexT mid = getChildForward( beg );
    if( mid == beg ){  // failure: never happens with the full childTable
      mid = getChildReverse( end );
      if( mid == end ){  // failure: never happens with the full childTable
	fastEqualRange( beg, end, textBase, subsetMap, subset );
	return;
      }
      childDirection = REVERSE;
    }else{
      childDirection = (mid < end) ? FORWARD : REVERSE;
    }
  }

  if( childDirection == FORWARD ){
    uchar e = subsetMap[ textBase[ suffixArray[ end - 1 ] ] ];
    if( subset > e ){ beg = end; return; }
    if( subset < e ) childDirection = REVERSE;  // flip it for next time
    while( 1 ){
      uchar b = subsetMap[ textBase[ suffixArray[ beg ] ] ];
      if( subset < b ) { end = beg; return; }
      if( b == e ) return;
      indexT mid = getChildForward( beg );
      if( mid == beg ){  // failure: never happens with the full childTable
	indexT offset = kiddyTable.empty() ? UCHAR_MAX : USHRT_MAX;
	equalRange( beg, end, textBase, subsetMap, subset, b, e, offset, 1 );
	return;
      }
      if( subset == b ) { end = mid; return; }
      beg = mid;
      if( b + 1 == e ) return;  // unnecessary, but may be faster
    }
  }else{
    uchar b = subsetMap[ textBase[ suffixArray[ beg ] ] ];
    if( subset < b ) { end = beg; return; }
    if( subset > b ) childDirection = FORWARD;  // flip it for next time
    while( 1 ){
      uchar e = subsetMap[ textBase[ suffixArray[ end - 1 ] ] ];
      if( subset > e ){ beg = end; return; }
      if( b == e ) return;
      indexT mid = getChildReverse( end );
      if( mid == end ){  // failure: never happens with the full childTable
	indexT offset = kiddyTable.empty() ? UCHAR_MAX : USHRT_MAX;
	equalRange( beg, end, textBase, subsetMap, subset, b, e, 1, offset );
	return;
      }
      if( subset == e ) { beg = mid; return; }
      end = mid;
      if( b + 1 == e ) return;  // unnecessary, but may be faster
    }
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
				     const CyclicSubsetSeed& seed,
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
      const uchar *textSubsetMap = seed.originalSubsetMap(s);
      x = textSubsetMap[ *t ];  // this text letter's subset
      y = s[ *q ];  // this query letter's subset
      if( x != y ) break;
      ++q;  // next query letter
      if( q == queryEnd ){  // we found a full match to [queryBeg, queryEnd)
	beg = lowerBound2( beg, mid, qBeg, queryEnd, tBeg, seed, sBeg );
	end = upperBound2( mid + 1, end, qEnd, queryEnd, tEnd, seed, sEnd );
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
				const CyclicSubsetSeed& seed,
				const uchar* subsetMap ) const{
  while( beg < end ){
    indexT mid = beg + (end - beg) / 2;
    indexT offset = suffixArray[mid];
    const uchar* t = textBase + offset;
    const uchar* q = queryBeg;
    const uchar* s = subsetMap;
    for( ; ; ){  // loop over consecutive letters
      const uchar *textSubsetMap = seed.originalSubsetMap(s);
      if( textSubsetMap[ *t ] < s[ *q ] ){
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
				const CyclicSubsetSeed& seed,
				const uchar* subsetMap ) const{
  while( beg < end ){
    indexT mid = beg + (end - beg) / 2;
    indexT offset = suffixArray[mid];
    const uchar* t = textBase + offset;
    const uchar* q = queryBeg;
    const uchar* s = subsetMap;
    for( ; ; ){  // loop over consecutive letters
      const uchar *textSubsetMap = seed.originalSubsetMap(s);
      if( textSubsetMap[ *t ] > s[ *q ] ){
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
