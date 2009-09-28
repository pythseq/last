// Copyright 2008, 2009 Martin C. Frith

#include "LastalArguments.hh"
#include "stringify.hh"
#include <unistd.h>  // getopt
#include <sstream>
#include <vector>
#include <stdexcept>
#include <cstring>  // strtok
//#include <iostream>  // for debugging

static void badopt( char opt, const char* arg ){
  throw std::runtime_error( std::string("bad option value: -") +
			    opt + ' ' + arg );
}

namespace cbrc{

LastalArguments::LastalArguments() :
  outFile("-"),
  outputFormat(1),
  outputType(3),
  strand(-1),  // depends on the alphabet
  maskLowercase(0),
  minScoreGapped(-1),  // depends on the alphabet
  minScoreGapless(-1),  // depends on minScoreGapped
  matchScore(-1),  // depends on the alphabet
  mismatchCost(-1),  // depends on the alphabet
  gapExistCost(-1),  // depends on the alphabet
  gapExtendCost(-1),  // depends on the alphabet
  gapPairCost(100000),  // I want it to be infinity, but avoid overflow
  frameshiftCost(0),  // this means: ordinary, non-translated alignment
  matrixFile(""),
  maxDropGapped(-1),  // depends on gap costs & maxDropGapless
  maxDropGapless(-1),  // depends on the score matrix
  inputFormat(0),
  minHitDepth(1),
  oneHitMultiplicity(10),
  queryStep(1),
  batchSize(0),  // depends on the outputType
  maxRepeatDistance(1000),  // sufficiently conservative?
  temperature(-1),  // depends on the score matrix
  gamma(1),
  geneticCodeFile(""),
  verbosity(0){}

void LastalArguments::fromArgs( int argc, char** argv, bool optionsOnly ){
  std::string usage = "\
usage: lastal [options] lastdb-name fasta-sequence-file(s)\n\
\n\
Main options (default settings):\n\
-h: show all options and their default settings\n\
-o: output file\n\
-u: mask lowercase letters: 0=off, 1=softer, 2=soft, 3=hard ("
    + stringify(maskLowercase) + ")\n\
-s: strand: 0=reverse, 1=forward, 2=both (2 for DNA, 1 for protein)\n\
-f: output format: 0=tabular, 1=maf ("
    + stringify(outputFormat) + ")\n\
";

  std::string help = "\
usage: lastal [options] lastdb-name fasta-sequence-file(s)\n\
\n\
Main options (default settings):\n\
-h: show all options and their default settings\n\
-o: output file\n\
-u: mask lowercase letters: 0=off, 1=softer, 2=soft, 3=hard ("
    + stringify(maskLowercase) + ")\n\
-s: strand: 0=reverse, 1=forward, 2=both (2 for DNA, 1 for protein)\n\
-f: output format: 0=tabular, 1=maf ("
    + stringify(outputFormat) + ")\n\
\n\
Score parameters (default settings):\n\
-r: match score   (DNA: 1, protein: blosum62, Q>0:  6)\n\
-q: mismatch cost (DNA: 1, protein: blosum62, Q>0: 18)\n\
-p: file for residue pair scores\n\
-a: gap existence cost (DNA: 7, protein: 11, Q>0: 21)\n\
-b: gap extension cost (DNA: 1, protein:  2, Q>0:  9)\n\
-c: unaligned residue pair cost ("
    + stringify(gapPairCost) + ")\n\
-F: frameshift cost\n\
-x: maximum score dropoff for gapped extensions (max[y, a+b*20])\n\
-y: maximum score dropoff for gapless extensions (max-match-score * 10)\n\
-d: minimum score for gapless alignments (e*3/5)\n\
-e: minimum score for gapped alignments (DNA: 40, protein: 100, Q>0: 180)\n\
\n\
Miscellaneous options (default settings):\n\
-Q: input format: 0=FASTA, 1=FASTQ-Sanger, 2=FASTQ-Solexa, 3=PRB (0)\n\
-m: maximum multiplicity for initial matches ("
    + stringify(oneHitMultiplicity) + ")\n\
-l: minimum depth for initial matches ("
    + stringify(minHitDepth) + ")\n\
-k: step-size along the query sequence ("
    + stringify(queryStep) + ")\n\
-i: query batch size (16 MiB if j=0, else 1 MiB if Q>0, else 128 MiB)\n\
-w: supress repeats within this distance inside large exact matches ("
    + stringify(maxRepeatDistance) + ")\n\
-t: 'temperature' for calculating probabilities (1/lambda)\n\
-g: 'gamma' parameter for gamma-centroid alignment ("
    + stringify(gamma) + ")\n\
-G: genetic code file\n\
-v: be verbose: write messages about what lastal is doing\n\
-j: output type: 0=match counts, 1=gapless, 2=redundant gapped, 3=gapped,\n\
                 4=probabilities, 5=centroid ("
    + stringify(outputType) + ")\n\
";

  optind = 1;  // allows us to scan arguments more than once(???)
  int c;
  while( (c = getopt(argc, argv,
		     "ho:u:s:f:r:q:p:a:b:c:F:x:y:d:e:Q:m:l:k:i:w:t:g:G:vj:"))
	 != -1 ){
    switch(c){
    case 'h':
      throw std::runtime_error(help);
    case 'o':
      outFile = optarg;
      break;
    case 'u':
      unstringify( maskLowercase, optarg );  // 4 not supported yet
      if( maskLowercase < 0 || maskLowercase > 3 ) badopt( c, optarg );
      break;
    case 's':
      unstringify( strand, optarg );
      if( strand < 0 || strand > 2 ) badopt( c, optarg );
      break;
    case 'f':
      unstringify( outputFormat, optarg );
      if( outputFormat < 0 || outputFormat > 1 ) badopt( c, optarg );
      break;
    case 'r':
      unstringify( matchScore, optarg );
      if( matchScore <= 0 ) badopt( c, optarg );
      break;
    case 'q':
      unstringify( mismatchCost, optarg );
      if( mismatchCost < 0 ) badopt( c, optarg );  // allow 0 for Fujibuchi-san
      break;
    case 'p':
      matrixFile = optarg;
      break;
    case 'a':
      unstringify( gapExistCost, optarg );
      if( gapExistCost < 0 ) badopt( c, optarg );
      break;
    case 'b':
      unstringify( gapExtendCost, optarg );
      if( gapExtendCost <= 0 ) badopt( c, optarg );
      break;
    case 'c':
      unstringify( gapPairCost, optarg );
      if( gapPairCost <= 0 ) badopt( c, optarg );
      break;
    case 'F':
      unstringify( frameshiftCost, optarg );
      if( frameshiftCost <= 0 ) badopt( c, optarg );
      break;
    case 'x':
      unstringify( maxDropGapped, optarg );
      if( maxDropGapped < 0 ) badopt( c, optarg );
      break;
    case 'y':
      unstringify( maxDropGapless, optarg );
      if( maxDropGapless < 0 ) badopt( c, optarg );
      break;
    case 'd':
      unstringify( minScoreGapless, optarg );
      if( minScoreGapless < 0 ) badopt( c, optarg );
      break;
    case 'e':
      unstringify( minScoreGapped, optarg );
      if( minScoreGapped < 0 ) badopt( c, optarg );
      break;
    case 'Q':
      unstringify( inputFormat, optarg );
      if( inputFormat < 0 || inputFormat > 3 ) badopt( c, optarg );
      break;
    case 'm':
      unstringify( oneHitMultiplicity, optarg );
      break;
    case 'l':
      unstringify( minHitDepth, optarg );
      if( minHitDepth <= 0 ) badopt( c, optarg );
      break;
    case 'k':
      unstringify( queryStep, optarg );
      if( queryStep <= 0 ) badopt( c, optarg );
      break;
    case 'i':
      unstringifySize( batchSize, optarg );
      if( batchSize <= 0 ) badopt( c, optarg );  // 0 means "not specified"
      break;
    case 'w':
      unstringify( maxRepeatDistance, optarg );
      break;
    case 't':
      unstringify( temperature, optarg );
      if( temperature <= 0 ) badopt( c, optarg );
      break;
    case 'g':
      unstringify( gamma, optarg );
      if( gamma <= 0 ) badopt( c, optarg );
      break;
    case 'G':
      geneticCodeFile = optarg;
      break;
    case 'v':
      ++verbosity;
      break;
    case 'j':
      unstringify( outputType, optarg );
      if( outputType < 0 || outputType > 5 ) badopt( c, optarg );
      break;
    case '?':
      throw std::runtime_error("bad option");
    }
  }

  if( maskLowercase == 2 && inputFormat > 0 )
    throw std::runtime_error("can't combine option -u 2 with option -Q > 0");

  if( outputType > 3 && inputFormat > 0 )
    throw std::runtime_error("can't combine option -j > 3 with option -Q > 0");

  if( isTranslated() && inputFormat > 0 )
    throw std::runtime_error("can't combine option -F with option -Q > 0");

  if( isTranslated() && outputType > 3 )
    throw std::runtime_error("can't combine option -F with option -j > 3");

  if( isTranslated() && outputType == 0 )
    throw std::runtime_error("can't combine option -F with option -j 0");

  if( optionsOnly ) return;
  if( optind == argc ) throw std::runtime_error(usage);
  lastdbName = argv[optind++];
  inputStart = optind;
}

void LastalArguments::fromLine( const std::string& line, bool optionsOnly ){
  std::vector<char> args( line.begin(), line.end() );
  args.push_back(0);  // don't forget the NUL terminator!
  std::vector<char*> argv;
  char* i = std::strtok( &args[0], " \t" );
  argv.push_back(i);
  while( i != NULL ){
    i = std::strtok( NULL, " \t" );
    argv.push_back(i);
  }
  fromArgs( argv.size()-1, &argv[0], optionsOnly );
}

void LastalArguments::fromStream( std::istream& is, bool optionsOnly ){
  std::string trigger = "#lastal";
  for( std::string line; std::getline( is, line ); /* noop */ )
    if( line.compare( 0, trigger.size(), trigger ) == 0 )
      fromLine( line, optionsOnly );
}

void LastalArguments::fromString( const std::string& s, bool optionsOnly ){
  std::istringstream iss(s);
  fromStream( iss, optionsOnly );
}

void LastalArguments::setDefaultsFromAlphabet( bool isDna, bool isProtein ){
  if( strand < 0 ) strand = (isDna || isTranslated()) ? 2 : 1;

  if( isProtein ){
    // default match & mismatch scores: Blosum62 matrix
    if( matchScore < 0 && mismatchCost >= 0 ) matchScore   = 1;  // idiot-proof
    if( mismatchCost < 0 && matchScore >= 0 ) mismatchCost = 1;  // idiot-proof
    if( gapExistCost   < 0 ) gapExistCost   =  11;
    if( gapExtendCost  < 0 ) gapExtendCost  =   2;
    if( minScoreGapped < 0 ) minScoreGapped = 100;
  }
  else if( inputFormat == 0 ){
    if( matchScore     < 0 ) matchScore     =   1;
    if( mismatchCost   < 0 ) mismatchCost   =   1;
    if( gapExistCost   < 0 ) gapExistCost   =   7;
    if( gapExtendCost  < 0 ) gapExtendCost  =   1;
    if( minScoreGapped < 0 ) minScoreGapped =  40;
  }
  else{  // sequence quality scores will be used:
    if( matchScore     < 0 ) matchScore     =   6;
    if( mismatchCost   < 0 ) mismatchCost   =  18;
    if( gapExistCost   < 0 ) gapExistCost   =  21;
    if( gapExtendCost  < 0 ) gapExtendCost  =   9;
    if( minScoreGapped < 0 ) minScoreGapped = 180;
    // With this scoring scheme for DNA, gapless lambda ~= ln(10)/10,
    // so these scores should be comparable to PHRED scores.
    // Furthermore, since mismatchCost/matchScore = 3, the target
    // distribution of paired bases ~= 99% identity.  Because the
    // quality scores are unlikely to be perfect, it may be best to
    // use a lower target %identity than we otherwise would.
  }

  if( minScoreGapless < 0 ) minScoreGapless = minScoreGapped * 3 / 5;  // ?

  if( batchSize == 0 ){
    /**/ if( outputType == 0 ) batchSize = 0x1000000;  // 16 Mbytes
    else if( inputFormat > 0 ) batchSize = 0x100000;   // 1 Mbyte
    else                       batchSize = 0x8000000;  // 128 Mbytes
  }

  if( isTranslated() && frameshiftCost < gapExtendCost )
    throw std::runtime_error("the frameshift cost must not be less than the gap extension cost");
}

void LastalArguments::setDefaultsFromMatrix( int maxMatchScore,
					     double lambda ){
  if( maxDropGapless < 0 ) maxDropGapless = maxMatchScore * 10;

  if( maxDropGapped < 0 ){
    maxDropGapped = std::max( gapExistCost + gapExtendCost * 20,
			      maxDropGapless );
  }

  if( temperature < 0 ) temperature = 1 / lambda;
}

void LastalArguments::writeCommented( std::ostream& stream ) const{
  stream << "# "
	 << "a=" << gapExistCost << ' '
	 << "b=" << gapExtendCost << ' '
	 << "c=" << gapPairCost << ' '
	 << "F=" << frameshiftCost << ' '
	 << "e=" << minScoreGapped << ' '
	 << "d=" << minScoreGapless << ' '
	 << "x=" << maxDropGapped << ' '
	 << "y=" << maxDropGapless << '\n';

  stream << "# "
	 << "u=" << maskLowercase << ' '
	 << "s=" << strand << ' '
	 << "m=" << oneHitMultiplicity << ' '
	 << "l=" << minHitDepth << ' '
	 << "k=" << queryStep << ' '
	 << "i=" << batchSize << ' '
	 << "w=" << maxRepeatDistance << ' '
	 << "t=" << temperature << ' '
	 << "g=" << gamma << ' '
	 << "j=" << outputType << '\n';

  stream << "# " << lastdbName << '\n';
}

}  // end namespace cbrc
