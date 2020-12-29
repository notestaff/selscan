/* norm -- a program for downstream analysis of iHS scores calculated by selscan
   Copyright (C) 2014  Zachary A Szpiech

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <iostream>
#include <fstream>
#include <map>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <cstring>
#include <fstream>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <utility>

#include <cstdio>
#include <thread>
#include <chrono>

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sort.h>

#include <boost/config.hpp>
#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
	using ::remove;
}
#endif

#include <boost/archive/tmpdir.hpp>
//#include <boost/archive/xml_iarchive.hpp>
//#include <boost/archive/xml_oarchive.hpp>

// include headers that implement a archive in simple text format
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/export.hpp>
#include <boost/range/numeric.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/foreach.hpp>
#include <boost/core/swap.hpp>

#define ForEach BOOST_FOREACH

#include "param_t.h"

//#define B_NVP(X) BOOST_SERIALIZATION_NVP(X)
#define B_NVP(X) X

//using namespace std;

const std::string VERSION = "1.3.0";

const std::string PREAMBLE = " -- a program for downstream analysis of selscan output\n\
Source code and binaries can be found at\n\
\t<https://www.github.com/szpiech/selscan>\n\
\n\
Citations:\n\
\n\
selscan: ZA Szpiech and RD Hernandez (2014) MBE, 31: 2824-2827.\n\
iHH12: R Torres et al. (2018) PLoS Genetics 15: e1007898.\n\
       N Garud et al. (2015) PLoS Genetics, 11: 1–32.\n\
nSL: A Ferrer-Admetlla, et al. (2014) MBE, 31: 1275-1291.\n\
XP-nSL: Szpiech et al. (2020) bioRxiv doi: \n\
        https://doi.org/10.1101/2020.05.19.104380.\n\
XP-EHH: PC Sabeti et al. (2007) Nature, 449: 913–918.\n\
iHS: BF Voight et al. (2006) PLoS Biology, 4: e72.\n\
\n\
To normalize selscan output across frequency bins:\n\
\n\
./norm [--ihs|--xpehh|--nsl|--xpnsl|--ihh12] --files <file1.*.out> ... <fileN.*.out>\n\
\n\
To normalize selscan output and analyze non-overlapping windows of fixed bp for \n\
extreme scores:\n\
\n\
./norm [--ihs|--xpehh|--nsl|--xpnsl|--ihh12] --files <file1.*.out> ... <fileN.*.out> --bp-win\n";

const std::string ARG_FREQ_BINS = "--bins";
const int DEFAULT_FREQ_BINS = 100;
const std::string HELP_FREQ_BINS = "The number of frequency bins in [0,1] for score normalization.";

const std::string ARG_FILES = "--files";
const std::string DEFAULT_FILES = "infile";
const std::string HELP_FILES = "A list of files delimited by whitespace for\n\
\tjoint normalization.\n\
\tExpected format for iHS or nSL files (no header):\n\
\t<locus name> <physical pos> <freq> <ihh1/sL1> <ihh2/sL2> <ihs/nsl>\n\
\tExpected format for XP-EHH files (one line header):\n\
\t<locus name> <physical pos> <genetic pos> <freq1> <ihh1> <freq2> <ihh2> <xpehh>\n\
\tExpected format for iHH12 files (one line header):\n\
\t<locus name> <physical pos> <freq1> <ihh12>";

const std::string ARG_LOG = "--log";
const std::string DEFAULT_LOG = "logfile";
const std::string HELP_LOG = "The log file name.";

const std::string ARG_SAVEBINS = "--save-bins";
const std::string DEFAULT_SAVEBINS = "";
const std::string HELP_SAVEBINS = "Save bins to this file.";

const std::string ARG_LOADBINS = "--load-bins";
const std::string DEFAULT_LOADBINS = "";
const std::string HELP_LOADBINS = "Load bins from this file.";

const std::string ARG_WINSIZE = "--winsize";
const int DEFAULT_WINSIZE = 100000;
const std::string HELP_WINSIZE = "The non-overlapping window size for calculating the percentage\n\
\tof extreme SNPs.";

const std::string ARG_QBINS = "--qbins";
const int DEFAULT_QBINS = 10;
const std::string HELP_QBINS = "Outlying windows are binned by number of sites within each\n\
\twindow.  This is the number of quantile bins to use.";

const std::string ARG_MINSNPS = "--min-snps";
const int DEFAULT_MINSNPS = 10;
const std::string HELP_MINSNPS = "Only consider a bp window if it has at least this many SNPs.";

const std::string ARG_SNPWIN = "--snp-win";
const bool DEFAULT_SNPWIN = false;
const std::string HELP_SNPWIN = "<not implemented> If set, will use windows of a constant\n\
\tSNP size with varying bp length.";

const std::string ARG_SNPWINSIZE = "--snp-win-size";
const int DEFAULT_SNPWINSIZE = 50;
const std::string HELP_SNPWINSIZE = "<not implemented> The number of SNPs in a window.";

const std::string ARG_BPWIN = "--bp-win";
const bool DEFAULT_BPWIN = false;
const std::string HELP_BPWIN = "If set, will use windows of a constant bp size with varying\n\
\tnumber of SNPs.";

const std::string ARG_IHS = "--ihs";
const bool DEFAULT_IHS = false;
const std::string HELP_IHS = "Do iHS normalization.";

const std::string ARG_NSL = "--nsl";
const bool DEFAULT_NSL = false;
const std::string HELP_NSL = "Do nSL normalization.";

const std::string ARG_XPEHH = "--xpehh";
const bool DEFAULT_XPEHH = false;
const std::string HELP_XPEHH = "Do XP-EHH normalization.";

const std::string ARG_XPEHH_FLIP_POPS = "--xpehh-flip-pops";
const bool DEFAULT_XPEHH_FLIP_POPS = false;
const std::string HELP_XPEHH_FLIP_POPS = "Flip core and ref pops for xp-ehh.";

const std::string ARG_XPNSL = "--xpnsl";
const bool DEFAULT_XPNSL = false;
const std::string HELP_XPNSL = "Do XP-nSL normalization.";

const std::string ARG_SOFT = "--ihh12";
const bool DEFAULT_SOFT = false;
const std::string HELP_SOFT = "Do ihh12 normalization.";

const std::string ARG_FIRST = "--first";
const bool DEFAULT_FIRST = false;
const std::string HELP_FIRST = "Output only the first file's normalization.";

const std::string ARG_CRIT_NUM = "--crit-val";
const double DEFAULT_CRIT_NUM = 2;
const std::string HELP_CRIT_NUM = "Set the critical value such that a SNP with |iHS| > CRIT_VAL is marked as an extreme SNP.  Default as in Voight et al.";

const std::string ARG_CRIT_PERCENT = "--crit-percent";
const double DEFAULT_CRIT_PERCENT = -1;
const std::string HELP_CRIT_PERCENT = "Set the critical value such that a SNP with iHS in the most extreme CRIT_PERCENT tails (two-tailed) is marked as an extreme SNP.\n\
\tNot used by default.";


const std::string ARG_ONLYSAVEBINS = "--only-save-bins";
const bool DEFAULT_ONLYSAVEBINS = false;
const std::string HELP_ONLYSAVEBINS = "only save bins info and then quit";

const int MISSING = -9999;

//returns number of lines in file
//throws 0 if the file fails
int checkIHSfile(std::ifstream &fin);
int checkXPEHHfile(std::ifstream &fin);
int checkIHH12file(std::ifstream &fin);

void readAllIHS(std::vector<std::string> filename, int fileLoci[], int nfiles, double freq[], double score[]);
void readAllXPEHH(std::vector<std::string> filename, int fileLoci[], int nfiles, double freq1[], double freq2[],
									double score[], bool flip_pops);
void readAllIHH12(std::vector<std::string> filename, int fileLoci[], int nfiles, double freq1[], double score[]);

void getMeanVarBins(double freq[], double data[], int nloci, double mean[], double variance[], int n[], int numBins, double threshold[]);

void normalizeIHSDataByBins(std::string &filename, std::string &outfilename, int &fileLoci, double mean[], double variance[], int n[], int numBins, double threshold[], double upperCutoff, double lowerCutoff);
void normalizeXPEHHDataByBins(std::string &filename, std::string &outfilename, int &fileLoci, double mean[], double variance[], int n[], int numBins, double threshold[], double upperCutoff, double lowerCutoff);
void normalizeIHH12DataByBins(std::string &filename, std::string &outfilename, int &fileLoci, double mean[], double variance[], int n[], int numBins, double threshold[], double upperCutoff, double lowerCutoff);

void saveIHSDataBins(std::string &binsOutfilename, double mean[], double variance[], int n[], int numBins, double threshold[],
										 double upperCutoff, double lowerCutoff);
void loadIHSDataBins(std::string &binsOutfilename, double mean[], double variance[], int n[], int &numBins, double threshold[],
										 double &upperCutoff, double &lowerCutoff);

void analyzeIHSBPWindows(std::string normedfiles[], int fileLoci[], int nfiles, int winSize, int numQuantiles, int minSNPs);
void analyzeXPEHHBPWindows(std::string normedfiles[], int fileLoci[], int nfiles, int winSize, int numQuantiles, int minSNPs);
void analyzeIHH12BPWindows(std::string normedfiles[], int fileLoci[], int nfiles, int winSize, int numQuantiles, int minSNPs);

int countCols(std::ifstream &fin);
int colsToSkip(std::ifstream &fin, int numCols);
void skipCols(std::ifstream &fin, int numCols);

int countFields(const std::string &str);
bool isint(std::string str);

std::ofstream flog;

typedef double *pdouble_t;
typedef int *pint_t;

template <typename T> inline
T *chk(T *ptr, const char *msg="alloc error") {
	if (!ptr) {
		std::cerr << "allocation error! " << msg << "\n";
		throw - 1;
	} else
		return ptr;
}

inline bool chk_cond(const bool cond, const char *msg="alloc error") {
	if (!cond) {
		std::cerr << "condition check failed! " << msg << "\n";
		throw - 1;
	} else
		return cond;
}

class BinsInfo {
private:
	bool _initialized;
	bool _ihs;
	bool _xpehh;
	bool _nsl;
	bool _ihh12;
	std::vector<double> _mean;
	std::vector<double> _variance;
	std::vector<int> _n;
	int _numBins;
	std::vector<double> _threshold;
	double _upperCutoff;
	double _lowerCutoff;

	friend class boost::serialization::access;
	// When the class Archive corresponds to an output archive, the
	// & operator is defined similar to <<.  Likewise, when the class Archive
	// is a type of input archive the & operator is defined similar to >>.
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & B_NVP(_initialized);
		ar & B_NVP(_ihs);
		ar & B_NVP(_xpehh);
		ar & B_NVP(_nsl);
		ar & B_NVP(_ihh12);

		ar & B_NVP(_mean);
		ar & B_NVP(_variance);
		ar & B_NVP(_n);
		ar & B_NVP(_numBins);
		ar & B_NVP(_threshold);
		ar & B_NVP(_upperCutoff);
		ar & B_NVP(_lowerCutoff);

		::chk_cond(_initialized);
		::chk_cond((_ihs + _xpehh + _nsl + _ihh12) == 1);
	}

	friend bool operator==(const BinsInfo& lhs, const BinsInfo& rhs);

	template <class T> 
	static void _replace_array(const std::vector<T>& _vec, T *& vec) {
		if (vec)
			delete [] vec;
		vec = chk(new T[_vec.size()]);
		std::copy(_vec.begin(), _vec.end(), vec);
	}

public:
	BinsInfo() { }
	BinsInfo(double mean[], double variance[], int n[], int numBins, double threshold[],
					 double upperCutoff, double lowerCutoff, bool ihs, bool xpehh, bool nsl, bool ihh12):
		_initialized(true), _ihs(ihs), _xpehh(xpehh), _nsl(nsl), _ihh12(ihh12),
		_mean(mean, mean+numBins), _variance(variance, variance+numBins), _n(n, n+numBins),
		_numBins(numBins), _threshold(threshold, threshold+numBins),
		_upperCutoff(upperCutoff), _lowerCutoff(lowerCutoff) {}	

	virtual ~BinsInfo() { }

	void set_bins_values(pdouble_t& mean, pdouble_t& variance, pint_t& n, int& numBins, pdouble_t& threshold,
											 double& upperCutoff,
											 double& lowerCutoff, bool ihs, bool xpehh, bool nsl, bool ihh12) const {
		chk_cond(_initialized);
		chk_cond(_ihs == ihs);
		chk_cond(_xpehh == xpehh);
		chk_cond(_nsl == nsl);
		chk_cond(_ihh12 == ihh12);
		_replace_array(_mean, mean);
		_replace_array(_variance, variance);
		_replace_array(_threshold, threshold);
		_replace_array(_n, n);

		numBins = _numBins;
		upperCutoff = _upperCutoff;
		lowerCutoff = _lowerCutoff;
	}

};

void print(std::string header, std::vector<double> const &input)
{
	std::cerr << header << "\n";
	for (auto it = input.cbegin(); it != input.cend(); it++)
		{
			std::cerr << *it << ' ';
		}
	std::cerr << "\n";
}

bool equal_or_nan(const std::vector<double>& lhs, const std::vector<double>& rhs) {
	if(lhs.size() != rhs.size()) return false;
	for(int i=0; i<lhs.size(); ++i)
		if(!((isnan(lhs[i]) && isnan(rhs[i])) || (lhs[i] == rhs[i])))
			return false;
	return true;
}

bool operator==(const BinsInfo& lhs, const BinsInfo& rhs)
{
	return 
		lhs._initialized && rhs._initialized &&
		lhs._ihs == rhs._ihs &&
		lhs._xpehh == rhs._xpehh &&
		lhs._nsl == rhs._nsl &&
		lhs._ihh12 == rhs._ihh12 &&
		equal_or_nan(lhs._mean, rhs._mean) && 
		equal_or_nan(lhs._variance, rhs._variance) && lhs._n == rhs._n && lhs._numBins == rhs._numBins &&
		equal_or_nan(lhs._threshold, rhs._threshold) && lhs._upperCutoff == rhs._upperCutoff && 
		lhs._lowerCutoff == rhs._lowerCutoff;
}
bool operator!=(const BinsInfo& lhs, const BinsInfo& rhs) { return !(lhs == rhs); }

BOOST_CLASS_EXPORT(BinsInfo);
BOOST_CLASS_VERSION(BinsInfo, 2);

std::string trim_whitespace(const std::string& str,
														const std::string& whitespace = " \t\n\r")
// adapted from https://stackoverflow.com/questions/1798112/removing-leading-and-trailing-spaces-from-a-string
{
	const auto strBegin = str.find_first_not_of(whitespace);
	if (strBegin == std::string::npos)
		return ""; // no content

	const auto strEnd = str.find_last_not_of(whitespace);
	const auto strRange = strEnd - strBegin + 1;

	return str.substr(strBegin, strRange);
}


/*
 * iterate through all the lines in file and
 * put them in given vector
 * from https://thispointer.com/c-how-to-read-a-file-line-by-line-into-a-vector/
 */
