// Copyright 2013, 2014 Martin C. Frith

#include "last-split.hh"

#include "cbrc_split_aligner.hh"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>

class MyString {
public:
  MyString() : v(1), s(0), e(0) {}

  size_t size() const { return s; }

  char &operator[](size_t i) { return v[i]; }

  void resize(size_t i) {
    memmove(&v[0] + i, &v[0] + s, e - s);
    e -= s - i;
    s = i;
  }

  void erasePrefix(size_t len) {
    s -= len;
    e -= len;
    memmove(&v[0], &v[0] + len, e);
  }

  bool appendLine(std::istream &stream) {
    size_t i = s;
    for (;;) {
      char *x = static_cast<char *>(memchr(&v[0] + i, '\n', e - i));
      if (x) {
	*x = 0;
	s = x - &v[0] + 1;
	return true;
      }
      i = e;
      e += 256;  // xxx ???
      if (v.size() < e) v.resize(e);
      e = i + stream.rdbuf()->sgetn(&v[i], e - i);
      if (e == i) {
	if (i == s) return false;
	v[i] = 0;
	e = s = i + 1;
	return true;
      }
    }
  }

private:
  std::vector<char> v;
  size_t s;
  size_t e;
};

static void err(const std::string& s) {
  throw std::runtime_error(s);
}

static std::istream& openIn(const std::string& fileName, std::ifstream& ifs) {
  if (fileName == "-") return std::cin;
  ifs.open(fileName.c_str());
  if (!ifs) err("can't open file: " + fileName);
  return ifs;
}

// Does the string start with the prefix?
static bool startsWith(const char *s, const char *prefix) {
  for (;;) {
    if (*prefix == 0) return true;
    if (*prefix != *s) return false;
    ++s;
    ++prefix;
  }
}

// Does the string have no non-space characters?
static bool isBlankLine(const char *s) {
  for (;;) {
    if (*s == 0) return true;
    if (!std::isspace(*s)) return false;
    ++s;
  }
}

static bool isSpace(char c) {
  return c > 0 && c <= ' ';
}

static bool isSameName(const char *sLine1, const char *sLine2) {
  do { ++sLine1; } while (isSpace(*sLine1));
  do { ++sLine2; } while (isSpace(*sLine2));
  for (;;) {
    if (*sLine1 > ' ') {
      if (*sLine2 != *sLine1) return false;
    } else {
      return *sLine2 <= ' ';
    }
    ++sLine1;
    ++sLine2;
  }
}

static void transpose(std::vector< std::vector<int> > &matrix) {
  size_t rows = matrix.size();
  size_t cols = matrix[0].size();
  std::vector< std::vector<int> > m(cols);
  for (size_t i = 0; i < cols; ++i) {
    m[i].resize(rows);
    for (size_t j = 0; j < rows; ++j) {
      m[i][j] = matrix[j][i];
    }
  }
  m.swap(matrix);
}

// Defines an ordering, for sorting.
static bool less(const cbrc::UnsplitAlignment& a,
		 const cbrc::UnsplitAlignment& b) {
  int qnameCmp = strcmp(a.qname, b.qname);
  if (qnameCmp  != 0        ) return qnameCmp  < 0;
  if (a.qstart  != b.qstart ) return a.qstart  < b.qstart;
  if (a.qend    != b.qend   ) return a.qend    < b.qend;
  if (a.qstrand != b.qstrand) return a.qstrand < b.qstrand;
  int qalignCmp = strcmp(a.qalign, b.qalign);
  if (qalignCmp != 0        ) return qalignCmp < 0;
  int ralignCmp = strcmp(a.ralign, b.ralign);
  if (ralignCmp != 0        ) return ralignCmp < 0;
  return a.linesBeg < b.linesBeg;  // stabilizes the sort
}

static int printSense(char *out, double senseStrandLogOdds) {
  double b = senseStrandLogOdds / std::log(2.0);
  if (b < 0.1 && b > -0.1) b = 0;
  else if (b > 10) b = std::floor(b + 0.5);
  else if (b < -10) b = std::ceil(b - 0.5);
  int precision = (b < 10 && b > -10) ? 2 : 3;
  return sprintf(out, " sense=%.*g", precision, b);
}

