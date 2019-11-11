// Copyright 2011, 2012, 2013 Martin C. Frith

// The algorithm is based on these recurrence formulas, for
// generalized affine gap costs.  For standard affine gap costs, set
// gup=infinity.
//
// gop = gapExistenceCost
// gep = gapExtensionCost
// gup = gapUnalignedCost
//
// The 1st sequence: s(1), s(2), s(3), ...
// The 2nd sequence: t(1), t(2), t(3), ...
//
// matchScore(i, j)  =  the score for aligning s(i) with t(j).
//
// Initialization:
// x(i, 0)  =  y(i, 0)  =  z(i, 0)  =  -INF  (for all i >= 0)
// x(0, j)  =  y(0, j)  =  z(0, j)  =  -INF  (for all j >= 0)
// x(0, 0)  =  0
//
// Recurrence (i > 0 and j > 0):
// X(i, j)  =  x(i-1, j-1)
// Y(i, j)  =  max[ y(i-1, j) - gep, y(i-1, j-1) - gup ]
// Z(i, j)  =  max[ z(i, j-1) - gep, z(i-1, j-1) - gup ]
// b(i, j)  =  max[ X(i, j), Y(i, j), Z(i, j) ]
// x(i, j)  =  b(i, j) + matchScore(i, j)
// y(i, j)  =  max[ b(i, j) - gop, Y(i, j) ]
// z(i, j)  =  max[ b(i, j) - gop, Z(i, j) ]
//
// Interpretations:
// X(i, j)  =  the best score for any alignment ending with s(i-1)
//             aligned to t(j-1).
// b(i, j)  =  the best score for any alignment ending at s(i-1) and
//             t(j-1).
// x(i, j)  =  the best score for any alignment ending with s(i)
//             aligned to t(j).

// The recurrences are calculated antidiagonal-by-antidiagonal, where:
// antidiagonal  =  i + j

// We store x(i, j), y(i, j), and z(i, j) in the following way.
// xScores: oxx2x33x444x5555x66666...
// yScores: xxx2x33x444x5555x66666...
// zScores: xxx2x33x444x5555x66666...
// "o" indicates a cell with score = 0.
// "x" indicates a pad cell with score = -INF.
// "2", "3", etc. indicate cells in antidiagonal 2, 3, etc.

#include "GappedXdropAligner.hh"
#include "GappedXdropAlignerInl.hh"
//#include <iostream>  // for debugging