bool getFileContent(std::string fileName, std::vector<std::string> & vecOfStrs, bool trim_each_line=true)
{
	// Open the File
	std::ifstream in(fileName.c_str());
	// Check if object is valid
	if(!in)
		{
			std::cerr << "Cannot open the File : "<<fileName<<std::endl;
			return false;
		}
	std::string str;
	// Read the next line from File untill it reaches the end.
	while (std::getline(in, str))
		{
			// Line contains string of length > 0 then save it in vector
			if(str.size() > 0) {
				if (trim_each_line)
					str = trim_whitespace(str);
				vecOfStrs.push_back(str);
			}
		}
	//Close The File
	in.close();
	return true;
}

template <typename T> inline
double ToDouble(T x) { return x; }

//
// Class: SumKeeper
//
// Keeps a running sum without round-off errors.  Also keeps count of
// NaN and infinite values.
//
// Adapted from: http://code.activestate.com/recipes/393090/
//
template <typename ValT = double, typename CountT = size_t>
class SumKeeper {
public:
	 SumKeeper() { clear(); }

	 typedef ValT value_t;
	 typedef CountT count_t;

	 // Method: add
	 // Add a value to the sum keeper.
	 void add( ValT x ) {
		 if ( (boost::math::isnan)( ToDouble( x ) ) ) numNaNs++;
		 else if ( (boost::math::isinf)( ToDouble( x ) ) ) numInfs++;
		 else {
			 numVals++;
		
			 int i = 0;
			 for ( typename std::vector<ValT>::const_iterator yi = partials.begin(); yi != partials.end(); yi++ ) {
				 ValT y = *yi;

				 if ( ::fabs( ToDouble( x ) ) < ::fabs( ToDouble( y ) ) ) boost::swap( x, y );
				 ValT hi = x + y;
				 ValT lo = y - ( hi - x );
				 if ( ToDouble( lo ) != 0.0 ) partials[ i++ ] = lo;
				 x = hi;
			 }
			 partials.erase( partials.begin()+i, partials.end() );
			 partials.push_back( x );
		 }
	 }

	 // Method: add
	 // Add a range of values to this SumKeeper.
	 template <class ValRange>
	 void add( const ValRange& valRange ) { ForEach( ValT val, valRange ) add( valRange ); }

	 SumKeeper<ValT,CountT>& operator+=( ValT x ) { add( x ); return *this; }
	 SumKeeper<ValT,CountT>& operator+=( const SumKeeper<ValT,CountT>& sk  ) {
		 add( sk.getSum() );
		 numVals += sk.numVals;
		 numNaNs += sk.numNaNs;
		 numInfs += sk.numInfs;
		 return *this;
	 }

	 ValT getSum() const { return boost::accumulate( partials, ValT(0.0) ); }
	 CountT getNumVals() const { return numVals; }
	 CountT getNumNaNs() const { return numNaNs; }
	 CountT getNumInfs() const { return numInfs; }

   ValT getMean() const { return static_cast< ValT >( numVals > 0 ? ( ToDouble( getSum() ) / double(numVals) ) : std::numeric_limits<double>::quiet_NaN() ); }

	 void clear() {
		 partials.clear();
		 numVals = 0;
		 numNaNs = 0;
		 numInfs = 0;
	 }
  
private:
	 std::vector<ValT> partials;

	 CountT numVals;
	 CountT numNaNs;
	 CountT numInfs;
  
};  // class SumKeeper

//
// Class: StatKeeper
//
// Keeps a running sum and sum-of-squares without round-off errors,
// allowing accurate computation of the mean and stddev.
//
template <typename ValT = double, typename CountT = size_t>
class StatKeeper {
public:
	 StatKeeper() { clear(); }

   void add( ValT x ) { sum.add( x ); sumSq.add( static_cast< ValT >( ToDouble( x ) * ToDouble( x ) ) ); }

	 void clear() { sum.clear(); sumSq.clear(); }

	 ValT getSum() const { return sum.getSum(); }
	 CountT getNumVals() const { return sum.getNumVals(); }
	 CountT getNumNaNs() const { return sum.getNumNaNs(); }
	 CountT getNumInfs() const { return sum.getNumInfs(); }

	 // Method: add
	 // Add a range of values to this SumKeeper.
	 template <class ValRange>
	 void add( const ValRange& valRange ) { ForEach( ValT val, valRange ) add( val ); }

	 ValT getMean() const { return sum.getMean(); }
	 ValT getStd() const {
		 ValT meanSoFar = getMean();
		 return static_cast< ValT >( std::sqrt( ToDouble( sumSq.getMean() - ( static_cast< ValT >( ToDouble( meanSoFar ) * ToDouble( meanSoFar ) ) ) ) ) );
	 }
	 
private:
	 // Fields:
	 //
	 //   sum - sum of values passed to <add()>
	 //   sumSq - sum of squares of values passed to <add()>
	 SumKeeper<ValT,CountT> sum, sumSq;
};  // class StatKeeper