static void doOneAlignmentPart(cbrc::SplitAligner &sa,
			       const LastSplitOptions &opts,
			       const cbrc::SplitAlignerParams &params,
			       bool isAlreadySplit,
			       const cbrc::UnsplitAlignment &a,
			       unsigned numOfParts, unsigned partNum,
			       unsigned alnNum,
			       unsigned qSliceBeg, unsigned qSliceEnd,
			       bool isSenseStrand, double senseStrandLogOdds) {
  std::vector<char> outputText;

  if (qSliceBeg >= a.qend || qSliceEnd <= a.qstart) {
    return;  // this can happen for spliced alignment!
  }

  unsigned qSliceBegTrimmed = qSliceBeg;
  unsigned qSliceEndTrimmed = qSliceEnd;
  unsigned alnBeg, alnEnd;
  cbrc::mafSliceBeg(a.ralign, a.qalign, a.qstart, qSliceBegTrimmed, alnBeg);
  cbrc::mafSliceEnd(a.ralign, a.qalign, a.qend,   qSliceEndTrimmed, alnEnd);

  if (qSliceBegTrimmed >= qSliceEndTrimmed) {
    return;  // I think this can happen for spliced alignment
  }

  int score =
    sa.segmentScore(alnNum, qSliceBeg, qSliceEnd) -
    sa.segmentScore(alnNum, qSliceBeg, qSliceBegTrimmed) -
    sa.segmentScore(alnNum, qSliceEndTrimmed, qSliceEnd);
  if (score < opts.score) return;

  std::vector<double> p;
  if (opts.direction != 0) {
    p = sa.marginalProbs(qSliceBegTrimmed, alnNum, alnBeg, alnEnd);
  }
  std::vector<double> pRev;
  if (opts.direction != 1) {
    sa.flipSpliceSignals(params);
    pRev = sa.marginalProbs(qSliceBegTrimmed, alnNum, alnBeg, alnEnd);
    sa.flipSpliceSignals(params);
  }
  if (opts.direction == 0) p.swap(pRev);
  if (opts.direction == 2) {
    double reverseProb = 1 / (1 + std::exp(senseStrandLogOdds));
    // the exp might overflow to inf, but that should be OK
    double forwardProb = 1 - reverseProb;
    for (unsigned i = 0; i < p.size(); ++i) {
      p[i] = forwardProb * p[i] + reverseProb * pRev[i];
    }
  }

  assert(!p.empty());
  double mismap = 1.0 - *std::max_element(p.begin(), p.end());
  mismap = std::max(mismap, 1e-10);
  if (mismap > opts.mismap) return;
  int mismapPrecision = 3;

  std::vector<char> slice;
  size_t lineLen = cbrc::mafSlice(slice, a, alnBeg, alnEnd, &p[0]);
  const char *sliceBeg = &slice[0];
  const char *sliceEnd = sliceBeg + slice.size();
  const char *pLine = sliceEnd - lineLen;
  const char *secondLastLine = pLine - lineLen;

  if (isAlreadySplit && secondLastLine[0] == 'p') {
    size_t backToSeq = alnEnd - alnBeg + 1;
    mismap = cbrc::pLinesToErrorProb(pLine - backToSeq, sliceEnd - backToSeq);
    if (mismap > opts.mismap) return;
    mismapPrecision = 2;
  }

  bool isLastalProbs = (*(secondLastLine - isAlreadySplit * lineLen) == 'p');
  if (opts.format == 'm' || (opts.format == 0 && !isLastalProbs)) {
    while (*(sliceEnd - lineLen) == 'p') sliceEnd -= lineLen;
  }

  const char *aLineOld = a.linesBeg[0];
  size_t aLineOldSize = a.linesBeg[1] - a.linesBeg[0] - 1;
  std::vector<char> aLine(aLineOldSize + 128);
  char *out = &aLine[0];
  if (opts.no_split && aLineOld[0] == 'a') {
    memcpy(out, aLineOld, aLineOldSize);
    out += aLineOldSize;
  } else {
    out += sprintf(out, "a score=%d", score);
  }
  out += sprintf(out, " mismap=%.*g", mismapPrecision, mismap);
  if (opts.direction == 2) out += printSense(out, senseStrandLogOdds);
  if (!opts.genome.empty() && !opts.no_split) {
    if (partNum > 0) {
      out = strcpy(out, isSenseStrand ? " acc=" : " don=") + 5;
      sa.spliceEndSignal(out, params, alnNum, qSliceBeg, isSenseStrand);
      out += 2;
    }
    if (partNum + 1 < numOfParts) {
      out = strcpy(out, isSenseStrand ? " don=" : " acc=") + 5;
      sa.spliceBegSignal(out, params, alnNum, qSliceEnd, isSenseStrand);
      out += 2;
    }
  }
  *out++ = '\n';

  outputText.insert(outputText.end(), &aLine[0], out);
  outputText.insert(outputText.end(), sliceBeg, sliceEnd);

  if (opts.no_split && a.linesEnd[-1][0] == 'c') {
    outputText.insert(outputText.end(), a.linesEnd[-1], a.linesEnd[0]);
    outputText.back() = '\n';
  }

  outputText.push_back('\n');

  std::cout.write(&outputText[0], outputText.size());
}

