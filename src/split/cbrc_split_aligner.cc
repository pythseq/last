// Copyright 2012 Risa Kawaguchi
// Copyright 2013 Martin C. Frith

#include "cbrc_split_aligner.hh"
#include "cbrc_linalg.hh"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

static void err(const std::string& s) {
  throw std::runtime_error(s);
}

static long max(long a, long b, long c, long d) {
  return (std::max(std::max(a, b), std::max(c, d)));
}

namespace cbrc {

// Orders candidate alignments by increasing DP start coordinate:
struct QbegLess {
  QbegLess(const unsigned *b, const unsigned *r)
    : dpBegs(b), rnameAndStrandIds(r) {}

  bool operator()(unsigned a, unsigned b) const {
    return
      dpBegs[a] != dpBegs[b] ? dpBegs[a] < dpBegs[b] :
      rnameAndStrandIds[a] < rnameAndStrandIds[b];
  }

  const unsigned *dpBegs;
  const unsigned *rnameAndStrandIds;
};

// Orders candidate alignments by decreasing DP end coordinate:
struct QendLess {
  QendLess(const unsigned *e, const unsigned *r)
    : dpEnds(e), rnameAndStrandIds(r) {}

  bool operator()(unsigned a, unsigned b) const {
    return
      dpEnds[a] != dpEnds[b] ? dpEnds[a] > dpEnds[b] :
      rnameAndStrandIds[a] < rnameAndStrandIds[b];
  }

  const unsigned *dpEnds;
  const unsigned *rnameAndStrandIds;
};

// Orders candidate alignments by chromosome & strand:
struct RbegLess {
  RbegLess(const unsigned *r) : rnameAndStrandIds(r) {}

  bool operator()(unsigned a, unsigned b) const {
    return rnameAndStrandIds[a] < rnameAndStrandIds[b];
  }

