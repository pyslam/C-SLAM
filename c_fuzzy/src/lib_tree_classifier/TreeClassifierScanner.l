%{
	#include <sstream>
	#include <stdexcept>
	#include <string>
	
	#include "TreeClassifierParser.tab.h"
	#include "TreeClassifierScanner.h"
	
	#undef yywrap
	#define yywrap() 1
	
	using namespace std;
%}


%option noyywrap nounput batch debug noinput c++
%option prefix="tc"

%x COMMENT LINECOMMENT

DIGIT [0-9]
CAPITAL [A-Z]
LOWERCASE [a-z]
LETTER {LOWERCASE}|{CAPITAL}
BLANK [ \t]
NEWLINE \n\r|\n
SIGN [-+]
UNDERSCORE [_]

%{
	// Code run each time a pattern is matched.
	#define YY_USER_ACTION  loc.columns (yyleng);
%}
%%
%{
	// Code run each time yylex is called.
	loc.step ();
%}

 /* C++ stle nested comment eater */
"/*" { yy_push_state(YY_START); BEGIN(COMMENT); }
<COMMENT>
{
	{NEWLINE} { loc.lines(yyleng); loc.step(); }
	"/*" { yy_push_state(YY_START); BEGIN(COMMENT); }
	"*/" { BEGIN(yy_top_state()); yy_pop_state(); }
	. {}
}

"//" { yy_push_state(YY_START); BEGIN(LINECOMMENT); }
<LINECOMMENT>
{
	{NEWLINE} { loc.lines(yyleng); loc.step(); BEGIN(yy_top_state()); yy_pop_state(); }
	. {}
}

 /* blank and new line */
{BLANK} { loc.step (); }
{NEWLINE} { loc.lines(yyleng); loc.step(); }

 /* keywords and symbols */
"class"|"CLASS" { return tc::TreeClassifierParser::make_CLASS(loc); }
"end_class"|"END_CLASS" { return tc::TreeClassifierParser::make_END_CLASS(loc); }
"is"|"IS" { return tc::TreeClassifierParser::make_IS(loc); }
"match"|"MATCH" { return tc::TreeClassifierParser::make_MATCH(loc); }
"on"|"ON" { return tc::TreeClassifierParser::make_ON(loc); }
"degree"|"DEGREE" { return tc::TreeClassifierParser::make_DEGREE(loc); }
"variables"|"VARIABLES"  { return tc::TreeClassifierParser::make_VARIABLES(loc); }
"end_variables"|"END_VARIABLES"  { return tc::TreeClassifierParser::make_END_VARIABLES(loc); }
"constants"|"CONSTANTS"  { return tc::TreeClassifierParser::make_CONSTANTS(loc); }
"end_constants"|"END_CONSTANTS"  { return tc::TreeClassifierParser::make_END_CONSTANTS(loc); }
"hidden"|"HIDDEN" { return tc::TreeClassifierParser::make_HIDDEN(loc); }
"extends"|"EXTENDS" { return tc::TreeClassifierParser::make_EXTENDS(loc); }
"=" { return tc::TreeClassifierParser::make_EQUAL(loc); }
"," { return tc::TreeClassifierParser::make_COMMA(loc); }
"." { return tc::TreeClassifierParser::make_PERIOD(loc); }
";" { return tc::TreeClassifierParser::make_SEMICOLON(loc); }
"(" { return tc::TreeClassifierParser::make_LPAR(loc); }
")" { return tc::TreeClassifierParser::make_RPAR(loc); }

 
 /* identificators */
{CAPITAL}({LETTER}|{DIGIT}|{UNDERSCORE})* { return tc::TreeClassifierParser::make_ID(yytext, loc); }
{LOWERCASE}({LETTER}|{DIGIT}|{UNDERSCORE})* { return tc::TreeClassifierParser::make_VAR_ID(yytext, loc); }

 /* unknown tokens */
. { std::stringstream ss; ss << "Error: Unknown char " << *yytext << " at " << loc << endl; throw std::runtime_error(ss.str()); }

 /* End of file */
<<EOF>> { return tc::TreeClassifierParser::make_END(loc); }

%%

int tcFlexLexer::yylex()
{
	return 0;
}