static void doOneQuery(cbrc::SplitAligner &sa, const LastSplitOptions &opts,
		       const cbrc::SplitAlignerParams &params,
		       bool isAlreadySplit,
		       const cbrc::UnsplitAlignment *beg,
		       const cbrc::UnsplitAlignment *end) {
  if (opts.verbose) std::cerr << beg->qname << "\t" << (end - beg);
  sa.layout(params, beg, end);
  if (opts.verbose) std::cerr << "\tcells=" << sa.cellsPerDpMatrix();
  size_t bytes = sa.memory(params, !opts.no_split, opts.direction == 2);
  if (bytes > opts.bytes) {
    if (opts.verbose) std::cerr << "\n";
    std::cerr << "last-split: skipping sequence " << beg->qname
	      << " (" << bytes << " bytes)\n";
    return;
  }
  sa.initMatricesForOneQuery(params);

  if (opts.direction != 0) {
    sa.forwardBackward(params);
  }
  if (opts.direction != 1) {
    sa.flipSpliceSignals(params);
    sa.forwardBackward(params);
    sa.flipSpliceSignals(params);
  }

  double senseStrandLogOdds =
    (opts.direction == 2) ? sa.spliceSignalStrandLogOdds() : 0;

  if (opts.no_split) {
    if (opts.verbose) std::cerr << "\n";
    unsigned numOfParts = end - beg;
    for (unsigned i = 0; i < numOfParts; ++i) {
      doOneAlignmentPart(sa, opts, params, isAlreadySplit, beg[i],
			 numOfParts, i, i, beg[i].qstart, beg[i].qend,
			 true, senseStrandLogOdds);
    }
  } else {
    long viterbiScore = LONG_MIN;
    if (opts.direction != 0) {
      viterbiScore = sa.viterbi(params);
      if (opts.verbose) std::cerr << "\t" << viterbiScore;
    }
    long viterbiScoreRev = LONG_MIN;
    if (opts.direction != 1) {
      sa.flipSpliceSignals(params);
      viterbiScoreRev = sa.viterbi(params);
      sa.flipSpliceSignals(params);
      if (opts.verbose) std::cerr << "\t" << viterbiScoreRev;
    }
    bool isSenseStrand = (viterbiScore >= viterbiScoreRev);
    std::vector<unsigned> alnNums;
    std::vector<unsigned> queryBegs;
    std::vector<unsigned> queryEnds;
    if (isSenseStrand) {
      sa.traceBack(params, viterbiScore, alnNums, queryBegs, queryEnds);
    } else {
      sa.flipSpliceSignals(params);
      sa.traceBack(params, viterbiScoreRev, alnNums, queryBegs, queryEnds);
      sa.flipSpliceSignals(params);
    }
    std::reverse(alnNums.begin(), alnNums.end());
    std::reverse(queryBegs.begin(), queryBegs.end());
    std::reverse(queryEnds.begin(), queryEnds.end());

    if (opts.verbose) std::cerr << "\n";
    unsigned numOfParts = alnNums.size();
    for (unsigned k = 0; k < numOfParts; ++k) {
      unsigned i = alnNums[k];
      doOneAlignmentPart(sa, opts, params, isAlreadySplit, beg[i],
			 numOfParts, k, i, queryBegs[k], queryEnds[k],
			 isSenseStrand, senseStrandLogOdds);
    }
  }
}