  const unsigned *rnameAndStrandIds;
};

// Merges the elements of the sorted range [beg2,end2) into the sorted
// range [beg1,end1).  Assumes it's OK to add elements past end1.
static void mergeInto(unsigned* beg1,
		      unsigned* end1,
                      const unsigned* beg2,
                      const unsigned* end2,
                      const RbegLess& lessFunc) {
  unsigned* end3 = end1 + (end2 - beg2);
  for (;;) {
    if (beg2 == end2)
      break;
    if (beg1 == end1) {
      std::copy(beg2, end2, beg1);
      break;
    }
    --end3;
    if (lessFunc(*(end2-1), *(end1-1)))
      *end3 = *--end1;
    else
      *end3 = *--end2;
  }
}

int SplitAligner::score_mat[64][64][numQualCodes];

// The score for a cis-splice with the given distance (i.e. intron length)
int SplitAligner::spliceScore(double dist) const {
    double logDist = std::log(dist);
    double d = logDist - meanLogDist;
    double s = spliceTerm1 + spliceTerm2 * d * d - logDist;
    return std::floor(scale * s + 0.5);
}

// The dinucleotide immediately downstream of the given coordinate
unsigned SplitAligner::spliceBegSignal(unsigned coordinate,
				       char strand) const {
  if (strand == '+') {
    const uchar *genomeBeg = genome.seqReader();
    const uchar *p = genomeBeg + coordinate;
    unsigned n1 = alphabet.canonical[*p];
    if (n1 >= 4) return 16;
    unsigned n2 = alphabet.canonical[*(p + 1)];
    if (n2 >= 4) return 16;
    return n1 * 4 + n2;
  } else {
    const uchar *genomeEnd = genome.seqReader() + genome.finishedSize();
    const uchar *p = genomeEnd - coordinate;
    unsigned n1 = alphabet.canonical[*(p - 1)];
    if (n1 >= 4) return 16;
    unsigned n2 = alphabet.canonical[*(p - 2)];
    if (n2 >= 4) return 16;
    return 15 - (n1 * 4 + n2);  // reverse-complement
  }
}

// The dinucleotide immediately upstream of the given coordinate
unsigned SplitAligner::spliceEndSignal(unsigned coordinate,
				       char strand) const {
  if (strand == '+') {
    const uchar *genomeBeg = genome.seqReader();
    const uchar *p = genomeBeg + coordinate;
    unsigned n2 = alphabet.canonical[*(p - 1)];
    if (n2 >= 4) return 16;
    unsigned n1 = alphabet.canonical[*(p - 2)];
    if (n1 >= 4) return 16;
    return n1 * 4 + n2;
  } else {
    const uchar *genomeEnd = genome.seqReader() + genome.finishedSize();
    const uchar *p = genomeEnd - coordinate;
    unsigned n2 = alphabet.canonical[*p];
    if (n2 >= 4) return 16;
    unsigned n1 = alphabet.canonical[*(p + 1)];
    if (n1 >= 4) return 16;
    return 15 - (n1 * 4 + n2);  // reverse-complement
  }
}

// Score for starting a splice at position j in the i-th candidate alignment
int SplitAligner::spliceBegScore(unsigned i, unsigned j) const {
  if (chromosomeIndex.empty()) return 0;
  unsigned coordinate = cell(spliceBegCoords, i, j);
  char strand = alns[i].qstrand[0];
  return spliceBegScores[spliceBegSignal(coordinate, strand)];
}

double SplitAligner::spliceBegProb(unsigned i, unsigned j) const {
  if (chromosomeIndex.empty()) return 1;
  unsigned coordinate = cell(spliceBegCoords, i, j);
  char strand = alns[i].qstrand[0];
  return spliceBegProbs[spliceBegSignal(coordinate, strand)];
}

// Score for ending a splice at position j in the i-th candidate alignment
int SplitAligner::spliceEndScore(unsigned i, unsigned j) const {
  if (chromosomeIndex.empty()) return 0;
  unsigned coordinate = cell(spliceEndCoords, i, j);
  char strand = alns[i].qstrand[0];
  return spliceEndScores[spliceEndSignal(coordinate, strand)];
}

double SplitAligner::spliceEndProb(unsigned i, unsigned j) const {
  if (chromosomeIndex.empty()) return 1;
  unsigned coordinate = cell(spliceEndCoords, i, j);
  char strand = alns[i].qstrand[0];
  return spliceEndProbs[spliceEndSignal(coordinate, strand)];
}

unsigned SplitAligner::findScore(unsigned j, long score) const {
  for (unsigned i = 0; i < numAlns; ++i) {
    if (dpBeg(i) >= j || dpEnd(i) < j) continue;
    if (cell(Vmat, i, j) + spliceBegScore(i, j) == score) return i;
  }
  return numAlns;
}

unsigned SplitAligner::findSpliceScore(unsigned i, unsigned j,
				       long score) const {
    assert(splicePrior > 0.0);
    unsigned iSeq = rnameAndStrandIds[i];
    unsigned iEnd = cell(spliceEndCoords, i, j);
    int iScore = spliceEndScore(i, j);
    for (unsigned k = 0; k < numAlns; k++) {
	if (rnameAndStrandIds[k] != iSeq) continue;
	if (dpBeg(k) >= j || dpEnd(k) < j) continue;
	unsigned kBeg = cell(spliceBegCoords, k, j);
	if (iEnd <= kBeg) continue;
	int s = iScore + spliceBegScore(k, j) + spliceScore(iEnd - kBeg);
	if (cell(Vmat, k, j) + s == score) return k;
    }
    return numAlns;
}

long SplitAligner::scoreFromSplice(unsigned i, unsigned j,
				   unsigned oldNumInplay,
				   unsigned& oldInplayPos) const {
  if (splicePrior <= 0.0) return LONG_MIN;
  long score = LONG_MIN;
  unsigned iSeq = rnameAndStrandIds[i];
  unsigned iEnd = cell(spliceEndCoords, i, j);
  int iScore = spliceEndScore(i, j);

  for (/* noop */; oldInplayPos < oldNumInplay; ++oldInplayPos) {
    unsigned k = oldInplayAlnIndices[oldInplayPos];
    if (rnameAndStrandIds[k] >= iSeq) break;
  }

  for (unsigned y = oldInplayPos; y < oldNumInplay; ++y) {
    unsigned k = oldInplayAlnIndices[y];
    if (rnameAndStrandIds[k] > iSeq) break;
    unsigned kBeg = cell(spliceBegCoords, k, j);
    if (iEnd <= kBeg) continue;
    int s = iScore + spliceBegScore(k, j) + spliceScore(iEnd - kBeg);
    score = std::max(score, cell(Vmat, k, j) + s);
  }

  return score;
}

void SplitAligner::updateInplayAlnIndicesF(unsigned& sortedAlnPos,
					   unsigned& oldNumInplay,
                                           unsigned& newNumInplay,
                                           unsigned j) {  // query coordinate
  oldInplayAlnIndices.swap(newInplayAlnIndices);
  oldNumInplay = newNumInplay;
  newNumInplay = 0;

  for (unsigned x = 0; x < oldNumInplay; ++x) {
    unsigned i = oldInplayAlnIndices[x];
    if (dpEnd(i) == j) continue;  // it is no longer "in play"
    newInplayAlnIndices[newNumInplay++] = i;
  }

  unsigned sortedAlnOldPos = sortedAlnPos;

  for (/* noop */; sortedAlnPos < numAlns; ++sortedAlnPos) {
    unsigned i = sortedAlnIndices[sortedAlnPos];
    if (dpBeg(i) > j) break;  // it is not yet "in play"
    //newInplayAlnIndices[newNumInplay++] = i;
  }

  mergeInto(&newInplayAlnIndices[0],
            &newInplayAlnIndices[0] + newNumInplay,
            &sortedAlnIndices[0] + sortedAlnOldPos,
            &sortedAlnIndices[0] + sortedAlnPos,
            RbegLess(&rnameAndStrandIds[0]));

  newNumInplay += (sortedAlnPos - sortedAlnOldPos);
}

void SplitAligner::updateInplayAlnIndicesB(unsigned& sortedAlnPos,
					   unsigned& oldNumInplay,
                                           unsigned& newNumInplay,
                                           unsigned j) {  // query coordinate
  oldInplayAlnIndices.swap(newInplayAlnIndices);
  oldNumInplay = newNumInplay;
  newNumInplay = 0;

  for (unsigned x = 0; x < oldNumInplay; ++x) {
    unsigned i = oldInplayAlnIndices[x];
    if (dpBeg(i) == j) continue;  // it is no longer "in play"
    newInplayAlnIndices[newNumInplay++] = i;
  }

  unsigned sortedAlnOldPos = sortedAlnPos;

  for (/* noop */; sortedAlnPos < numAlns; ++sortedAlnPos) {
    unsigned i = sortedAlnIndices[sortedAlnPos];
    if (dpEnd(i) < j) break;  // it is not yet "in play"
    //newInplayAlnIndices[newNumInplay++] = i;
  }

  mergeInto(&newInplayAlnIndices[0],
            &newInplayAlnIndices[0] + newNumInplay,
            &sortedAlnIndices[0] + sortedAlnOldPos,
            &sortedAlnIndices[0] + sortedAlnPos,
            RbegLess(&rnameAndStrandIds[0]));

  newNumInplay += (sortedAlnPos - sortedAlnOldPos);
}

long SplitAligner::viterbi() {
    resizeMatrix(Vmat, 1);
    resizeVector(Vvec, 1);

    for (unsigned i = 0; i < numAlns; ++i) cell(Vmat, i, dpBeg(i)) = INT_MIN/2;
    cell(Vvec, minBeg) = INT_MIN/2;
    long scoreFromJump = INT_MIN/2;

    stable_sort(sortedAlnIndices.begin(), sortedAlnIndices.end(),
		QbegLess(&dpBegs[0], &rnameAndStrandIds[0]));
    unsigned sortedAlnPos = 0;
    unsigned oldNumInplay = 0;
    unsigned newNumInplay = 0;

    for (unsigned j = minBeg; j < maxEnd; j++) {
	updateInplayAlnIndicesF(sortedAlnPos, oldNumInplay, newNumInplay, j);
	unsigned oldInplayPos = 0;
	long sMax = INT_MIN/2;
	for (unsigned x = 0; x < newNumInplay; ++x) {
	    unsigned i = newInplayAlnIndices[x];
	    long s = max(JB(i, j),
			 scoreIndel(i, j),
			 scoreFromJump + spliceEndScore(i, j),
			 scoreFromSplice(i, j, oldNumInplay, oldInplayPos)) + cell(Amat, i, j);
	    cell(Vmat, i, j+1) = s;
	    sMax = std::max(sMax, s + spliceBegScore(i, j+1));
	}
	cell(Vvec, j+1) = std::max(sMax + restartScore, cell(Vvec, j));
	scoreFromJump = std::max(sMax + jumpScore, cell(Vvec, j+1));
    }

    return endScore();
}

long SplitAligner::endScore() const {
    long score = LONG_MIN;
    for (unsigned i = 0; i < numAlns; ++i)
	score = std::max(score, cell(Vmat, i, alns[i].qend));
    return score;
}

unsigned SplitAligner::findEndScore(long score) const {
    for (unsigned i = 0; i < numAlns; ++i)
        if (cell(Vmat, i, alns[i].qend) == score)
            return i;
    return numAlns;
}

void SplitAligner::traceBack(long viterbiScore,
			     std::vector<unsigned>& alnNums,
			     std::vector<unsigned>& queryBegs,
			     std::vector<unsigned>& queryEnds) const {
  unsigned i = findEndScore(viterbiScore);
  assert(i < numAlns);
  unsigned j = alns[i].qend;

  alnNums.push_back(i);
  queryEnds.push_back(j);

  for (;;) {
    long score = cell(Vmat, i, j);
    --j;
    score -= cell(Amat, i, j);
    if (score == JB(i, j)) {
	queryBegs.push_back(j);
        return;            
    } else if (score == scoreIndel(i, j)) {
	/* noop */;
    } else {
	queryBegs.push_back(j);
	long s = score - spliceEndScore(i, j);
	if (s == cell(Vvec, j)) {
	  while (s == cell(Vvec, j-1)) --j;
	  i = findScore(j, s - restartScore);
	} else {
	    unsigned k = findScore(j, s - jumpScore);
	    i = (k < numAlns) ? k : findSpliceScore(i, j, score);
	}
	assert(i < numAlns);
	alnNums.push_back(i);
	queryEnds.push_back(j);
    }
  }
}

int SplitAligner::segmentScore(unsigned alnNum,
			       unsigned queryBeg, unsigned queryEnd) const {
  int score = 0;
  unsigned i = alnNum;
  for (unsigned j = queryBeg; j < queryEnd; ++j) {
    score += cell(Amat, i, j);
    if (j > queryBeg) score += cell(Dmat, i, j);
  }
  return score;
}

double SplitAligner::probFromSpliceF(unsigned i, unsigned j,
				     unsigned oldNumInplay,
				     unsigned& oldInplayPos) const {
  if (splicePrior <= 0.0) return 0.0;
  double sum = 0.0;
  unsigned iSeq = rnameAndStrandIds[i];
  unsigned iEnd = cell(spliceEndCoords, i, j);
  double iProb = spliceEndProb(i, j);

  for (/* noop */; oldInplayPos < oldNumInplay; ++oldInplayPos) {
    unsigned k = oldInplayAlnIndices[oldInplayPos];
    if (rnameAndStrandIds[k] >= iSeq) break;
  }

  for (unsigned y = oldInplayPos; y < oldNumInplay; ++y) {
    unsigned k = oldInplayAlnIndices[y];
    if (rnameAndStrandIds[k] > iSeq) break;
    unsigned kBeg = cell(spliceBegCoords, k, j);
    if (iEnd <= kBeg) continue;
    double p = iProb * spliceBegProb(k, j) * spliceProb(iEnd - kBeg);
    sum += cell(Fmat, k, j) * p;
  }

  return sum;
}

double SplitAligner::probFromSpliceB(unsigned i, unsigned j,
				     unsigned oldNumInplay,
				     unsigned& oldInplayPos) const {
  if (splicePrior <= 0.0) return 0.0;
  double sum = 0.0;
  unsigned iSeq = rnameAndStrandIds[i];
  unsigned iBeg = cell(spliceBegCoords, i, j);
  double iProb = spliceBegProb(i, j);

  for (/* noop */; oldInplayPos < oldNumInplay; ++oldInplayPos) {
    unsigned k = oldInplayAlnIndices[oldInplayPos];
    if (rnameAndStrandIds[k] >= iSeq) break;
  }

  for (unsigned y = oldInplayPos; y < oldNumInplay; ++y) {
    unsigned k = oldInplayAlnIndices[y];
    if (rnameAndStrandIds[k] > iSeq) break;
    unsigned kEnd = cell(spliceEndCoords, k, j);
    if (kEnd <= iBeg) continue;
    double p = iProb * spliceEndProb(k, j) * spliceProb(kEnd - iBeg);
    sum += cell(Bmat, k, j) * p;
  }

  return sum;
}

void SplitAligner::forward() {
    resizeVector(rescales, 1);
    cell(rescales, minBeg) = 1.0;

    resizeMatrix(Fmat, 1);
    for (unsigned i = 0; i < numAlns; ++i) cell(Fmat, i, dpBeg(i)) = 0.0;
    double probFromRestart = 0.0;
    double probFromJump = 0.0;
    double begprob = 1.0;
    double zF = 0.0;  // sum of probabilities from the forward algorithm

    stable_sort(sortedAlnIndices.begin(), sortedAlnIndices.end(),
		QbegLess(&dpBegs[0], &rnameAndStrandIds[0]));
    unsigned sortedAlnPos = 0;
    unsigned oldNumInplay = 0;
    unsigned newNumInplay = 0;

    for (unsigned j = minBeg; j < maxEnd; j++) {
	updateInplayAlnIndicesF(sortedAlnPos, oldNumInplay, newNumInplay, j);
	unsigned oldInplayPos = 0;
	double r = cell(rescales, j);
	zF /= r;
	double pSum = 0.0;
	double rNew = 1.0;
	for (unsigned x = 0; x < newNumInplay; ++x) {
	    unsigned i = newInplayAlnIndices[x];
	    double p = (IB(i, j) * begprob +
			cell(Fmat, i, j) * cell(Dexp, i, j) +
			probFromJump * spliceEndProb(i, j) +
			probFromSpliceF(i, j, oldNumInplay, oldInplayPos)) * cell(Aexp, i, j) / r;
	    cell(Fmat, i, j+1) = p;
	    zF += IE(i, j+1) * p;
	    pSum += p * spliceBegProb(i, j+1);
	    rNew += p;
        }
        begprob /= r;
        cell(rescales, j+1) = rNew;
	probFromRestart = pSum * restartProb + probFromRestart / r;
	probFromJump = pSum * jumpProb + probFromRestart;
    }

    //zF /= cell(rescales, maxEnd);
    cell(rescales, maxEnd) = zF;  // this causes scaled zF to equal 1
}

void SplitAligner::backward() {
    resizeMatrix(Bmat, 1);
    for (unsigned i = 0; i < numAlns; ++i) cell(Bmat, i, dpEnd(i)) = 0.0;
    double probFromRestart = 0.0;
    double probFromJump = 0.0;
    double endprob = 1.0;
    //double zB = 0.0;  // sum of probabilities from the backward algorithm

    stable_sort(sortedAlnIndices.begin(), sortedAlnIndices.end(),
		QendLess(&dpEnds[0], &rnameAndStrandIds[0]));
    unsigned sortedAlnPos = 0;
    unsigned oldNumInplay = 0;
    unsigned newNumInplay = 0;

    for (unsigned j = maxEnd; j > minBeg; j--) {
	updateInplayAlnIndicesB(sortedAlnPos, oldNumInplay, newNumInplay, j);
	unsigned oldInplayPos = 0;
	double r = cell(rescales, j);
	//zB /= r;
	double pSum = 0.0;
	for (unsigned x = 0; x < newNumInplay; ++x) {
	    unsigned i = newInplayAlnIndices[x];
	    double p = (IE(i, j) * endprob +
			cell(Bmat, i, j) * cell(Dexp, i, j) +
			probFromJump * spliceBegProb(i, j) +
			probFromSpliceB(i, j, oldNumInplay, oldInplayPos)) * cell(Aexp, i, j-1) / r;
	    cell(Bmat, i, j-1) = p;
	    //zB += IB(i, j-1) * p;
	    pSum += p * spliceEndProb(i, j-1);
        }
        endprob /= r;
	probFromRestart = pSum * restartProb + probFromRestart / r;
	probFromJump = pSum * jumpProb + probFromRestart;
    }

    //zB /= cell(rescales, minBeg);
}

std::vector<double>
SplitAligner::marginalProbs(unsigned queryBeg, unsigned alnNum,
			    unsigned alnBeg, unsigned alnEnd) const {
    std::vector<double> output;
    unsigned i = alnNum;
    unsigned j = queryBeg;
    for (unsigned pos = alnBeg; pos < alnEnd; ++pos) {
        if (alns[i].qalign[pos] == '-') {
            double value = cell(Fmat, i, j) * cell(Bmat, i, j) * cell(Dexp, i, j) / cell(rescales, j);
            output.push_back(value);
        } else {
            double value = cell(Fmat, i, j+1) * cell(Bmat, i, j) / cell(Aexp, i, j);
            if (value != value) value = 0.0;
            output.push_back(value);
            j++;
        }
    }
    return output;
}

// The next 2 routines represent affine gap scores in a cunning way.
// Amat holds scores at query bases, and at every base that is aligned
// to a gap it gets a score of gapExistenceScore + gapExtensionScore.
// Dmat holds scores between query bases, and between every pair of
// bases that are both aligned to gaps it gets a score of
// -gapExistenceScore.  This produces suitable affine gap scores, even
// if we jump from one alignment to another in the middle of a gap.

void SplitAligner::calcBaseScores(unsigned i) {
  int firstGapScore = gapExistenceScore + gapExtensionScore;
  const UnsplitAlignment& a = alns[i];
  unsigned j = dpBeg(i);

  while (j < a.qstart) cell(Amat, i, j++) = firstGapScore;

  for (unsigned k = 0; j < a.qend; ++k) {
    char x = a.ralign[k];
    char y = a.qalign[k];
    int q = a.qQual.empty() ? numQualCodes - 1 : a.qQual[k] - qualityOffset;
    assert(q >= 0);
    if (y == '-') /* noop */;
    else if (x == '-') cell(Amat, i, j++) = firstGapScore;
    else cell(Amat, i, j++) = score_mat[x % 64][y % 64][q];
    // Amazingly, in ASCII, '.' equals 'n' mod 64.
    // So '.' will get the same scores as 'n'.
  }

  while (j < dpEnd(i)) cell(Amat, i, j++) = firstGapScore;
}

void SplitAligner::calcInsScores(unsigned i) {
  const UnsplitAlignment& a = alns[i];
  unsigned j = dpBeg(i);
  bool isExt = false;

  while (j < a.qstart) {
    cell(Dmat, i, j++) = (isExt ? -gapExistenceScore : 0);
    isExt = true;
  }

  for (unsigned k = 0; k < a.qalign.size(); ++k) {
    bool isDel = (a.qalign[k] == '-');
    bool isIns = (a.ralign[k] == '-');
    if (!isDel) cell(Dmat, i, j++) = (isIns && isExt ? -gapExistenceScore : 0);
    isExt = isIns;
  }

  while (j < dpEnd(i)) {
    cell(Dmat, i, j++) = (isExt ? -gapExistenceScore : 0);
    isExt = true;
  }

  cell(Dmat, i, j++) = 0;
}

void SplitAligner::calcDelScores(unsigned i) {
  const UnsplitAlignment& a = alns[i];
  unsigned j = a.qstart;
  int delScore = 0;
  for (unsigned k = 0; k < a.qalign.size(); ++k) {
    if (a.qalign[k] == '-') {  // deletion in query
      if (delScore == 0) delScore = gapExistenceScore;
      delScore += gapExtensionScore;
    } else {
      cell(Dmat, i, j++) += delScore;
      delScore = 0;
    }
  }
  cell(Dmat, i, j++) += delScore;
}

void SplitAligner::calcScoreMatrices() {
  resizeMatrix(Amat, 0);
  resizeMatrix(Dmat, 1);

  for (unsigned i = 0; i < numAlns; i++) {
    calcBaseScores(i);
    calcInsScores(i);
    calcDelScores(i);
  }
}

void SplitAligner::initSpliceCoords() {
  resizeMatrix(spliceBegCoords, 1);
  resizeMatrix(spliceEndCoords, 1);

  for (unsigned i = 0; i < numAlns; ++i) {
    const UnsplitAlignment& a = alns[i];
    unsigned j = dpBeg(i);
    unsigned k = a.rstart;

    if (!chromosomeIndex.empty()) {
      StringNumMap::const_iterator f = chromosomeIndex.find(a.rname);
      if (f == chromosomeIndex.end())
	err("can't find " + a.rname + " in the genome");
      unsigned c = f->second;
      if (a.qstrand == "+") k += genome.seqBeg(c);
      else                  k += genome.finishedSize() - genome.seqEnd(c);
    }

    cell(spliceBegCoords, i, j) = k;
    while (j < a.qstart) {
      cell(spliceEndCoords, i, j) = k;
      ++j;
      cell(spliceBegCoords, i, j) = k;
    }
    for (unsigned x = 0; x < a.ralign.size(); ++x) {
      if (a.qalign[x] != '-') cell(spliceEndCoords, i, j) = k;
      if (a.qalign[x] != '-') ++j;
      if (a.ralign[x] != '-') ++k;
      if (a.qalign[x] != '-') cell(spliceBegCoords, i, j) = k;
    }
    while (j < dpEnd(i)) {
      cell(spliceEndCoords, i, j) = k;
      ++j;
      cell(spliceBegCoords, i, j) = k;
    }
    cell(spliceEndCoords, i, j) = k;
  }
}

struct RnameAndStrandLess {
  RnameAndStrandLess(const UnsplitAlignment *a) : alns(a) {}