namespace cbrc {

// Puts 2 "dummy" antidiagonals at the start, so that we can safely
// look-back from subsequent antidiagonals.
void GappedXdropAligner::init() {
  scoreOrigins.resize(0);
  scoreEnds.resize(1);

  initAntidiagonal(0, 1);
  xScores[0] = 0;
  yScores[0] = -INF;
  zScores[0] = -INF;

  initAntidiagonal(0, 2);
  xScores[1] = -INF;
  yScores[1] = -INF;
  zScores[1] = -INF;

  bestAntidiagonal = 0;
}

int GappedXdropAligner::align(const uchar *seq1,
                              const uchar *seq2,
                              bool isForward,
			      int globality,
                              const ScoreMatrixRow *scorer,
                              int delExistenceCost,
                              int delExtensionCost,
                              int insExistenceCost,
                              int insExtensionCost,
                              int gapUnalignedCost,
                              int maxScoreDrop,
                              int maxMatchScore) {
  const int seqIncrement = isForward ? 1 : -1;
  const bool isAffine = isAffineGaps(delExistenceCost, delExtensionCost,
				     insExistenceCost, insExtensionCost,
				     gapUnalignedCost);

  size_t maxSeq1begs[] = { 0, 9 };
  size_t minSeq1ends[] = { 1, 0 };

  int bestScore = 0;
  int bestEdgeScore = -INF;
  size_t bestEdgeAntidiagonal = 0;

  init();
  seq1queue.clear();
  seq2queue.clear();

  for (size_t antidiagonal = 0; /* noop */; ++antidiagonal) {
    size_t seq1beg = std::min(maxSeq1begs[0], maxSeq1begs[1]);
    size_t seq1end = std::max(minSeq1ends[0], minSeq1ends[1]);

    if (seq1beg >= seq1end) break;

    size_t scoreEnd = scoreEnds.back();
    int numCells = seq1end - seq1beg;

    initAntidiagonal(seq1end, scoreEnd + numCells + 1);  // + 1 pad cell

    if (seq1end > seq1queue.size()) {
      seq1queue.push(scorer[*seq1], seq1beg);
      seq1 += seqIncrement;
    }
    const const_int_ptr *s1 = &seq1queue[seq1beg];

    size_t seq2pos = antidiagonal - seq1beg;
    if (seq2pos + 1 > seq2queue.size()) {
      seq2queue.push(*seq2, seq2pos - numCells + 1);
      seq2 += seqIncrement;
    }
    const uchar *s2 = &seq2queue[seq2pos];

    if (!globality && isDelimiter(*s2, *scorer))
      updateMaxScoreDrop(maxScoreDrop, numCells, maxMatchScore);

    int minScore = bestScore - maxScoreDrop;

    int *x0 = &xScores[scoreEnd];
    int *y0 = &yScores[scoreEnd];
    int *z0 = &zScores[scoreEnd];
    const int *y1 = &yScores[hori(antidiagonal, seq1beg)];
    const int *z1 = &zScores[vert(antidiagonal, seq1beg)];
    const int *x2 = &xScores[diag(antidiagonal, seq1beg)];

    *x0++ = *y0++ = *z0++ = -INF;  // add one pad cell

    if (globality && isDelimiter(*s2, *scorer)) {
      const int *z2 = &zScores[diag(antidiagonal, seq1beg)];
      int b = maxValue(x2[0], z1[0]-insExtensionCost, z2[0]-gapUnalignedCost);
      if (b >= minScore)
	updateBest1(bestEdgeScore, bestEdgeAntidiagonal, bestSeq1position,
		    b, antidiagonal, seq1beg);
    }

    if (isAffine) {
      for (int i = 0; i < numCells; ++i) {
	int x = x2[i];
	int y = y1[i] - delExtensionCost;
	int z = z1[i] - insExtensionCost;
	int b = maxValue(x, y, z);
	if (b >= minScore) {
	  if (b > bestScore) {
	    bestScore = b;
	    bestAntidiagonal = antidiagonal;
	  }
	  x0[i] = b + s1[0][*s2];
	  y0[i] = maxValue(b - delExistenceCost, y);
	  z0[i] = maxValue(b - insExistenceCost, z);
	}
	else x0[i] = y0[i] = z0[i] = -INF;
	++s1;
	--s2;
      }
    } else {
      const int *y2 = &yScores[diag(antidiagonal, seq1beg)];
      const int *z2 = &zScores[diag(antidiagonal, seq1beg)];
      for (int i = 0; i < numCells; ++i) {
        int x = x2[i];
        int y = maxValue(y1[i] - delExtensionCost, y2[i] - gapUnalignedCost);
        int z = maxValue(z1[i] - insExtensionCost, z2[i] - gapUnalignedCost);
        int b = maxValue(x, y, z);
        if (b >= minScore) {
	  if (b > bestScore) {
	    bestScore = b;
	    bestAntidiagonal = antidiagonal;
	  }
          x0[i] = b + s1[0][*s2];
          y0[i] = maxValue(b - delExistenceCost, y);
          z0[i] = maxValue(b - insExistenceCost, z);
        }
        else x0[i] = y0[i] = z0[i] = -INF;
	++s1;
	--s2;
      }
    }

    const int *seq1back = seq1queue[seq1end - 1];

    if (globality && isDelimiter(0, seq1back)) {
      const int *y2 = &yScores[diag(antidiagonal, seq1beg)];
      int n = numCells - 1;
      int b = maxValue(x2[n], y1[n]-delExtensionCost, y2[n]-gapUnalignedCost);
      if (b >= minScore)
	updateBest1(bestEdgeScore, bestEdgeAntidiagonal, bestSeq1position,
		    b, antidiagonal, seq1end-1);
    }

    if (!globality && isDelimiter(0, seq1back))
      updateMaxScoreDrop(maxScoreDrop, numCells, maxMatchScore);

    const int *x0base = x0 - seq1beg;
    updateFiniteEdges(maxSeq1begs, minSeq1ends, x0base, x0+numCells, numCells);
  }

  if (globality) {
    bestAntidiagonal = bestEdgeAntidiagonal;
    bestScore = bestEdgeScore;
  } else {
    calcBestSeq1position(bestScore);
  }
  return bestScore;
}

bool GappedXdropAligner::getNextChunk(size_t &end1,
                                      size_t &end2,
                                      size_t &length,
				      int delExistenceCost,
				      int delExtensionCost,
				      int insExistenceCost,
				      int insExtensionCost,
                                      int gapUnalignedCost) {
  if (bestAntidiagonal == 0) return false;

  end1 = bestSeq1position;
  end2 = bestAntidiagonal - bestSeq1position;
  const size_t undefined = -1;
  length = undefined;

  int state = 0;

  while (1) {
    assert(bestSeq1position <= bestAntidiagonal);

    size_t h = hori(bestAntidiagonal, bestSeq1position);
    size_t v = vert(bestAntidiagonal, bestSeq1position);
    size_t d = diag(bestAntidiagonal, bestSeq1position);

    int x = xScores[d];
    int y = yScores[h] - delExtensionCost;
    int z = zScores[v] - insExtensionCost;
    int a = yScores[d] - gapUnalignedCost;
    int b = zScores[d] - gapUnalignedCost;

    if (state == 1 || state == 3) {
      y += delExistenceCost;
      a += delExistenceCost;
    }

    if (state == 2 || state == 4) {
      z += insExistenceCost;
      b += insExistenceCost;
    }

    state = maxIndex(x, y, z, a, b);

    if (length == undefined && (state > 0 || bestAntidiagonal == 0)) {
      length = end1 - bestSeq1position;
      assert(length != undefined);
    }

    if (length != undefined && state == 0) return true;

    if (state < 1 || state > 2) bestAntidiagonal -= 2;
    else                        bestAntidiagonal -= 1;

    if (state != 2) bestSeq1position -= 1;
  }
}

}