static void doOneBatch(MyString &inputText,
		       const std::vector<size_t> &lineEnds,
		       const std::vector<unsigned> &mafEnds,
		       cbrc::SplitAligner &sa, const LastSplitOptions &opts,
		       const cbrc::SplitAlignerParams &params,
		       bool isAlreadySplit) {
  std::vector<char *> linePtrs(lineEnds.size());
  for (size_t i = 0; i < lineEnds.size(); ++i) {
    linePtrs[i] = &inputText[0] + lineEnds[i];
  }

  std::vector<cbrc::UnsplitAlignment> mafs;
  mafs.reserve(mafEnds.size() - 1);  // saves memory: no excess capacity
  for (unsigned i = 1; i < mafEnds.size(); ++i)
    mafs.push_back(cbrc::UnsplitAlignment(&linePtrs[0] + mafEnds[i-1],
					  &linePtrs[0] + mafEnds[i],
					  opts.isTopSeqQuery));

  sort(mafs.begin(), mafs.end(), less);

  const cbrc::UnsplitAlignment *beg = mafs.empty() ? 0 : &mafs[0];
  const cbrc::UnsplitAlignment *end = beg + mafs.size();
  const cbrc::UnsplitAlignment *mid = beg;
  size_t qendMax = 0;

  while (mid < end) {
    if (mid->qend > qendMax) qendMax = mid->qend;
    ++mid;
    if (mid == end || strcmp(mid->qname, beg->qname) != 0 ||
	(mid->qstart >= qendMax && !opts.isSplicedAlignment)) {
      doOneQuery(sa, opts, params, isAlreadySplit, beg, mid);
      beg = mid;
      qendMax = 0;
    }
  }
}

static void addMaf(std::vector<unsigned> &mafEnds,
		   const std::vector<size_t> &lineEnds) {
  if (lineEnds.size() - 1 > mafEnds.back())  // if we have new maf lines:
    mafEnds.push_back(lineEnds.size() - 1);
}

static void eraseOldInput(MyString &inputText,
			  std::vector<size_t> &lineEnds,
			  std::vector<unsigned> &mafEnds) {
  size_t numOfOldLines = mafEnds.back();
  size_t numOfOldChars = lineEnds[numOfOldLines];
  inputText.erasePrefix(numOfOldChars);
  lineEnds.erase(lineEnds.begin(), lineEnds.begin() + numOfOldLines);
  for (size_t i = 0; i < lineEnds.size(); ++i) {
    lineEnds[i] -= numOfOldChars;
  }
  mafEnds.resize(1);
}