int main(int argc, char *argv[])
{
    std::cerr << "norm v" + VERSION + "\n";
    param_t params;
    params.setPreamble(PREAMBLE);
    params.addFlag(ARG_FREQ_BINS, DEFAULT_FREQ_BINS, "", HELP_FREQ_BINS);
    params.addListFlag(ARG_FILES, DEFAULT_FILES, "", HELP_FILES);
    params.addFlag(ARG_LOG, DEFAULT_LOG, "", HELP_LOG);
    params.addFlag(ARG_SAVEBINS, DEFAULT_SAVEBINS, "", HELP_SAVEBINS);
    params.addFlag(ARG_LOADBINS, DEFAULT_LOADBINS, "", HELP_LOADBINS);
    params.addFlag(ARG_WINSIZE, DEFAULT_WINSIZE, "", HELP_WINSIZE);
    params.addFlag(ARG_QBINS, DEFAULT_QBINS, "", HELP_QBINS);
    params.addFlag(ARG_MINSNPS, DEFAULT_MINSNPS, "", HELP_MINSNPS);
    params.addFlag(ARG_SNPWIN, DEFAULT_SNPWIN, "SILENT", HELP_SNPWIN);
    params.addFlag(ARG_SNPWINSIZE, DEFAULT_SNPWINSIZE, "SILENT", HELP_SNPWINSIZE);
    params.addFlag(ARG_BPWIN, DEFAULT_BPWIN, "", HELP_BPWIN);
    params.addFlag(ARG_FIRST, DEFAULT_FIRST, "", HELP_FIRST);
    params.addFlag(ARG_CRIT_NUM, DEFAULT_CRIT_NUM, "", HELP_CRIT_NUM);
    params.addFlag(ARG_CRIT_PERCENT, DEFAULT_CRIT_PERCENT, "", HELP_CRIT_PERCENT);
    params.addFlag(ARG_IHS, DEFAULT_IHS, "", HELP_IHS);
    params.addFlag(ARG_NSL, DEFAULT_NSL, "", HELP_NSL);
    params.addFlag(ARG_SOFT, DEFAULT_SOFT, "", HELP_SOFT);
    params.addFlag(ARG_XPEHH, DEFAULT_XPEHH, "", HELP_XPEHH);
    params.addFlag(ARG_XPEHH_FLIP_POPS, DEFAULT_XPEHH_FLIP_POPS, "", HELP_XPEHH_FLIP_POPS);
    params.addFlag(ARG_XPNSL, DEFAULT_XPNSL, "", HELP_XPNSL);
    params.addFlag(ARG_ONLYSAVEBINS, DEFAULT_ONLYSAVEBINS, "", HELP_ONLYSAVEBINS);


    try
    {
        params.parseCommandLine(argc, argv);
    }
    catch (...)
    {
        return 1;
    }

    int numBins = params.getIntFlag(ARG_FREQ_BINS);
    std::vector<std::string> filename_orig = params.getStringListFlag(ARG_FILES);
		std::vector<std::string> filename;
		{
			for (std::vector<std::string>::const_iterator it = filename_orig.begin(); it != filename_orig.end(); ++it) {
				if ( it->front() != '@')
					filename.push_back(*it);
				else {
					bool ok = getFileContent(it->substr(1), filename);
					if ( !ok ) {
						std::cerr << "Error reading file list: " << (*it) << "\n";
            throw - 1;
					}
				}
			}
		}
    int nfiles = filename.size();
    int winSize = params.getIntFlag(ARG_WINSIZE);
    std::string infoOutfile = params.getStringFlag(ARG_LOG);
    std::string binsOutfile = params.getStringFlag(ARG_SAVEBINS);
    std::string binsInfile = params.getStringFlag(ARG_LOADBINS);
    int numQBins = params.getIntFlag(ARG_QBINS);
    int minSNPs = params.getIntFlag(ARG_MINSNPS);
    int snpWinSize = params.getIntFlag(ARG_SNPWINSIZE);
    bool BPWIN = params.getBoolFlag(ARG_BPWIN);
    bool SNPWIN = params.getBoolFlag(ARG_SNPWIN);
    bool FIRST = params.getBoolFlag(ARG_FIRST);
    double critNum = params.getDoubleFlag(ARG_CRIT_NUM);
    double critPercent = params.getDoubleFlag(ARG_CRIT_PERCENT);
    bool IHS = params.getBoolFlag(ARG_IHS);
    bool NSL = params.getBoolFlag(ARG_NSL);
    bool SOFT = params.getBoolFlag(ARG_SOFT);
    bool XPEHH = params.getBoolFlag(ARG_XPEHH);
    bool XPNSL = params.getBoolFlag(ARG_XPNSL);
    bool ONLYSAVEBINS = params.getBoolFlag(ARG_ONLYSAVEBINS);
    bool XPEHH_FLIP_POPS = params.getBoolFlag(ARG_XPEHH_FLIP_POPS);

    if(XPNSL) XPEHH = true;

    if (numBins <= 0)
    {
        std::cerr << "ERROR: Must have a positive integer of frequency bins.\n";
        return 1;
    }

    if (numQBins <= 0)
    {
        std::cerr << "ERROR: Must have a positive integer of quantile bins.\n";
        return 1;
    }

    if (winSize <= 0)
    {
        std::cerr << "ERROR: Must have a positive integer window size.\n";
        return 1;
    }

    if (critNum <= 0)
    {
        std::cerr << "ERROR: Must give a positive critical value for |iHS| scores.\n";
        return 1;
    }

    if (critPercent != DEFAULT_CRIT_PERCENT && (critPercent <= 0 || critPercent >= 1))
    {
        std::cerr << "ERROR: Critical percentile must be in (0,1).\n";
        return 1;
    }

    
    if(IHS + XPEHH + NSL + SOFT!= 1){
        std::cerr << "Must specify exactly one of " + ARG_IHS + ", " + ARG_XPEHH + "," + ARG_NSL + "," + ARG_SOFT + "," + ARG_XPNSL + ".\n";
        return 1;
    }
    std::cerr << "You have provided " << nfiles << " output files for joint normalization.\n";

    std::string *outfilename = chk(new std::string[nfiles]);
    int *fileLoci = chk(new int[nfiles]);

    //std::ifstream* fin = new std::ifstream[nfiles];
    //ofstream* fout = new ofstream[nfiles];

    std::ifstream fin;

    int totalLoci = 0;

    //logging
    flog.open(infoOutfile.c_str());
    if (flog.fail())
    {
        std::cerr << "ERROR: " << infoOutfile << " " << strerror(errno) << std::endl;
        return 1;
    }

    for (int i = 0; i < argc; i++)
    {
        flog << argv[i] << " ";
    }
    flog << "\n\n";

    //flog << "Input files:\n";

    //For each file, open it, and check it for integrity
    //Also record total number of lines so we can allocate
    //enough space for the array of paired data that will
    //be used to calculate E[X] and E[X^2]
    for (int i = 0; i < nfiles; i++)
    {
        char str[10];
        sprintf(str, "%d", numBins);
        if(IHS || NSL) outfilename[i] = filename[i] + "." + str + "bins.norm";
        if(XPEHH || SOFT) outfilename[i] = filename[i] + ".norm";
        
        fin.open(filename[i].c_str());
        if (fin.fail())
        {
            std::cerr << "ERROR: " << infoOutfile << " " << strerror(errno);
            flog << "ERROR: " << infoOutfile << " " << strerror(errno);
            return 1;
        }
        else
        {
            std::cerr << "Opened " << filename[i] << std::endl;
            //flog << filename[i] << std::endl;
        }

        //check integrity of file and keep count of the number of lines
        try
        {
            if (IHS || NSL) fileLoci[i] = checkIHSfile(fin);
            if (SOFT) fileLoci[i] = checkIHH12file(fin);
            if (XPEHH) fileLoci[i] = checkXPEHHfile(fin);
            totalLoci += fileLoci[i];
        }
        catch (...)
        {
            return 1;
        }
        fin.close();
    }

    std::cerr << "\nTotal loci: " << totalLoci << std::endl;
    flog << "\nTotal loci: " << totalLoci << std::endl;

    if (IHS || NSL)
    {
        std::cerr << "Reading all data.\n";
        double *freq = chk(new double[totalLoci]);
        double *score = chk(new double[totalLoci]);
        //read in all data
        readAllIHS(filename, fileLoci, nfiles, freq, score);

        double *mean = chk(new double[numBins]);
        double *variance = chk(new double[numBins]);
        int *n = chk(new int[numBins]);

        double minFreq;
        double maxFreq;
        double step;

        //This would use the empirical range to draw bin boundaries
        //gsl_stats_minmax(&minFreq,&maxFreq,freq,1,totalLoci);

        //This uses the possible range to draw bin boundaries
        minFreq = 0.0;
        maxFreq = 1.0;

        step = (maxFreq - minFreq) / double(numBins);

        double *threshold = chk(new double[numBins]);

        for (int b = 0; b < numBins; b++)
        {
            threshold[b] = minFreq + (b + 1) * step;
        }

        std::cerr << "Calculating mean and variance per frequency bin:\n\n";
        getMeanVarBins(freq, score, totalLoci, mean, variance, n, numBins, threshold);

        gsl_sort(score, 1, totalLoci);

        double upperCutoff, lowerCutoff;

        if (critPercent != DEFAULT_CRIT_PERCENT && (critPercent > 0 && critPercent < 1))
        {
            upperCutoff = gsl_stats_quantile_from_sorted_data (score, 1, totalLoci, 1 - critPercent / 2.0 );
            lowerCutoff = gsl_stats_quantile_from_sorted_data (score, 1, totalLoci, critPercent / 2.0);

            std::cerr << "\nTop cutoff: " << upperCutoff << std::endl;
            std::cerr << "Bottom cutoff: " << lowerCutoff << "\n\n";
            flog << "\nTop cutoff: " << upperCutoff << std::endl;
            flog << "Bottom cutoff: " << lowerCutoff << "\n\n";
        }
        else
        {
            upperCutoff = critNum;
            lowerCutoff = -critNum;
        }
        delete [] freq;
        delete [] score;

        //Output bins info to file.
        std::cerr << "bin\tnum\tmean\tvariance\n";
        flog << "bin\tnum\tmean\tvariance\n";
        for (int i = 0; i < numBins; i++)
        {
            std::cerr << threshold[i] << "\t" << n[i] <<  "\t" << mean[i] << "\t" << variance[i] << std::endl;
            flog << threshold[i] << "\t" << n[i] <<  "\t" << mean[i] << "\t" << variance[i] << std::endl;
        }

				if (!binsOutfile.empty()) {
					const BinsInfo binsInfo(mean, variance, n, numBins, threshold, upperCutoff, lowerCutoff,
																	IHS, XPEHH, NSL, SOFT);


					// save data to archive
					if (1) {
						// create and open a character archive for output
						std::ofstream ofs(binsOutfile.c_str());
						assert(ofs.good());
						boost::archive::binary_oarchive oa(ofs);
						// write class instance to archive
						oa << B_NVP(binsInfo);
						// archive and stream closed when destructors are called
					}
					std::this_thread::sleep_for(std::chrono::seconds(2));

					// load data from archive
					{
						std::ifstream ifs(binsOutfile.c_str());
						assert(ifs.good());
						//std::string s(std::istreambuf_iterator<char>(ifs), {});
						//std::cerr << "CONTENTS: " << s << "\n";
						if(1){
							boost::archive::binary_iarchive ia(ifs, std::ios::binary);
							BinsInfo binsInfoRead;
							
							std::cerr << "RESTORING BINS FROM " << binsOutfile << "\n";
							ia >> B_NVP(binsInfoRead);
							if(binsInfoRead == binsInfo) {
								std::cerr << "BINS INFO VERIFIED\n";
							} else {
								std::cerr << "BINS INFO NOT VERIFIED!!!!\n";
							}

							// archive and stream closed when destructors are called
						}
					}

				}  // 				if (!binsOutfile.empty()) 
				if (ONLYSAVEBINS) {
					std::cerr << "saved bins, exiting\n";
					return EXIT_SUCCESS;
				}

				if (!binsInfile.empty()) {
					std::ifstream ifs(binsInfile.c_str());
					assert(ifs.good());
					//std::string s(std::istreambuf_iterator<char>(ifs), {});
					//std::cerr << "CONTENTS: " << s << "\n";
					if(1){
						boost::archive::binary_iarchive ia(ifs, std::ios::binary);
						BinsInfo binsInfoRead;
							
						std::cerr << "RESTORING BINS FROM " << binsInfile << "\n";
						ia >> B_NVP(binsInfoRead);

						
						binsInfoRead.set_bins_values(mean, variance, n, numBins, threshold, upperCutoff, lowerCutoff,
																				 IHS, XPEHH, NSL, SOFT);
						// archive and stream closed when destructors are called
					}
				}
				

				

        //Read each file and create normed files.
        if (FIRST) nfiles = 1;
        for (int i = 0; i < nfiles; i++)
        {
					std::cerr << "Normalizing " << filename[i] << "\n";
            normalizeIHSDataByBins(filename[i], outfilename[i], fileLoci[i], mean, variance, n, numBins, threshold, upperCutoff, lowerCutoff);
            //fin[i].close();
            //fout[i].close();
        }

        delete [] threshold;
        delete [] mean;
        delete [] variance;
        delete [] n;

        if (BPWIN) analyzeIHSBPWindows(outfilename, fileLoci, nfiles, winSize, numQBins, minSNPs);
        //if(SNPWIN) analyzeSNPWindows(outfilename,fileLoci,nfiles,snpWinSize);
    }
    else if (XPEHH || SOFT) {

        std::cerr << "Reading all data.\n";
        double *freq1 = chk(new double[totalLoci]);
        double *freq2;
        if(XPEHH) freq2 = chk(new double[totalLoci]);
        double *score = chk(new double[totalLoci]);
        //read in all data
        if(XPEHH) readAllXPEHH(filename, fileLoci, nfiles, freq1, freq2, score, XPEHH_FLIP_POPS);
        if(SOFT) readAllIHH12(filename, fileLoci, nfiles, freq1, score);
        numBins = 1;
        double *mean = chk(new double[numBins]);
        double *variance = chk(new double[numBins]);
        int *n = chk(new int[numBins]);

        double minFreq;
        double maxFreq;
        double step;

        //This would use the empirical range to draw bin boundaries
        //gsl_stats_minmax(&minFreq,&maxFreq,freq,1,totalLoci);

        //This uses the possible range to draw bin boundaries
        minFreq = 0.0;
        maxFreq = 1.0;

        step = (maxFreq - minFreq) / double(numBins);

        double *threshold = chk(new double[numBins]);

        for (int b = 0; b < numBins; b++)
        {
            threshold[b] = minFreq + (b + 1) * step;
        }

        std::cerr << "Calculating mean and variance:\n\n";
        getMeanVarBins(freq1, score, totalLoci, mean, variance, n, numBins, threshold);

        gsl_sort(score, 1, totalLoci);

        double upperCutoff, lowerCutoff;

        if (critPercent != DEFAULT_CRIT_PERCENT && (critPercent > 0 && critPercent < 1))
        {
            upperCutoff = gsl_stats_quantile_from_sorted_data (score, 1, totalLoci, 1 - critPercent / 2.0 );
            lowerCutoff = gsl_stats_quantile_from_sorted_data (score, 1, totalLoci, critPercent / 2.0);

            std::cerr << "\nTop cutoff: " << upperCutoff << std::endl;
            std::cerr << "Bottom cutoff: " << lowerCutoff << "\n\n";
            flog << "\nTop cutoff: " << upperCutoff << std::endl;
            flog << "Bottom cutoff: " << lowerCutoff << "\n\n";
        }
        else
        {
            upperCutoff = critNum;
            lowerCutoff = -critNum;
        }
        delete [] freq1;
        if (XPEHH) delete [] freq2;
        delete [] score;

        //Output bins info to file.
        std::cerr << "num\tmean\tvariance\n";
        flog << "num\tmean\tvariance\n";
        for (int i = 0; i < numBins; i++)
        {
            std::cerr << n[i] <<  "\t" << mean[i] << "\t" << variance[i] << std::endl;
            flog << n[i] <<  "\t" << mean[i] << "\t" << variance[i] << std::endl;
        }

				if (!binsOutfile.empty()) {
					const BinsInfo binsInfo(mean, variance, n, numBins, threshold, upperCutoff, lowerCutoff,
																	IHS, XPEHH, NSL, SOFT);

					// save data to archive
					if (1) {
						// create and open a character archive for output
						std::ofstream ofs(binsOutfile.c_str());
						assert(ofs.good());
						boost::archive::binary_oarchive oa(ofs);
						// write class instance to archive
						oa << B_NVP(binsInfo);
						// archive and stream closed when destructors are called
					}
					std::this_thread::sleep_for(std::chrono::seconds(2));

					// load data from archive
					{
						std::ifstream ifs(binsOutfile.c_str());
						assert(ifs.good());
						//std::string s(std::istreambuf_iterator<char>(ifs), {});
						//std::cerr << "CONTENTS: " << s << "\n";
						if(1){
							boost::archive::binary_iarchive ia(ifs, std::ios::binary);
							BinsInfo binsInfoRead;
							
							std::cerr << "RESTORING BINS FROM " << binsOutfile << "\n";
							ia >> B_NVP(binsInfoRead);
							if(binsInfoRead == binsInfo) {
								std::cerr << "BINS INFO VERIFIED\n";
							} else {
								std::cerr << "BINS INFO NOT VERIFIED!!!!\n";
							}

							// archive and stream closed when destructors are called
						}
					}

				}  // 				if (!binsOutfile.empty()) 
				if (ONLYSAVEBINS) {
					std::cerr << "saved bins, exiting\n";
					return EXIT_SUCCESS;
				}

				if (!binsInfile.empty()) {
					std::ifstream ifs(binsInfile.c_str());
					assert(ifs.good());
					//std::string s(std::istreambuf_iterator<char>(ifs), {});
					//std::cerr << "CONTENTS: " << s << "\n";
					if(1){
						boost::archive::binary_iarchive ia(ifs, std::ios::binary);
						BinsInfo binsInfoRead;
							
						std::cerr << "RESTORING BINS FROM " << binsInfile << "\n";
						ia >> B_NVP(binsInfoRead);

						
						binsInfoRead.set_bins_values(mean, variance, n, numBins, threshold, upperCutoff, lowerCutoff,
																				 IHS, XPEHH, NSL, SOFT);
						// archive and stream closed when destructors are called
					}
				}
				

        //Read each file and create normed files.
        if (FIRST) nfiles = 1;
        for (int i = 0; i < nfiles; i++)
        {
            std::cerr << "Normalizing " << filename[i] << "\n";
            if(XPEHH) normalizeXPEHHDataByBins(filename[i], outfilename[i], fileLoci[i], mean, variance, n, numBins, threshold, upperCutoff, lowerCutoff);
            if(SOFT) normalizeIHH12DataByBins(filename[i], outfilename[i], fileLoci[i], mean, variance, n, numBins, threshold, upperCutoff, lowerCutoff);
            //fin[i].close();
            //fout[i].close();
        }

        delete [] threshold;
        delete [] mean;
        delete [] variance;
        delete [] n;

        if(BPWIN){
            if (XPEHH) analyzeXPEHHBPWindows(outfilename, fileLoci, nfiles, winSize, numQBins, minSNPs);
            if (SOFT) analyzeIHH12BPWindows(outfilename, fileLoci, nfiles, winSize, numQBins, minSNPs);
        }
    }
    flog.close();
    return 0;
}
/*
void analyzeSNPWindows(string normedfiles[],int fileLoci[], int nfiles, int snpWinSize)
{
  cerr << "\nAnalyzing SNP windows:\n\n";
  std::vector<int>* winStarts = new std::vector<int>[nfiles];
  std::vector<int>* winEnds = new std::vector<int>[nfiles];
  std::vector<string>* startSNP = new std::vector<int>[nfiles];
  std::vector<string>* endSNP = new std::vector<int>[nfiles];
  std::vector<double>* fracCrit = new std::vector<double>[nfiles];
  std::ifstream fin;
  std::ofstream fout;
  string* winfilename = new string[nfiles];

  char str[10];
  sprintf(str,"%d",snpWinSize);

  string name;
  int pos;
  double freq, ihh1, ihh2, data, normedData;
  bool crit;
  int numWindows = 0;

  for (int i = 0; i < nfiles; i++)
    {
      fin.open(normedfiles[i].c_str());
      if(fin.fail())
    {
      cerr << "ERROR: " << normedfiles[i] << " " << strerror(errno);
      throw -1;
    }

      //generate winfile names
      winfilename[i] = normedfiles[i];
      winfilename[i] += ".";
      winfilename[i] += str;
      winfilename[i] += "snp.windows";

      //Load information into vectors for analysis
      int winStart = 1;
      int winEnd = winStart + winSize - 1;
      int snpsInWin = 0;
      int numCrit = 0;
      for(int j = 0; j < fileLoci[i]; j++)
    {
      fin >> name;
      fin >> pos;
      fin >> freq;
      fin >> ihh1;
      fin >> ihh2;
      fin >> data;
      fin >> normedData;
      fin >> crit;

      snpsInWin++;
      numCrit+=crit;
    }
    }
}
*/