  bool operator()(unsigned a, unsigned b) const {
    return
      alns[a].qstrand != alns[b].qstrand ? alns[a].qstrand < alns[b].qstrand :
      alns[a].rname < alns[b].rname;
  }

  const UnsplitAlignment *alns;
};

void SplitAligner::initRnameAndStrandIds() {
  rnameAndStrandIds.resize(numAlns);
  RnameAndStrandLess less(&alns[0]);
  stable_sort(sortedAlnIndices.begin(), sortedAlnIndices.end(), less);
  unsigned c = 0;
  for (unsigned i = 0; i < numAlns; ++i) {
    unsigned k = sortedAlnIndices[i];
    if (i > 0 && less(sortedAlnIndices[i-1], k)) ++c;
    rnameAndStrandIds[k] = c;
  }
}

void SplitAligner::initForwardBackward() {
  resizeMatrix(Aexp, 0);
  resizeMatrix(Dexp, 1);

  for (unsigned i = 0; i < numAlns; ++i)
    for (unsigned j = dpBeg(i); j < dpEnd(i); ++j)
      cell(Aexp, i, j) = std::exp(cell(Amat, i, j) / scale);

  for (unsigned i = 0; i < numAlns; ++i)
    for (unsigned j = dpBeg(i); j <= dpEnd(i); ++j)
      cell(Dexp, i, j) = std::exp(cell(Dmat, i, j) / scale);

  // if x/scale < about -745, then exp(x/scale) will be exactly 0.0
}

void SplitAligner::initDpBounds() {
  minBeg = -1;
  for (unsigned i = 0; i < numAlns; ++i)
    minBeg = std::min(minBeg, alns[i].qstart);

  maxEnd = 0;
  for (unsigned i = 0; i < numAlns; ++i)
    maxEnd = std::max(maxEnd, alns[i].qend);

  dpBegs.resize(numAlns);
  dpEnds.resize(numAlns);

  bool isExtend = (jumpProb > 0.0 || splicePrior > 0.0);

  // If we are doing the repeated matches algorithm without any cis-
  // or trans-splicing, then we only need to do dynamic programming
  // along the length of each candidate alignment.  Otherwise, we need
  // to consider "end gaps" and extend the DP beyond the ends of each
  // candidate.  This extension is very inefficient for long query
  // sequences with many short candidate alignments: this is a real
  // problem, and there are probably ways to mitigate it...

  for (unsigned i = 0; i < numAlns; ++i) {
    dpBegs[i] = isExtend ? minBeg : alns[i].qstart;
    dpEnds[i] = isExtend ? maxEnd : alns[i].qend;
  }
}

void SplitAligner::initForOneQuery(std::vector<UnsplitAlignment>::const_iterator beg,
				   std::vector<UnsplitAlignment>::const_iterator end) {
    assert(end > beg);
    numAlns = end - beg;
    alns = beg;

    initDpBounds();
    calcScoreMatrices();

    sortedAlnIndices.resize(numAlns);
    for (unsigned i = 0; i < numAlns; ++i) sortedAlnIndices[i] = i;
    oldInplayAlnIndices.resize(numAlns);
    newInplayAlnIndices.resize(numAlns);

    if (splicePrior > 0.0 || !chromosomeIndex.empty()) {
        initSpliceCoords();
	//initRnameAndStrandIds();
    }
    initRnameAndStrandIds();

    initForwardBackward();
}

void SplitAligner::flipSpliceSignals() {
  Vmat.swap(VmatRev);
  Vvec.swap(VvecRev);
  Fmat.swap(FmatRev);
  Bmat.swap(BmatRev);
  rescales.swap(rescalesRev);

  for (int i = 0; i < 16; ++i) {
    int j = 15 - ((i%4) * 4 + (i/4));  // reverse-complement
    std::swap(spliceBegScores[i], spliceEndScores[j]);
    std::swap(spliceBegProbs[i], spliceEndProbs[j]);
  }
}

double SplitAligner::spliceSignalStrandProb() const {
  assert(rescales.size() == rescalesRev.size());
  double r = 1.0;
  for (unsigned j = 0; j < rescales.size(); ++j) {
    r *= rescalesRev[j] / rescales[j];
  }  // r might overflow to inf, but that should be OK
  return 1.0 / (1.0 + r);
}

// 1st 1 million reads from SRR359290.fastq:
// lastal -Q1 -e120 hg19/last/female-1111110m
// last-split-probs -s150 -b.01 splicePrior=0
// distance sample size: 41829
// distance quartiles: 312 1122 3310
// estimated mean ln[distance] 7.02287
// estimated standard deviation of ln[distance] 1.75073
// This log-normal fits the data pretty well, especially for longer
// introns, but it's a bit inaccurate for short introns.

// last-split-probs -s150 splicePrior=0.01 meanLogDist=7.0 sdevLogDist=1.75
// distance sample size: 46107
// distance quartiles: 316 1108 3228
// estimated mean ln[distance] 7.01031
// estimated standard deviation of ln[distance] 1.72269

void SplitAligner::setSpliceParams(double splicePriorIn,
				   double meanLogDistIn,
				   double sdevLogDistIn) {
  splicePrior = splicePriorIn;
  meanLogDist = meanLogDistIn;
  sdevLogDist = sdevLogDistIn;

  if (splicePrior <= 0.0) return;

  const double rootTwoPi = std::sqrt(8.0 * std::atan(1.0));
  spliceTerm1 = -std::log(sdevLogDist * rootTwoPi / splicePrior);
  spliceTerm2 = -1.0 / (2.0 * sdevLogDist * sdevLogDist);
}

void SplitAligner::setParams(int gapExistenceScoreIn, int gapExtensionScoreIn,
			     int jumpScoreIn, int restartScoreIn,
			     double scaleIn, int qualityOffsetIn) {
  gapExistenceScore = gapExistenceScoreIn;
  gapExtensionScore = gapExtensionScoreIn;
  jumpScore = jumpScoreIn;
  restartScore = restartScoreIn;
  scale = scaleIn;
  qualityOffset = qualityOffsetIn;
  jumpProb = std::exp(jumpScore / scale);
  restartProb = std::exp(restartScore / scale);
}

static int scoreFromProb(double prob, double scale) {
  return std::floor(scale * std::log(prob) + 0.5);
}

void SplitAligner::setSpliceSignals() {
  // If an RNA-DNA alignment reaches position i in the DNA, the
  // probability of splicing from i to j is:
  //   P(i & j)  =  d(i) * a(j) * f(j - i),
  // where:
  // d(i) and a(j) depend on the DNA sequences at i and j, e.g. GT-AG,
  // and:
  // f(j - i) is a probability density function, e.g. log-normal.
  // So: the sum over j of f(j - i)  =  1.
  // The probability of splicing from i to anywhere is:
  //   P(i)  =  d(i) * sum over j of [a(j) * f(j - i)]
  // So, a typical value of P(i) is: typical(d) * typical(a).

  // Here, we set the values of d(i) and a(j).
  // XXX We should allow the user to choose different values.
  // Only the relative values matter, because we will normalize them
  // (so that the overall splice probability is set by splicePrior).

  // The values for non-GT-AG signals are unnaturally high, to allow
  // for various kinds of error.

  double dGT = 0.95;
  double dGC = 0.02;
  double dAT = 0.004;
  double dNN = 0.002;

  double aAG = 0.968;
  double aAC = 0.004;
  double aNN = 0.002;

  // We assume the dinucleotides have roughly equal 1/16 abundances.

  double dAvg = (dGT + dGC + dAT + dNN * 13) / 16;
  double aAvg = (aAG + aAC + aNN * 14) / 16;

  for (int i = 0; i < 17; ++i) {
    spliceBegScores[i] = scoreFromProb(dNN / dAvg, scale);
    spliceEndScores[i] = scoreFromProb(aNN / aAvg, scale);
  }

  spliceBegScores[2 * 4 + 3] = scoreFromProb(dGT / dAvg, scale);
  spliceBegScores[2 * 4 + 1] = scoreFromProb(dGC / dAvg, scale);
  spliceBegScores[0 * 4 + 3] = scoreFromProb(dAT / dAvg, scale);

  spliceEndScores[0 * 4 + 2] = scoreFromProb(aAG / aAvg, scale);
  spliceEndScores[0 * 4 + 1] = scoreFromProb(aAC / aAvg, scale);

  for (int i = 0; i < 17; ++i) {
    spliceBegProbs[i] = std::exp(spliceBegScores[i] / scale);
    spliceEndProbs[i] = std::exp(spliceEndScores[i] / scale);
  }
}

void SplitAligner::printParameters() const {
  if (jumpProb > 0.0) {
    std::cout << "# trans=" << jumpScore << "\n";
  }

  if (!chromosomeIndex.empty()) {
    std::cout << "#"
	      << " GT=" << spliceBegScores[2 * 4 + 3]
	      << " GC=" << spliceBegScores[2 * 4 + 1]
	      << " AT=" << spliceBegScores[0 * 4 + 3]
	      << " NN=" << spliceBegScores[0 * 4 + 0]
	      << "\n";

    std::cout << "#"
	      << " AG=" << spliceEndScores[0 * 4 + 2]
	      << " AC=" << spliceEndScores[0 * 4 + 1]
	      << " NN=" << spliceEndScores[0 * 4 + 0]
	      << "\n";
  }
}

void SplitAligner::readGenome(const std::string& baseName) {
  std::string alphabetLetters;
  unsigned seqCount = -1;
  unsigned volumes = -1;

  std::string fileName = baseName + ".prj";
  std::ifstream f(fileName.c_str());
  if (!f) err("can't open file: " + fileName);

  std::string line, word;
  while (getline(f, line)) {
    std::istringstream iss(line);
    getline(iss, word, '=');
    if (word == "alphabet") iss >> alphabetLetters;
    if (word == "numofsequences") iss >> seqCount;
    if( word == "volumes" ) iss >> volumes;
  }

  if (alphabetLetters != "ACGT" || seqCount+1 == 0)
    err("can't read file: " + fileName);

  if (volumes+1 > 0 && volumes > 1)
    err("can't read multi-volume lastdb files, sorry");

  genome.fromFiles(baseName, seqCount, 0);

  for (unsigned i = 0; i < seqCount; ++i) {
    std::string n = genome.seqName(i);
    if (!chromosomeIndex.insert(std::make_pair(n, i)).second)
      err("duplicate sequence name: " + n);
  }

  alphabet.fromString(alphabetLetters);
}

static double probFromPhred(double s) {
  return std::pow(10.0, -0.1 * s);
}

static int generalizedScore(double score, double scale, double phredScore,
			    double letterProb) {
  double r = std::exp(score / scale);
  double p = probFromPhred(phredScore);
  if (p >= 1) p = 0.999999;  // kludge to avoid numerical instability
  double otherProb = 1 - letterProb;
  assert(otherProb > 0);
  double u = p / otherProb;
  double x = (1 - u) * r + u;
  assert(x > 0);
  return std::floor(scale * std::log(x) + 0.5);
}

static int min(const std::vector< std::vector<int> >& matrix) {
  int m = matrix.at(0).at(0);
  for (unsigned i = 0; i < matrix.size(); ++i)
    for (unsigned j = 0; j < matrix[i].size(); ++j)
      m = std::min(m, matrix[i][j]);
  return m;
}

static int matrixLookup(const std::vector< std::vector<int> >& matrix,
			const std::string& rowNames,
			const std::string& colNames, char x, char y) {
  std::string::size_type row = rowNames.find(x);
  std::string::size_type col = colNames.find(y);
  if (row == std::string::npos || col == std::string::npos)
    return min(matrix);
  else
    return matrix.at(row).at(col);
}

void SplitAligner::setScoreMat(const std::vector< std::vector<int> >& matrix,
			       const std::string& rowNames,
			       const std::string& colNames) {
  const std::string bases = "ACGT";

  // Reverse-engineer the abundances of ACGT from the score matrix:
  unsigned blen = bases.size();
  std::vector<double> bvec(blen * blen);
  std::vector<double*> bmat(blen);
  for (unsigned i = 0; i < blen; ++i) bmat[i] = &bvec[i * blen];
  for (unsigned i = 0; i < blen; ++i)
    for (unsigned j = 0; j < blen; ++j)
      bmat[i][j] = std::exp(matrixLookup(matrix, rowNames, colNames,
					 bases[i], bases[j]) / scale);
  std::vector<double> queryLetterProbs(blen, 1.0);
  linalgSolve(&bmat[0], &queryLetterProbs[0], blen);

  for (int i = 64; i < 128; ++i) {
    char x = std::toupper(i);
    for (int j = 64; j < 128; ++j) {
      char y = std::toupper(j);
      int score = matrixLookup(matrix, rowNames, colNames, x, y);
      for (int q = 0; q < numQualCodes; ++q) {
	std::string::size_type xc = bases.find(x);
	std::string::size_type yc = bases.find(y);
	if (xc == std::string::npos || yc == std::string::npos) {
	  score_mat[i % 64][j % 64][q] = score;
	} else {
	  double p = queryLetterProbs[yc];
	  score_mat[i % 64][j % 64][q] = generalizedScore(score, scale, q, p);
	}
      }
    }
  }
}

}