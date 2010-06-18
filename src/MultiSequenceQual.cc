// Copyright 2009, 2010 Martin C. Frith

#include "MultiSequence.hh"
#include "stringify.hh"
#include <algorithm>  // max_element
#include <cctype>  // toupper
#include <limits>  // numeric_limits

// make C++ tolerable:
#define CI(type) std::vector<type>::const_iterator

#define ERR(x) throw std::runtime_error(x)

using namespace cbrc;

std::istream&
MultiSequence::appendFromFastq( std::istream& stream, indexT maxSeqLen ){
  const uchar padQualityScore = 0;  // dummy value: should never get used

  // initForAppending:
  if( qualityScores.empty() )
    qualityScores.insert( qualityScores.end(), padSize, padQualityScore );

  // reinitForAppending:
  if( qualityScores.size() > seq.v.size() )
    qualityScores.erase( qualityScores.begin(),
			 qualityScores.end() - seq.v.size() );

  if( isFinished() ){
    readFastaName(stream);
    if( !stream ) return stream;

    uchar c;

    // don't bother to obey maxSeqLen exactly: harmless for short sequences
    while( stream >> c && c != '+' ){  // skips whitespace
      seq.v.push_back(c);
    }

    stream.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

    while( qualityScores.size() < seq.v.size() && stream >> c ){  // skips WS
      qualityScores.push_back(c);
    }

    if( seq.v.size() != qualityScores.size() ) ERR( "bad FASTQ data" );
  }

  if( isFinishable(maxSeqLen) ){
    finish();
    qualityScores.insert( qualityScores.end(), padSize, padQualityScore );
  }

  return stream;
}

std::istream&
MultiSequence::appendFromPrb( std::istream& stream, indexT maxSeqLen,
			      unsigned alphSize, const uchar decode[] ){
  const uchar padQualityScore = 0;  // dummy value: should never get used
  std::size_t qualPadSize = padSize * alphSize;
  std::size_t qualSize = seq.v.size() * alphSize;

  // initForAppending:
  if( qualityScores.empty() )
    qualityScores.insert( qualityScores.end(), qualPadSize, padQualityScore );

  // reinitForAppending:
  if( qualityScores.size() > qualSize )
    qualityScores.erase( qualityScores.begin(),
			 qualityScores.end() - qualSize );

  if( isFinished() ){
    std::string line;
    getline( stream, line );  // slow but simple
    if( !stream ) return stream;

    // give the sequence a boring name:
    static std::size_t lineCount = 0;
    std::string name = stringify( ++lineCount );
    addName(name);

    std::istringstream iss(line);
    int q;
    while( iss >> q ){
      if( q < -64 || q > 62 )
	ERR( "quality score too large: " + stringify(q) );
      qualityScores.push_back( q + 64 );  // ASCII-encode the quality score
    }

    if( qualityScores.size() % alphSize != 0 ) ERR( "bad PRB data" );

    for( CI(uchar) i = qualityScores.begin() + qualSize;
	 i < qualityScores.end(); i += alphSize ){
      unsigned maxIndex = std::max_element( i, i + alphSize ) - i;
      seq.v.push_back( decode[ maxIndex ] );
    }
  }

  if( isFinishable(maxSeqLen) ){
    finish();
    qualityScores.insert( qualityScores.end(), qualPadSize, padQualityScore );
  }

  return stream;
}

std::istream& MultiSequence::readPssmHeader( std::istream& stream ){
  pssmColumnLetters.clear();

  // Look for a line with one letter per column of the PSSM:
  std::string line;
  while( getline( stream, line ) ){
    std::istringstream iss(line);
    std::string word;

    while( iss >> word ){
      if( word.size() == 1 ){
        uchar letter = std::toupper( word[0] );
        // allow for PSI-BLAST format, with repeated letters:
        if( pssmColumnLetters.size() && pssmColumnLetters[0] == letter ) break;
        pssmColumnLetters.push_back(letter);
      }else{
        pssmColumnLetters.clear();
        break;
      }
    }

    if( pssmColumnLetters.size() ) break;
  }

  if( !stream ) return stream;

  // give the PSSM a boring name:
  static std::size_t pssmCount = 0;
  std::string name = stringify( ++pssmCount );
  addName(name);

  return stream;
}

std::istream&
MultiSequence::appendFromPssm( std::istream& stream, indexT maxSeqLen,
                               const uchar* lettersToNumbers,
                               bool isMaskLowercase ){
  std::size_t pssmPadSize = padSize * scoreMatrixRowSize;
  std::size_t pssmSize = seq.v.size() * scoreMatrixRowSize;

  // initForAppending:
  if( pssm.empty() )
    pssm.insert( pssm.end(), pssmPadSize, -INF );

  // reinitForAppending:
  if( pssm.size() > pssmSize )
    pssm.erase( pssm.begin(), pssm.end() - pssmSize );

  if( isFinished() ){
    readPssmHeader(stream);
    if( !stream ) return stream;
  }

  while( seq.v.size() < maxSeqLen ){
    unsigned position;
    uchar letter;
    int score;
    std::vector<int> scores;
    stream >> position >> letter;
    while( scores.size() < pssmColumnLetters.size() && stream >> score ){
      scores.push_back(score);
    }
    if( !stream ) break;

    seq.v.push_back(letter);

    int minScore = *std::min_element( scores.begin(), scores.end() );
    pssm.insert( pssm.end(), scoreMatrixRowSize, minScore );
    std::vector<int>::iterator row = pssm.end() - scoreMatrixRowSize;
    for( unsigned i = 0; i < scores.size(); ++i ){
      uchar columnLetter = pssmColumnLetters[i];
      unsigned column = lettersToNumbers[columnLetter];
      if( column >= scoreMatrixRowSize )
        ERR( std::string("bad column-letter in PSSM: ") + char(columnLetter) );
      row[column] = scores[i];
      if( !isMaskLowercase ){
        unsigned column = lettersToNumbers[ std::tolower(columnLetter) ];
        if( column >= scoreMatrixRowSize ) continue;  // ?
        row[column] = scores[i];
      }
    }
    unsigned delimiterColumn = lettersToNumbers[' '];
    assert( delimiterColumn < scoreMatrixRowSize );
    row[delimiterColumn] = -INF;

    stream.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
  }

  if( isFinishable(maxSeqLen) ){
    finish();
    pssm.insert( pssm.end(), pssmPadSize, -INF );
  }

  if( !stream.bad() ) stream.clear();

  return stream;
}