// return number of columns in file
int countCols(std::ifstream &fin)
{
    int current_cols = 0;
    int currentPos = fin.tellg();

    std::string line;
    // read the rest of this line
    getline(fin, line);
    // read the next full line
    getline(fin, line);

    current_cols = countFields(line);

    fin.clear();
    // restore the previous position
    fin.seekg(currentPos);

    currentPos = fin.tellg();

    return current_cols;
}

int colsToSkip(std::ifstream &fin, int numCols)
{
    // determine the number of cols to skip 
    // (the number more than the number we care about: 6)
    int presentNumCols = countCols(fin);
    int numberColsToSkip = 0;
    std::string junk;

    if ( presentNumCols > numCols)
    {
        numberColsToSkip = presentNumCols - numCols;
    }

    return numberColsToSkip;
}

void skipCols(std::ifstream &fin, int numCols)
{
    std::string junk;
    
    for(int i=0; i<numCols; i++)   
    {
        fin >> junk;
    }
}

void analyzeIHSBPWindows(std::string normedfiles[], int fileLoci[], int nfiles, int winSize, int numQuantiles, int minSNPs)
{
    std::cerr << "\nAnalyzing BP windows:\n\n";
    //int totalLoci = 0;
    //for (int i = 0; i < nfiles; i++) totalLoci+=fileLoci[i];
    std::vector<int> *winStarts = chk(new std::vector<int>[nfiles]);
    std::vector<int> *nSNPs = chk(new std::vector<int>[nfiles]);
    std::vector<double> *fracCrit = chk(new std::vector<double>[nfiles]);
    std::vector<double> *maxAbsScore = chk(new std::vector<double>[nfiles]);

    std::ifstream fin;
    std::ofstream fout;
    std::string *winfilename = chk(new std::string[nfiles]);

    char str[10];
    sprintf(str, "%d", winSize / 1000);

    std::string name;
    int pos;
    double freq, ihh1, ihh2, data, normedData;
    bool crit;
    int numWindows = 0;

    for (int i = 0; i < nfiles; i++)
    {
        fin.open(normedfiles[i].c_str());
        if (fin.fail())
        {
            std::cerr << "ERROR: " << normedfiles[i] << " " << strerror(errno);
            throw - 1;
        }

        //generate winfile names
        winfilename[i] = normedfiles[i];
        winfilename[i] += ".";
        winfilename[i] += str;
        winfilename[i] += "kb.windows";

        //Load information into vectors for analysis
        int winStart = 1;
        int winEnd = winStart + winSize - 1;
        int numSNPs = 0;
        int numCrit = 0;
        int maxAbs = -99999;
        for (int j = 0; j < fileLoci[i]; j++)
        {
            fin >> name;
            fin >> pos;
            fin >> freq;
            fin >> ihh1;
            fin >> ihh2;
            fin >> data;
            fin >> normedData;
            fin >> crit;

            while (pos > winEnd)
            {
                winStarts[i].push_back(winStart);
                nSNPs[i].push_back(numSNPs);
                if (numSNPs == 0) fracCrit[i].push_back(-1);
                else fracCrit[i].push_back(double(numCrit) / double(numSNPs));

                if (numSNPs >= minSNPs && numCrit >= 0) numWindows++;
                maxAbsScore[i].push_back(maxAbs);
                
                maxAbs = -99999;
                winStart += winSize;
                winEnd += winSize;
                numSNPs = 0;
                numCrit = 0;
            }

            if(abs(normedData) > maxAbs) maxAbs = abs(normedData);
            numSNPs++;
            numCrit += crit;
        }
        fin.close();
    }

    std::cerr << numWindows << " nonzero windows.\n";
    flog << numWindows << " nonzero windows.\n";
    double *allSNPsPerWindow = chk(new double[numWindows]);
    double *allFracCritPerWindow = chk(new double[numWindows]);
    int k = 0;
    //Load all num SNPs per window into a single double std::vector to determine quantile boundaries across
    for (int i = 0; i < nfiles; i++)
    {
        for (int j = 0; j < nSNPs[i].size(); j++)
        {
            if (nSNPs[i][j] >= minSNPs && fracCrit[i][j] >= 0)
            {
                allSNPsPerWindow[k] = nSNPs[i][j];
                allFracCritPerWindow[k] = fracCrit[i][j];
                k++;
            }
        }
    }

    //Sort allSNPsPerWindow and rearrange allFracCritPerWindow based on that sorting
    gsl_sort2(allSNPsPerWindow, 1, allFracCritPerWindow, 1, numWindows);

    double *quantileBound = chk(new double[numQuantiles]);
    //determine quantile boundaries
    for (int i = 0; i < numQuantiles; i++)
    {
        quantileBound[i] = gsl_stats_quantile_from_sorted_data (allSNPsPerWindow, 1, numWindows, double(i + 1) / double(numQuantiles));
    }

    /*
     *instead of splitting into a mini vector for each quantile bin, just pass a reference to the
     *start of the slice plus its size to gsl_stats_quantile_from_sorted_data
     *will need the number of snps per quantile bin
     */
    int b = 0;//quantileBoundary index
    int count = 0;//number in quantile, not necessarily equal across quantiles because of ties
    int start = 0;//starting index for the sort function
    std::map<std::string, double> *topWindowBoundary = chk(new std::map<std::string, double>[numQuantiles]);

    //cerr << "\nnSNPs 0.1 0.5 1.0 5.0\n";
    //flog << "\nnSNPs 0.1 0.5 1.0 5.0\n";

    std::cerr << "\nnSNPs 1.0 5.0\n";
    flog << "\nnSNPs 1.0 5.0\n";


    for (int i = 0; i < numWindows; i++)
    {
        if (allSNPsPerWindow[i] <= quantileBound[b])
        {
            count++;
        }
        else
        {
            gsl_sort(&(allFracCritPerWindow[start]), 1, count);

            //topWindowBoundary[b]["0.1"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.999);
            //topWindowBoundary[b]["0.5"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.995);
            topWindowBoundary[b]["1.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.990);
            topWindowBoundary[b]["5.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.950);

            std::cerr << quantileBound[b] << " "
                 //<< topWindowBoundary[b]["0.1"] << " "
                 //<< topWindowBoundary[b]["0.5"] << " "
                 << topWindowBoundary[b]["1.0"] << " "
                 << topWindowBoundary[b]["5.0"] << std::endl;

            flog << quantileBound[b] << " "
                 //<< topWindowBoundary[b]["0.1"] << " "
                 //<< topWindowBoundary[b]["0.5"] << " "
                 << topWindowBoundary[b]["1.0"] << " "
                 << topWindowBoundary[b]["5.0"] << std::endl;

            start = i;
            count = 0;
            b++;
        }
    }

    gsl_sort(&(allFracCritPerWindow[start]), 1, count);
    //topWindowBoundary[b]["0.1"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.999);
    //topWindowBoundary[b]["0.5"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.995);
    topWindowBoundary[b]["1.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.990);
    topWindowBoundary[b]["5.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.950);

    std::cerr << quantileBound[b] << " "
         //<< topWindowBoundary[b]["0.1"] << " "
         //<< topWindowBoundary[b]["0.5"] << " "
         << topWindowBoundary[b]["1.0"] << " "
         << topWindowBoundary[b]["5.0"] << "\n\n";

    flog << quantileBound[b] << " "
         //<< topWindowBoundary[b]["0.1"] << " "
         //<< topWindowBoundary[b]["0.5"] << " "
         << topWindowBoundary[b]["1.0"] << " "
         << topWindowBoundary[b]["5.0"] << "\n\n";

    delete [] allSNPsPerWindow;
    delete [] allFracCritPerWindow;

    for (int i = 0; i < nfiles; i++)
    {
        fout.open(winfilename[i].c_str());
        if (fout.fail())
        {
            std::cerr << "ERROR: " << winfilename[i] << " " << strerror(errno);
            throw - 1;
        }
        std::cerr << "Creating window file " << winfilename[i] << std::endl;
        flog << "Creating window file " << winfilename[i] << std::endl;
        for (int j = 0; j < nSNPs[i].size(); j++)
        {
            if (nSNPs[i][j] < minSNPs || fracCrit[i][j] < 0)
            {
                fout << winStarts[i][j] << "\t" << winStarts[i][j] + winSize << "\t" << nSNPs[i][j] << "\t" << fracCrit[i][j] << "\t-1\tNA" << std::endl;
                continue;
            }
            double percentile = 100.0;
            for (b = 0; b < numQuantiles; b++)
            {
                if (nSNPs[i][j] <= quantileBound[b]) break;
            }

            if (fracCrit[i][j] >= topWindowBoundary[b]["5.0"] && fracCrit[i][j] < topWindowBoundary[b]["1.0"])
            {
                percentile = 5.0;
            }
            else if (fracCrit[i][j] >= topWindowBoundary[b]["1.0"])// && fracCrit[i][j] < topWindowBoundary[b]["0.5"])
            {
                percentile = 1.0;
            }
            /*
            else if (fracCrit[i][j] >= topWindowBoundary[b]["0.5"] && fracCrit[i][j] < topWindowBoundary[b]["0.1"])
            {
                percentile = 0.5;
            }
            else if (fracCrit[i][j] >= topWindowBoundary[b]["0.1"])
            {
                percentile = 0.1;
            }
            */
            fout << winStarts[i][j] << "\t" << winStarts[i][j] + winSize << "\t" << nSNPs[i][j] << "\t" << fracCrit[i][j] << "\t" << percentile << "\t";
            if(maxAbsScore[i][j] == -99999){
                fout << "NA\n";
            }
            else{
                fout << maxAbsScore[i][j] << std::endl;
            }
        }
        fout.close();
    }

    delete [] quantileBound;
    delete [] topWindowBoundary;
    delete [] winStarts;
    delete [] nSNPs;
    delete [] fracCrit;
    delete [] winfilename;

    return;
}

void analyzeXPEHHBPWindows(std::string normedfiles[], int fileLoci[], int nfiles, int winSize, int numQuantiles, int minSNPs)
{
    std::cerr << "\nAnalyzing BP windows:\n\n";
    //int totalLoci = 0;
    //for (int i = 0; i < nfiles; i++) totalLoci+=fileLoci[i];
    std::vector<int> *winStarts = chk(new std::vector<int>[nfiles]);
    std::vector<int> *nSNPs = chk(new std::vector<int>[nfiles]);
    std::vector<double> *fracCritTop = chk(new std::vector<double>[nfiles]);
    std::vector<double> *fracCritBot = chk(new std::vector<double>[nfiles]);
    std::vector<double> *maxScore = chk(new std::vector<double>[nfiles]);
    std::vector<double> *minScore = chk(new std::vector<double>[nfiles]);

    std::ifstream fin;
    std::ofstream fout;
    std::string *winfilename = chk(new std::string[nfiles]);

    char str[10];
    sprintf(str, "%d", winSize / 1000);

    std::string name, header;
    int pos;
    double gpos, freq1, freq2, ihh1, ihh2, data, normedData;
    int crit;
    int numWindowsTop = 0;
    int numWindowsBot = 0;

    for (int i = 0; i < nfiles; i++)
    {
        fin.open(normedfiles[i].c_str());
        if (fin.fail())
        {
            std::cerr << "ERROR: " << normedfiles[i] << " " << strerror(errno);
            throw - 1;
        }

        getline(fin, header);

        //generate winfile names
        winfilename[i] = normedfiles[i];
        winfilename[i] += ".";
        winfilename[i] += str;
        winfilename[i] += "kb.windows";

        //Load information into vectors for analysis
        int winStart = 1;
        int winEnd = winStart + winSize - 1;
        int numSNPs = 0;
        int numCritTop = 0;
        int numCritBot = 0;
        double max = -99999;
        double min = 99999;
        for (int j = 0; j < fileLoci[i]; j++)
        {
            fin >> name;
            fin >> pos;
            fin >> gpos;
            fin >> freq1;
            fin >> ihh1;
            fin >> freq2;
            fin >> ihh2;
            fin >> data;
            fin >> normedData;
            fin >> crit;

            while (pos > winEnd)
            {
                winStarts[i].push_back(winStart);
                nSNPs[i].push_back(numSNPs);
                if (numSNPs < minSNPs){
                    fracCritTop[i].push_back(-1);
                    fracCritBot[i].push_back(-1);
                }
                else{
                    fracCritTop[i].push_back(double(numCritTop) / double(numSNPs));
                    numWindowsTop++;
                    fracCritBot[i].push_back(double(numCritBot) / double(numSNPs));
                    numWindowsBot++;
                }
                maxScore[i].push_back(max);
                minScore[i].push_back(min);
                
                max = -99999;
                min = 99999;
                winStart += winSize;
                winEnd += winSize;
                numSNPs = 0;
                numCritTop = 0;
                numCritBot = 0;
            }

            if(normedData > max) max = normedData;
            if(normedData < min) min = normedData;
            numSNPs++;
            if(crit == 1) numCritTop++;
            else if (crit == -1) numCritBot++;
        }
        fin.close();
    }

    std::cerr << numWindowsTop << " windows with nSNPs >= " << minSNPs << ".\n";
    flog << numWindowsTop << " windows with nSNPs >= " << minSNPs << ".\n";
    double *allSNPsPerWindowTop = chk(new double[numWindowsTop]);
    double *allFracCritPerWindowTop = chk(new double[numWindowsTop]);

    double *allSNPsPerWindowBot = chk(new double[numWindowsBot]);
    double *allFracCritPerWindowBot = chk(new double[numWindowsBot]);

    int kTop = 0;
    int kBot = 0;
    //Load all num SNPs per window into a single double vector to determine quantile boundaries across
    for (int i = 0; i < nfiles; i++)
    {
        for (int j = 0; j < nSNPs[i].size(); j++)
        {
            if (nSNPs[i][j] >= minSNPs && fracCritTop[i][j] >= 0)
            {
                allSNPsPerWindowTop[kTop] = nSNPs[i][j];
                allFracCritPerWindowTop[kTop] = fracCritTop[i][j];
                kTop++;
            }
            if (nSNPs[i][j] >= minSNPs && fracCritBot[i][j] >= 0)
            {
                allSNPsPerWindowBot[kBot] = nSNPs[i][j];
                allFracCritPerWindowBot[kBot] = fracCritBot[i][j];
                kBot++;
            }
        }
    }

    //Sort allSNPsPerWindow and rearrange allFracCritPerWindow based on that sorting
    gsl_sort2(allSNPsPerWindowTop, 1, allFracCritPerWindowTop, 1, numWindowsTop);
    gsl_sort2(allSNPsPerWindowBot, 1, allFracCritPerWindowBot, 1, numWindowsBot);

    double *quantileBoundTop = chk(new double[numQuantiles]);
    double *quantileBoundBot = chk(new double[numQuantiles]);

    //determine quantile boundaries
    for (int i = 0; i < numQuantiles; i++)
    {
        quantileBoundTop[i] = gsl_stats_quantile_from_sorted_data (allSNPsPerWindowTop, 1, numWindowsTop, double(i + 1) / double(numQuantiles));
        quantileBoundBot[i] = gsl_stats_quantile_from_sorted_data (allSNPsPerWindowBot, 1, numWindowsBot, double(i + 1) / double(numQuantiles));
    }



////TOP
    /*
     *instead of splitting into a mini vector for each quantile bin, just pass a reference to the
     *start of the slice plus its size to gsl_stats_quantile_from_sorted_data
     *will need the number of snps per quantile bin
     */
    int bTop = 0;//quantileBoundary index
    int countTop = 0;//number in quantile, not necessarily equal across quantiles because of ties
    int startTop = 0;//starting index for the sort function
    std::map<std::string, double> *topWindowBoundaryTop = chk(new std::map<std::string, double>[numQuantiles]);


    std::cerr << "\nHigh Scores\nnSNPs 1.0 5.0\n";
    flog << "\nHigh Scores\nnSNPs 1.0 5.0\n";


    for (int i = 0; i < numWindowsTop; i++)
    {
        if (allSNPsPerWindowTop[i] <= quantileBoundTop[bTop])
        {
            countTop++;
        }
        else
        {
            gsl_sort(&(allFracCritPerWindowTop[startTop]), 1, countTop);

            topWindowBoundaryTop[bTop]["1.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindowTop[startTop]), 1, countTop, 0.990);
            topWindowBoundaryTop[bTop]["5.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindowTop[startTop]), 1, countTop, 0.950);

            std::cerr << quantileBoundTop[bTop] << " "
                 << topWindowBoundaryTop[bTop]["1.0"] << " "
                 << topWindowBoundaryTop[bTop]["5.0"] << std::endl;

            flog << quantileBoundTop[bTop] << " "
                 << topWindowBoundaryTop[bTop]["1.0"] << " "
                 << topWindowBoundaryTop[bTop]["5.0"] << std::endl;

            startTop = i;
            countTop = 0;
            bTop++;
        }
    }

    gsl_sort(&(allFracCritPerWindowTop[startTop]), 1, countTop);
    topWindowBoundaryTop[bTop]["1.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindowTop[startTop]), 1, countTop, 0.990);
    topWindowBoundaryTop[bTop]["5.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindowTop[startTop]), 1, countTop, 0.950);

    std::cerr << quantileBoundTop[bTop] << " "
         << topWindowBoundaryTop[bTop]["1.0"] << " "
         << topWindowBoundaryTop[bTop]["5.0"] << "\n\n";

    flog << quantileBoundTop[bTop] << " "
         << topWindowBoundaryTop[bTop]["1.0"] << " "
         << topWindowBoundaryTop[bTop]["5.0"] << "\n\n";

    delete [] allSNPsPerWindowTop;
    delete [] allFracCritPerWindowTop;

///BOT 
/*
     *instead of splitting into a mini vector for each quantile bin, just pass a reference to the
     *start of the slice plus its size to gsl_stats_quantile_from_sorted_data
     *will need the number of snps per quantile bin
     */
    int bBot = 0;//quantileBoundary index
    int countBot = 0;//number in quantile, not necessarily equal across quantiles because of ties
    int startBot = 0;//starting index for the sort function
    std::map<std::string, double> *topWindowBoundaryBot = chk(new std::map<std::string, double>[numQuantiles]);


    std::cerr << "\nLow Scores\nnSNPs 1.0 5.0\n";
    flog << "\nLow Scores\nnSNPs 1.0 5.0\n";


    for (int i = 0; i < numWindowsBot; i++)
    {
        if (allSNPsPerWindowBot[i] <= quantileBoundBot[bBot])
        {
            countBot++;
        }
        else
        {
            gsl_sort(&(allFracCritPerWindowBot[startBot]), 1, countBot);

            topWindowBoundaryBot[bBot]["1.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindowBot[startBot]), 1, countBot, 0.990);
            topWindowBoundaryBot[bBot]["5.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindowBot[startBot]), 1, countBot, 0.950);

            std::cerr << quantileBoundBot[bBot] << " "
                 << topWindowBoundaryBot[bBot]["1.0"] << " "
                 << topWindowBoundaryBot[bBot]["5.0"] << std::endl;

            flog << quantileBoundBot[bBot] << " "
                 << topWindowBoundaryBot[bBot]["1.0"] << " "
                 << topWindowBoundaryBot[bBot]["5.0"] << std::endl;

            startBot = i;
            countBot = 0;
            bBot++;
        }
    }

    gsl_sort(&(allFracCritPerWindowBot[startBot]), 1, countBot);
    topWindowBoundaryBot[bBot]["1.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindowBot[startBot]), 1, countBot, 0.990);
    topWindowBoundaryBot[bBot]["5.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindowBot[startBot]), 1, countBot, 0.950);

    std::cerr << quantileBoundBot[bBot] << " "
         << topWindowBoundaryBot[bBot]["1.0"] << " "
         << topWindowBoundaryBot[bBot]["5.0"] << "\n\n";

    flog << quantileBoundBot[bBot] << " "
         << topWindowBoundaryBot[bBot]["1.0"] << " "
         << topWindowBoundaryBot[bBot]["5.0"] << "\n\n";

    delete [] allSNPsPerWindowBot;
    delete [] allFracCritPerWindowBot;


    for (int i = 0; i < nfiles; i++)
    {
        fout.open(winfilename[i].c_str());
        if (fout.fail())
        {
            std::cerr << "ERROR: " << winfilename[i] << " " << strerror(errno);
            throw - 1;
        }
        std::cerr << "Creating window file " << winfilename[i] << std::endl;
        flog << "Creating window file " << winfilename[i] << std::endl;
        for (int j = 0; j < nSNPs[i].size(); j++)
        {
            fout << winStarts[i][j] << "\t" << winStarts[i][j] + winSize << "\t" << nSNPs[i][j] << "\t" << fracCritTop[i][j] << "\t" << fracCritBot[i][j] << "\t";
            if (nSNPs[i][j] < minSNPs)
            {
                fout << "-1\t-1\tNA\tNA" << std::endl;
                continue;
            }

            double percentile = 100.0;
            for (bTop = 0; bTop < numQuantiles; bTop++)
            {
                if (nSNPs[i][j] <= quantileBoundTop[bTop]) break;
            }

            if (fracCritTop[i][j] >= topWindowBoundaryTop[bTop]["5.0"] && fracCritTop[i][j] < topWindowBoundaryTop[bTop]["1.0"])
            {
                percentile = 5.0;
            }
            else if (fracCritTop[i][j] >= topWindowBoundaryTop[bTop]["1.0"])// && fracCritTop[i][j] < topWindowBoundaryTop[b]["0.5"])
            {
                percentile = 1.0;
            }
            
            fout << percentile << "\t";

            percentile = 100.0;
            for (bBot = 0; bBot < numQuantiles; bBot++)
            {
                if (nSNPs[i][j] <= quantileBoundBot[bBot]) break;
            }

            if (fracCritBot[i][j] >= topWindowBoundaryBot[bBot]["5.0"] && fracCritBot[i][j] < topWindowBoundaryBot[bBot]["1.0"])
            {
                percentile = 5.0;
            }
            else if (fracCritBot[i][j] >= topWindowBoundaryBot[bBot]["1.0"])// && fracCritTop[i][j] < topWindowBoundaryTop[b]["0.5"])
            {
                percentile = 1.0;
            }
            
            fout << percentile << "\t";

            if(maxScore[i][j] == -99999){
                fout << "NA\t";
            }
            else{
                fout << maxScore[i][j] << "\t";
            }
            if(minScore[i][j] == 99999){
                fout << "NA\n";
            }
            else{
                fout << minScore[i][j] << std::endl;
            }
        }
        fout.close();
    }

    delete [] quantileBoundTop;
    delete [] topWindowBoundaryTop;
    delete [] quantileBoundBot;
    delete [] topWindowBoundaryBot;

    delete [] winStarts;
    delete [] nSNPs;
    delete [] fracCritTop;
    delete [] fracCritBot;

    delete [] winfilename;

    return;
}

void analyzeIHH12BPWindows(std::string normedfiles[], int fileLoci[], int nfiles, int winSize, int numQuantiles, int minSNPs)
{
    std::cerr << "\nAnalyzing BP windows:\n\n";
    //int totalLoci = 0;
    //for (int i = 0; i < nfiles; i++) totalLoci+=fileLoci[i];
    std::vector<int> *winStarts = chk(new std::vector<int>[nfiles]);
    std::vector<int> *nSNPs = chk(new std::vector<int>[nfiles]);
    std::vector<double> *fracCrit = chk(new std::vector<double>[nfiles]);

    std::ifstream fin;
    std::ofstream fout;
    std::string *winfilename = chk(new std::string[nfiles]);

    char str[10];
    sprintf(str, "%d", winSize / 1000);

    std::string name, header;
    int pos;
    double gpos, freq1, ihh12, data, normedData;
    bool crit;
    int numWindows = 0;

    for (int i = 0; i < nfiles; i++)
    {
        fin.open(normedfiles[i].c_str());
        if (fin.fail())
        {
            std::cerr << "ERROR: " << normedfiles[i] << " " << strerror(errno);
            throw - 1;
        }

        getline(fin, header);

        //generate winfile names
        winfilename[i] = normedfiles[i];
        winfilename[i] += ".";
        winfilename[i] += str;
        winfilename[i] += "kb.windows";

        //Load information into vectors for analysis
        int winStart = 1;
        int winEnd = winStart + winSize - 1;
        int numSNPs = 0;
        int numCrit = 0;
        for (int j = 0; j < fileLoci[i]; j++)
        {
            fin >> name;
            fin >> pos;
            fin >> freq1;
            fin >> data;
            fin >> normedData;
            fin >> crit;

            while (pos > winEnd)
            {
                winStarts[i].push_back(winStart);
                nSNPs[i].push_back(numSNPs);
                if (numSNPs == 0) fracCrit[i].push_back(-1);
                else fracCrit[i].push_back(double(numCrit) / double(numSNPs));

                if (numSNPs >= minSNPs && numCrit >= 0) numWindows++;

                winStart += winSize;
                winEnd += winSize;
                numSNPs = 0;
                numCrit = 0;
            }

            numSNPs++;
            numCrit += crit;
        }
        fin.close();
    }

    std::cerr << numWindows << " nonzero windows.\n";
    flog << numWindows << " nonzero windows.\n";
    double *allSNPsPerWindow = chk(new double[numWindows]);
    double *allFracCritPerWindow = chk(new double[numWindows]);
    int k = 0;
    //Load all num SNPs per window into a single double vector to determine quantile boundaries across
    for (int i = 0; i < nfiles; i++)
    {
        for (int j = 0; j < nSNPs[i].size(); j++)
        {
            if (nSNPs[i][j] >= minSNPs && fracCrit[i][j] >= 0)
            {
                allSNPsPerWindow[k] = nSNPs[i][j];
                allFracCritPerWindow[k] = fracCrit[i][j];
                k++;
            }
        }
    }

    //Sort allSNPsPerWindow and rearrange allFracCritPerWindow based on that sorting
    gsl_sort2(allSNPsPerWindow, 1, allFracCritPerWindow, 1, numWindows);

    double *quantileBound = chk(new double[numQuantiles]);
    //determine quantile boundaries
    for (int i = 0; i < numQuantiles; i++)
    {
        quantileBound[i] = gsl_stats_quantile_from_sorted_data (allSNPsPerWindow, 1, numWindows, double(i + 1) / double(numQuantiles));
    }

    /*
     *instead of splitting into a mini vector for each quantile bin, just pass a reference to the
     *start of the slice plus its size to gsl_stats_quantile_from_sorted_data
     *will need the number of snps per quantile bin
     */
    int b = 0;//quantileBoundary index
    int count = 0;//number in quantile, not necessarily equal across quantiles because of ties
    int start = 0;//starting index for the sort function
    std::map<std::string, double> *topWindowBoundary = chk(new std::map<std::string, double>[numQuantiles]);

    //cerr << "\nnSNPs 0.1 0.5 1.0 5.0\n";
    //flog << "\nnSNPs 0.1 0.5 1.0 5.0\n";

    std::cerr << "\nnSNPs 1.0 5.0\n";
    flog << "\nnSNPs 1.0 5.0\n";


    for (int i = 0; i < numWindows; i++)
    {
        if (allSNPsPerWindow[i] <= quantileBound[b])
        {
            count++;
        }
        else
        {
            gsl_sort(&(allFracCritPerWindow[start]), 1, count);

            //topWindowBoundary[b]["0.1"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.999);
            //topWindowBoundary[b]["0.5"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.995);
            topWindowBoundary[b]["1.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.990);
            topWindowBoundary[b]["5.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.950);

            std::cerr << quantileBound[b] << " "
                 //<< topWindowBoundary[b]["0.1"] << " "
                 //<< topWindowBoundary[b]["0.5"] << " "
                 << topWindowBoundary[b]["1.0"] << " "
                 << topWindowBoundary[b]["5.0"] << std::endl;

            flog << quantileBound[b] << " "
                 //<< topWindowBoundary[b]["0.1"] << " "
                 //<< topWindowBoundary[b]["0.5"] << " "
                 << topWindowBoundary[b]["1.0"] << " "
                 << topWindowBoundary[b]["5.0"] << std::endl;

            start = i;
            count = 0;
            b++;
        }
    }

    gsl_sort(&(allFracCritPerWindow[start]), 1, count);
    //topWindowBoundary[b]["0.1"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.999);
    //topWindowBoundary[b]["0.5"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.995);
    topWindowBoundary[b]["1.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.990);
    topWindowBoundary[b]["5.0"] = gsl_stats_quantile_from_sorted_data(&(allFracCritPerWindow[start]), 1, count, 0.950);

    std::cerr << quantileBound[b] << " "
         //<< topWindowBoundary[b]["0.1"] << " "
         //<< topWindowBoundary[b]["0.5"] << " "
         << topWindowBoundary[b]["1.0"] << " "
         << topWindowBoundary[b]["5.0"] << "\n\n";

    flog << quantileBound[b] << " "
         //<< topWindowBoundary[b]["0.1"] << " "
         //<< topWindowBoundary[b]["0.5"] << " "
         << topWindowBoundary[b]["1.0"] << " "
         << topWindowBoundary[b]["5.0"] << "\n\n";

    delete [] allSNPsPerWindow;
    delete [] allFracCritPerWindow;

    for (int i = 0; i < nfiles; i++)
    {
        fout.open(winfilename[i].c_str());
        if (fout.fail())
        {
            std::cerr << "ERROR: " << winfilename[i] << " " << strerror(errno);
            throw - 1;
        }
        std::cerr << "Creating window file " << winfilename[i] << std::endl;
        flog << "Creating window file " << winfilename[i] << std::endl;
        for (int j = 0; j < nSNPs[i].size(); j++)
        {
            if (nSNPs[i][j] < minSNPs || fracCrit[i][j] < 0)
            {
                fout << winStarts[i][j] << "\t" << winStarts[i][j] + winSize << "\t" << nSNPs[i][j] << "\t" << fracCrit[i][j] << "\t-1" << std::endl;
                continue;
            }
            double percentile = 100.0;
            for (b = 0; b < numQuantiles; b++)
            {
                if (nSNPs[i][j] <= quantileBound[b]) break;
            }

            if (fracCrit[i][j] >= topWindowBoundary[b]["5.0"] && fracCrit[i][j] < topWindowBoundary[b]["1.0"])
            {
                percentile = 5.0;
            }
            else if (fracCrit[i][j] >= topWindowBoundary[b]["1.0"])// && fracCrit[i][j] < topWindowBoundary[b]["0.5"])
            {
                percentile = 1.0;
            }
            /*
            else if (fracCrit[i][j] >= topWindowBoundary[b]["0.5"] && fracCrit[i][j] < topWindowBoundary[b]["0.1"])
            {
                percentile = 0.5;
            }
            else if (fracCrit[i][j] >= topWindowBoundary[b]["0.1"])
            {
                percentile = 0.1;
            }
            */
            fout << winStarts[i][j] << "\t" << winStarts[i][j] + winSize << "\t" << nSNPs[i][j] << "\t" << fracCrit[i][j] << "\t" << percentile << std::endl;
        }
        fout.close();
    }

    delete [] quantileBound;
    delete [] topWindowBoundary;
    delete [] winStarts;
    delete [] nSNPs;
    delete [] fracCrit;
    delete [] winfilename;

    return;
}


void getMeanVarBins(double freq[], double data[], int nloci, double mean[], double variance[], int n[], int numBins, double threshold[])
{
	typedef SumKeeper<double> sumkeeper_t;
	std::vector<sumkeeper_t> meanSum(numBins), varianceSum(numBins);
    //initialize
    for (int b = 0; b < numBins; b++)
    {
        n[b] = 0;
        mean[b] = 0;
        variance[b] = 0;
    }

    //Calculate sum(x_i) stored in mean[b], and sum(x_i^2) stored in variance[b] in each frequency bin b
    for (int i = 0; i < nloci; i++)
    {
        if (data[i] == MISSING) continue;
        for (int b = 0; b < numBins; b++)
        {
            if (freq[i] < threshold[b])
            {
                n[b]++;
                mean[b] += data[i];
                variance[b] += data[i] * data[i];

								meanSum[b].add(data[i]);
								varianceSum[b].add(data[i] * data[i]);

                break;
            }
        }
    }
		for ( int ii = 0; ii < numBins; ii++ ) {
			std::cerr << "bin " << ii << " n " << meanSum[ii].getNumVals() << " meanDiff " << (mean[ii] - meanSum[ii].getSum()) << 
				" stdDiff " << (variance[ii] - varianceSum[ii].getSum()) << std::endl;
			mean[ii] = meanSum[ii].getSum();
			variance[ii] = varianceSum[ii].getSum();
		}

    //Transform the sum(x_i) and sum(x_i^2) into mean and variances for each bin
    double temp;
    for (int b = 0; b < numBins; b++)
    {
        temp = ( variance[b] - (mean[b] * mean[b]) / (n[b]) ) / (n[b] - 1);
        variance[b] = temp;
        temp = mean[b] / n[b];
        mean[b] = temp;
    }

    //normalize the full data array
    //so that we can calculate quntiles later
    for (int i = 0; i < nloci; i++)
    {
        if (data[i] == MISSING) continue;
        for (int b = 0; b < numBins; b++)
        {
            if (freq[i] < threshold[b])
            {
                data[i] = (data[i] - mean[b]) / sqrt(variance[b]);
                break;
            }
        }
    }
    return;
}


void saveIHSDataBins(std::string &binsOutfilename, double mean[], double variance[], int n[], int numBins, double threshold[],
										 double upperCutoff, double lowerCutoff) {
	
}
void loadIHSDataBins(std::string &binsOutfilename, double mean[], double variance[], int n[], int &numBins, double threshold[],
										 double &upperCutoff, double &lowerCutoff);

//Reads a file, calculates the normalized score, and
//outputs the original row plus normed score
void normalizeIHSDataByBins(std::string &filename, std::string &outfilename, int &fileLoci, double mean[], double variance[], int n[], int numBins, double threshold[], double upperCutoff, double lowerCutoff)
{
    std::ifstream fin;
    std::ofstream fout;

    fin.open(filename.c_str());
    fout.open(outfilename.c_str());
    if (fout.fail())
    {
        std::cerr << "ERROR: " << outfilename << " " << strerror(errno);
        throw 1;
    }

    std::string name;
    int pos;
    double freq, data, normedData, ihh1, ihh2;;
    int numInBin = 0;
    std::string junk;

    // determine the number of cols to skip 
    // (the number more than the number we care about: 6)
    int numColsToSkip = 0;
    numColsToSkip = colsToSkip(fin, 6);

    for (int j = 0; j < fileLoci; j++)
    {
        fin >> name;
        fin >> pos;
        fin >> freq;
        fin >> ihh1;
        fin >> ihh2;
        fin >> data;

        // read in and skip extra columns
        skipCols(fin, numColsToSkip);

        if (data == MISSING) continue;
        for (int b = 0; b < numBins; b++)
        {
            if (freq < threshold[b])
            {
                normedData = (data - mean[b]) / sqrt(variance[b]);
                numInBin = n[b];
                break;
            }
        }

        if (numInBin >= 20)
        {
            fout << name << "\t"
                 << pos << "\t"
                 << freq << "\t"
                 << ihh1 << "\t"
                 << ihh2 << "\t"
                 << data << "\t"
                 << normedData << "\t";
            if (normedData >= upperCutoff || normedData <= lowerCutoff) fout << "1\n";
            else fout << "0\n";
        }
    }

    fin.close();
    fout.close();

    return;
}

void normalizeXPEHHDataByBins(std::string &filename, std::string &outfilename, int &fileLoci, double mean[], double variance[], int n[], int numBins, double threshold[], double upperCutoff, double lowerCutoff)
{
    std::ifstream fin;
    std::ofstream fout;

    fin.open(filename.c_str());
    fout.open(outfilename.c_str());
    if (fout.fail())
    {
        std::cerr << "ERROR: " << outfilename << " " << strerror(errno);
        throw 1;
    }

    std::string name, header;
    int pos;
    double gpos, freq1, freq2, data, normedData, ihh1, ihh2;;
    int numInBin = 0;

    getline(fin, header);

    fout << header + "\tnormxpehh\tcrit\n";

    for (int j = 0; j < fileLoci; j++)
    {
        fin >> name;
        fin >> pos;
        fin >> gpos;
        fin >> freq1;
        fin >> ihh1;
        fin >> freq2;
        fin >> ihh2;
        fin >> data;

        if (data == MISSING) continue;
        for (int b = 0; b < numBins; b++)
        {
            if (freq1 < threshold[b])
            {
                normedData = (data - mean[b]) / sqrt(variance[b]);
                numInBin = n[b];
                break;
            }
        }

        if (numInBin >= 20)
        {
            fout << name << "\t"
                 << pos << "\t"
                 << gpos << "\t"
                 << freq1 << "\t"
                 << ihh1 << "\t"
                 << freq2 << "\t"
                 << ihh2 << "\t"
                 << data << "\t"
                 << normedData << "\t";
            if (normedData >= upperCutoff) fout << "1\n";
            else if (normedData <= lowerCutoff) fout << "-1\n";
            else fout << "0\n";
        }
    }

    fin.close();
    fout.close();

    return;
}

void normalizeIHH12DataByBins(std::string &filename, std::string &outfilename, int &fileLoci, double mean[], double variance[], int n[], int numBins, double threshold[], double upperCutoff, double lowerCutoff)
{
    std::ifstream fin;
    std::ofstream fout;

    fin.open(filename.c_str());
    fout.open(outfilename.c_str());
    if (fout.fail())
    {
        std::cerr << "ERROR: " << outfilename << " " << strerror(errno);
        throw 1;
    }

    std::string name, header;
    int pos;
    double gpos, freq1, data, normedData, ihh1, ihh2;;
    int numInBin = 0;

    getline(fin, header);

    fout << header + "\tnormihh12\tcrit\n";

    for (int j = 0; j < fileLoci; j++)
    {
        fin >> name;
        fin >> pos;
        fin >> freq1;
        fin >> data;

        if (data == MISSING) continue;
        for (int b = 0; b < numBins; b++)
        {
            if (freq1 < threshold[b])
            {
                normedData = (data - mean[b]) / sqrt(variance[b]);
                numInBin = n[b];
                break;
            }
        }

        if (numInBin >= 20)
        {
            fout << name << "\t"
                 << pos << "\t"
                 << freq1 << "\t"
                 << data << "\t"
                 << normedData << "\t";
            if (normedData >= upperCutoff || normedData <= lowerCutoff) fout << "1\n";
            else fout << "0\n";
        }
    }

    fin.close();
    fout.close();

    return;
}

//returns number of lines in file
int checkIHSfile(std::ifstream &fin)
{
    std::string line;
    int expected_cols = 6;
    int expected_cols_alternate = 10; // this is the case if --ihs-detail is specified (four extra columns for iHH left/right and ancestral/derived)
    int current_cols = 0;

    //beginning of the file stream
    int start = fin.tellg();

    int nloci = 0;
    while (getline(fin, line))
    {
        nloci++;
        current_cols = countFields(line);
        if ((current_cols != expected_cols && current_cols != expected_cols_alternate) && nloci > 1)
        {
            std::cerr << "ERROR: line " << nloci << " has " << current_cols
                 << " columns, but expected " << expected_cols << " or " << expected_cols_alternate << " columns.\n";
            throw 0;
        }
        //previous_cols = current_cols;
    }

    fin.clear();
    fin.seekg(start);

    return nloci;
}

void readAllIHS(std::vector<std::string> filename, int fileLoci[], int nfiles, double freq[], double score[])
{
    std::ifstream fin;
    std::string junk;
    int overallCount = 0;
    for (int i = 0; i < nfiles; i++)
    {
        fin.open(filename[i].c_str());

        int numColsToSkip = 0;
        numColsToSkip = colsToSkip(fin, 6);

        for (int j = 0; j < fileLoci[i]; j++)
        {
            fin >> junk;
            fin >> junk;
            fin >> freq[overallCount];
            fin >> junk;
            fin >> junk;
            fin >> score[overallCount];
            skipCols(fin, numColsToSkip);
            overallCount++;
        }
        fin.close();
    }

    return;
}

int checkXPEHHfile(std::ifstream &fin)
{
    std::string line;
    int expected_cols = 8;
    int current_cols = 0;

    //beginning of the file stream
    int start = fin.tellg();

    int nloci = 0;
    while (getline(fin, line))
    {
        nloci++;
        current_cols = countFields(line);
        if ((current_cols != expected_cols) && nloci > 1)
        {
            std::cerr << "ERROR: line " << nloci << " has " << current_cols
                 << " columns, but expected " << expected_cols << " columns.\n";
            throw 0;
        }
        //previous_cols = current_cols;
    }

    nloci--;

    fin.clear();
    fin.seekg(start);

    return nloci;
}


int checkIHH12file(std::ifstream &fin)
{
    std::string line;
    int expected_cols = 4;
    int current_cols = 0;

    //beginning of the file stream
    int start = fin.tellg();

    int nloci = 0;
    while (getline(fin, line))
    {
        nloci++;
        current_cols = countFields(line);
        if ((current_cols != expected_cols) && nloci > 1)
        {
            std::cerr << "ERROR: line " << nloci << " has " << current_cols
                 << " columns, but expected " << expected_cols << " columns.\n";
            throw 0;
        }
        //previous_cols = current_cols;
    }

    nloci--;

    fin.clear();
    fin.seekg(start);

    return nloci;
}

void readAllXPEHH(std::vector<std::string> filename, int fileLoci[], int nfiles, double freq1[], double freq2[],
									double score[], bool flip_pops)
{
    std::ifstream fin;
    std::string junk;
    int overallCount = 0;
    for (int i = 0; i < nfiles; i++)
    {
        fin.open(filename[i].c_str());
        getline(fin, junk);
        for (int j = 0; j < fileLoci[i]; j++)
        {
            fin >> junk;
            fin >> junk;
            fin >> junk;
            fin >> freq1[overallCount];
            fin >> junk;
            fin >> freq2[overallCount];
            fin >> junk;
            fin >> score[overallCount];

						if (flip_pops) {
							std::swap(freq1[overallCount], freq2[overallCount]);
							score[overallCount] = -score[overallCount];
						}

            overallCount++;
        }
        fin.close();
    }

    return;
}

void readAllIHH12(std::vector<std::string> filename, int fileLoci[], int nfiles, double freq1[], double score[])
{
    std::ifstream fin;
    std::string junk;
    int overallCount = 0;
    for (int i = 0; i < nfiles; i++)
    {
        fin.open(filename[i].c_str());
        getline(fin, junk);
        for (int j = 0; j < fileLoci[i]; j++)
        {
            fin >> junk;
            fin >> junk;
            fin >> freq1[overallCount];
            fin >> score[overallCount];
            overallCount++;
        }
        fin.close();
    }

    return;
}


int countFields(const std::string &str)
{
    std::string::const_iterator it;
    int result;
    int numFields = 0;
    int seenChar = 0;
    for (it = str.begin() ; it < str.end(); it++)
    {
        result = isspace(*it);
        if (result == 0 && seenChar == 0)
        {
            numFields++;
            seenChar = 1;
        }
        else if (result != 0)
        {
            seenChar = 0;
        }
    }
    return numFields;
}

bool isint(std::string str)
{
    for (std::string::iterator it = str.begin(); it != str.end(); it++)
    {
        if (!isdigit(*it)) return 0;
    }

    return 1;
}