void lastSplit(LastSplitOptions& opts) {
  cbrc::SplitAlignerParams params;
  cbrc::SplitAligner sa;
  std::vector< std::vector<int> > scoreMatrix;
  std::string rowNames, colNames;
  std::string word, name, key;
  int state = 0;
  int sequenceFormat = -1;
  int gapExistenceCost = -1;
  int gapExtensionCost = -1;
  int insExistenceCost = -1;
  int insExtensionCost = -1;
  int lastalScoreThreshold = -1;
  double scale = 0;
  double genomeSize = 0;
  MyString inputText;
  std::vector<size_t> lineEnds(1);  // offsets in inputText of line starts/ends
  std::vector<unsigned> mafEnds(1);  // which lines are in which MAF block
  unsigned sLineCount = 0;
  size_t qNameLineBeg = 0;
  bool isAlreadySplit = false;  // has the input already undergone last-split?

  for (unsigned i = 0; i < opts.inputFileNames.size(); ++i) {
    std::ifstream inFileStream;
    std::istream& input = openIn(opts.inputFileNames[i], inFileStream);
    while (inputText.appendLine(input)) {
      const char *linePtr = &inputText[0] + lineEnds.back();
      if (state == -1) {  // we are reading the score matrix within the header
	std::istringstream ls(linePtr);
	std::vector<int> row;
	int score;
	ls >> word >> name;
	while (ls >> score) row.push_back(score);
	if (word == "#" && name.size() == 1 &&
	    row.size() == colNames.size() && ls.eof()) {
	  rowNames.push_back(std::toupper(name[0]));
	  scoreMatrix.push_back(row);
	} else {
	  state = 0;
	}
      }
      if (state == 0) {  // we are reading the header
	std::istringstream ls(linePtr);
	std::string names;
	ls >> word;
	while (ls >> name) {
	  if (name.size() == 1) names.push_back(std::toupper(name[0]));
	  else break;
	}
	if (word == "#" && !names.empty() && !ls && scoreMatrix.empty()) {
	  colNames = names;
	  state = -1;
	} else if (linePtr[0] == '#') {
	  std::istringstream ls(linePtr);
	  while (ls >> word) {
	    std::istringstream ws(word);
	    getline(ws, key, '=');
	    if (key == "a") ws >> gapExistenceCost;
	    if (key == "b") ws >> gapExtensionCost;
	    if (key == "A") ws >> insExistenceCost;
	    if (key == "B") ws >> insExtensionCost;
	    if (key == "e") ws >> lastalScoreThreshold;
	    if (key == "t") ws >> scale;
	    if (key == "Q") ws >> sequenceFormat;
	    if (key == "letters") ws >> genomeSize;
	  }
	  // try to determine if last-split was already run (fragile):
	  if (startsWith(linePtr, "# m=")) isAlreadySplit = true;
	} else if (!isBlankLine(linePtr)) {
	  if (scoreMatrix.empty())
	    err("I need a header with score parameters");
	  if (gapExistenceCost < 0 || gapExtensionCost < 0 ||
	      insExistenceCost < 0 || insExtensionCost < 0 ||
	      lastalScoreThreshold < 0 || scale <= 0 || genomeSize <= 0)
	    err("can't read the header");
	  if (sequenceFormat == 2 || sequenceFormat >= 4)
	    err("unsupported Q format");
	  opts.setUnspecifiedValues(lastalScoreThreshold, scale);
	  int restartCost =
	    opts.isSplicedAlignment ? -(INT_MIN/2) : opts.score - 1;
	  double jumpProb = opts.isSplicedAlignment
	    ? opts.trans / (2 * genomeSize)  // 2 strands
	    : 0.0;
	  int jumpCost = (jumpProb > 0.0) ?
	    -params.scoreFromProb(jumpProb, scale) : -(INT_MIN/2);
	  int qualityOffset =
            (sequenceFormat == 0) ? 0 : (sequenceFormat == 3) ? 64 : 33;
	  opts.print();
	  if (opts.isTopSeqQuery) {
	    transpose(scoreMatrix);
	    std::swap(rowNames, colNames);
	    std::swap(gapExistenceCost, insExistenceCost);
	    std::swap(gapExtensionCost, insExtensionCost);
	  }
	  params.setParams(-gapExistenceCost, -gapExtensionCost,
			   -insExistenceCost, -insExtensionCost,
			   -jumpCost, -restartCost, scale, qualityOffset);
	  double splicePrior = opts.isSplicedAlignment ? opts.cis : 0.0;
	  params.setSpliceParams(splicePrior, opts.mean, opts.sdev);
	  params.setScoreMat(scoreMatrix, rowNames.c_str(), colNames.c_str());
	  params.setSpliceSignals();
	  if (!opts.genome.empty()) params.readGenome(opts.genome);
	  params.print();
	  std::cout << "#\n";
	  state = 1;
	}
      }
      if (linePtr[0] == '#' && !startsWith(linePtr, "# batch")) {
	std::cout << linePtr << "\n";
      }
      if (state == 1) {  // we are reading alignments
	if (isBlankLine(linePtr)) {
	  addMaf(mafEnds, lineEnds);
	} else if (strchr(opts.no_split ? "asqpc" : "sqp", linePtr[0])) {
	  if (!opts.isTopSeqQuery && linePtr[0] == 's' && sLineCount++ % 2 &&
	      !isSameName(&inputText[qNameLineBeg], linePtr)) {
	    doOneBatch(inputText, lineEnds, mafEnds, sa, opts, params,
		       isAlreadySplit);
	    eraseOldInput(inputText, lineEnds, mafEnds);
	    qNameLineBeg = lineEnds.back();
	  }
	  lineEnds.push_back(inputText.size());
	}
      }
      inputText.resize(lineEnds.back());
    }
  }
  addMaf(mafEnds, lineEnds);
  doOneBatch(inputText, lineEnds, mafEnds, sa, opts, params, isAlreadySplit);
}
