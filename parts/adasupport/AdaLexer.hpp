#ifndef INC_AdaLexer_hpp_
#define INC_AdaLexer_hpp_

#line 28 "ada.g"

#include <antlr/SemanticException.hpp>  // antlr wants this
#include "AdaAST.hpp"
#include "problemreporter.h"

#line 11 "AdaLexer.hpp"
#include <antlr/config.hpp>
/* $ANTLR 2.7.2: "ada.g" -> "AdaLexer.hpp"$ */
#include <antlr/CommonToken.hpp>
#include <antlr/InputBuffer.hpp>
#include <antlr/BitSet.hpp>
#include "AdaTokenTypes.hpp"
#include <antlr/CharScanner.hpp>
class AdaLexer : public antlr::CharScanner, public AdaTokenTypes
{
#line 1883 "ada.g"

private:
  unsigned int m_numberOfErrors;
  ProblemReporter* m_problemReporter;

public:

  void resetErrors ()                          { m_numberOfErrors = 0; }
  unsigned int numberOfErrors () const         { return m_numberOfErrors; }
  void setProblemReporter (ProblemReporter* r) { m_problemReporter = r; }

  void reportError (const antlr::RecognitionException& ex) {
    m_problemReporter->reportError
           (ex.toString ().c_str (),
            ex.getFilename ().c_str (),
            ex.getLine (),
            ex.getColumn ());
    ++m_numberOfErrors;
  }

  void reportError (const std::string& errorMessage) {
    m_problemReporter->reportError
           (errorMessage.c_str (),
            getFilename().c_str (),
            getLine (),
            getColumn ());
    ++m_numberOfErrors;
  }

  void reportWarning (const std::string& warnMessage) {
    m_problemReporter->reportWarning
           (warnMessage.c_str (),
            getFilename ().c_str (),
            getLine (),
            getColumn ());
  }
#line 22 "AdaLexer.hpp"
private:
	void initLiterals();
public:
	bool getCaseSensitiveLiterals() const
	{
		return false;
	}
public:
	AdaLexer(std::istream& in);
	AdaLexer(antlr::InputBuffer& ib);
	AdaLexer(const antlr::LexerSharedInputState& state);
	antlr::RefToken nextToken();
	public: void mCOMMENT_INTRO(bool _createToken);
	public: void mDOT_DOT(bool _createToken);
	public: void mLT_LT(bool _createToken);
	public: void mBOX(bool _createToken);
	public: void mGT_GT(bool _createToken);
	public: void mASSIGN(bool _createToken);
	public: void mRIGHT_SHAFT(bool _createToken);
	public: void mNE(bool _createToken);
	public: void mLE(bool _createToken);
	public: void mGE(bool _createToken);
	public: void mEXPON(bool _createToken);
	public: void mPIPE(bool _createToken);
	public: void mCONCAT(bool _createToken);
	public: void mDOT(bool _createToken);
	public: void mEQ(bool _createToken);
	public: void mLT_(bool _createToken);
	public: void mGT(bool _createToken);
	public: void mPLUS(bool _createToken);
	public: void mMINUS(bool _createToken);
	public: void mSTAR(bool _createToken);
	public: void mDIV(bool _createToken);
	public: void mLPAREN(bool _createToken);
	public: void mRPAREN(bool _createToken);
	public: void mCOLON(bool _createToken);
	public: void mCOMMA(bool _createToken);
	public: void mSEMI(bool _createToken);
	public: void mTIC(bool _createToken);
	public: void mIDENTIFIER(bool _createToken);
	public: void mCHARACTER_LITERAL(bool _createToken);
	public: void mCHAR_STRING(bool _createToken);
	public: void mNUMERIC_LIT(bool _createToken);
	protected: void mDIGIT(bool _createToken);
	protected: void mBASED_INTEGER(bool _createToken);
	protected: void mEXPONENT(bool _createToken);
	protected: void mEXTENDED_DIGIT(bool _createToken);
	public: void mWS_(bool _createToken);
	public: void mCOMMENT(bool _createToken);
private:
	
	static const unsigned long _tokenSet_0_data_[];
	static const antlr::BitSet _tokenSet_0;
	static const unsigned long _tokenSet_1_data_[];
	static const antlr::BitSet _tokenSet_1;
	static const unsigned long _tokenSet_2_data_[];
	static const antlr::BitSet _tokenSet_2;
	static const unsigned long _tokenSet_3_data_[];
	static const antlr::BitSet _tokenSet_3;
	static const unsigned long _tokenSet_4_data_[];
	static const antlr::BitSet _tokenSet_4;
	static const unsigned long _tokenSet_5_data_[];
	static const antlr::BitSet _tokenSet_5;
	static const unsigned long _tokenSet_6_data_[];
	static const antlr::BitSet _tokenSet_6;
	static const unsigned long _tokenSet_7_data_[];
	static const antlr::BitSet _tokenSet_7;
};

#endif /*INC_AdaLexer_hpp_*/
