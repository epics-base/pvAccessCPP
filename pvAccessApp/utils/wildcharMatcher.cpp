/*
 * wildcharMatcher.cpp
 *
 *  Created on: Nov 4, 2010
 *      Author: Miha Vitorovic
 */

#include <iostream>

#include "wildcharMatcher.h"

using std::cout;

namespace epics {
    namespace pvAccess {

        /** Wildchar matcher debug */
        const bool WCM_DEBUG = false;

        /** Value of initial state */
        const int WCM_INITIAL = 0;

        /** Value of final state */
        const int WCM_FINAL = 2;

        /** Value of error state */
        const int WCM_ERROR = 99;

        /** Any character (except control, unless escaped) */
        const int WCM_TOKEN_CHAR = 0;

        /** Token for end of set: ] */
        const int WCM_TOKEN_END = 1;

        /** Token for negation: */
        const int WCM_TOKEN_NOT = 2;

        /** Token for range specification: - */
        const int WCM_TOKEN_MINUS = 3;

        /**
         * Transition table holds the nextState used in set parsing. Rows define
         * states, columns define tokens. transitions[1][3] = 5 means: if in state
         * 1 next token is 3, goto state 5
         */
        const int TRANSITIONS[][4] = { { 1, WCM_FINAL, 3, 4 }, { 1, WCM_FINAL,
                WCM_ERROR, 5 }, { WCM_ERROR, WCM_ERROR, WCM_ERROR, WCM_ERROR },
                { 1, WCM_FINAL, WCM_ERROR, 4 }, { 6, WCM_ERROR, WCM_ERROR,
                        WCM_ERROR }, { 6, WCM_FINAL, WCM_ERROR, WCM_ERROR }, {
                        1, WCM_FINAL, WCM_ERROR, WCM_ERROR } };

        int getToken(const char ch) {
            switch(ch) {
            case ']':
                return WCM_TOKEN_END;
            case '!':
                return WCM_TOKEN_NOT;
            case '-':
                return WCM_TOKEN_MINUS;
            default:
                return WCM_TOKEN_CHAR;
            }
        }

        bool testSet(const String pattern, int offset, const char ch) {
            int n = pattern.length();

            int state = WCM_INITIAL;
            int nextToken = ' ';
            char nextChar = ' ';
            char ch1 = ' ';

            bool found = false;

            bool negate = false;

            while(!found) {
                // Check for offset in case of final state, which is over the limit,
                // if ] is at the end of the string.
                if(offset<n) {
                    nextChar = pattern.at(offset);

                    if(nextChar=='\\') {
                        // Any escaped sequence is two characters, otherwise error will
                        // be throws, since this is an invalid sequence anyway
                        nextChar = pattern.at(offset+1);
                        nextToken = WCM_TOKEN_CHAR;
                        offset++;
                    }
                    else {
                        nextToken = getToken(nextChar);
                    }
                }

                switch(state) {
                case WCM_INITIAL:
                    if(nextToken==WCM_TOKEN_NOT) {
                        negate = true;
                        break;
                    }

                    // No break, states 0, 1, 3, 6 have same next condition.
                case 1:
                    if(nextToken==WCM_TOKEN_END) return found^negate;
                case 3:
                case 6:
                    if(nextToken==WCM_TOKEN_CHAR) {
                        found = (ch==nextChar);
                        ch1 = nextChar;
                    }
                    break;
                case 4:
                    // condition [-a...
                    found = (ch<=nextChar);
                    break;
                case 5:
                    if(nextToken==WCM_TOKEN_CHAR) found = ((ch>=ch1)&&(ch
                            <=nextChar)); // condition ...a-z...
                    if(nextToken==WCM_TOKEN_END) found = (ch>=ch1); // condition ...a-]
                    break;
                }

                if(WCM_DEBUG) {
                    cout<<"( "<<state<<" -> "<<TRANSITIONS[state][nextToken]
                            <<" ) token = "<<nextToken<<" char = "<<nextChar
                            <<", found = "<<found<<", negate = "<<negate;
                }

                // Lookup next state in transition table and check for valid pattern
                state = TRANSITIONS[state][nextToken];

                if(state==WCM_ERROR) return false;
                // don't bother, this is a no match anyway
                // throw new RuntimeException("Invalid pattern");

                if(state==WCM_FINAL) return found^negate;

                offset++;
            }

            return found^negate;
        }

        bool parse(const String pattern, const int ofp, const String str,
                const int ofs) {
            int lp = pattern.length();
            int ls = str.length();

            int ip = ofp; // index into pattern string

            int is = ofs; // index into test string

            char chp, chs;

            if(WCM_DEBUG) {
                if((ip>-1)&&(is>-1)&&(ip<lp)&&(is<ls)) {
                    cout<<"parse: "<<pattern.substr(ip)<<" "<<str.substr(is);
                }
            }

            // Match happens only, if we parse both strings exactly to the end
            while(ip<lp) {
                chp = pattern.at(ip);

                if(WCM_DEBUG) {
                    if((ip>-1)&&(is>-1)&&(ip<lp)&&(is<ls)) {
                        cout<<pattern.substr(ip)<<" "<<str.substr(is);
                    }
                }

                switch(chp) {
                case '[':
                    // Each set must be close with a ], otherwise it is invalid.
                    int end = pattern.find(']', ip);

                    if(end==-1) return false;

                    // Is this set followed by a *
                    bool isWildchar = ((end+1)<lp)&&(pattern.at(end+1)=='*');

                    if(is<ls)
                        chs = str.at(is);
                    else
                        return parse(pattern, end+2, str, is);

                    // Does this character match
                    bool thisChar = testSet(pattern, ip+1, chs);

                    // Check for single character match only if there is no
                    // * at the end.
                    if(!thisChar&&!isWildchar) return false; // Return only if this character does not match

                    if(isWildchar) {
                        // If this character does not match, maybe this set
                        // can be skipped entirely
                        if(!thisChar) {
                            ip = end+2;
                            break;
                        }

                        // Special case when this character matches, although
                        // it should not: a[a-z]*z == az
                        if(parse(pattern, end+2, str, is)) return true;

                        // Try to match next character
                        if(parse(pattern, ip, str, is+1)) return true;
                    }

                    // Single character matched, set was processed, since
                    // no * was at the end.
                    ip = end+1;
                    is++;
                    break;
                case '?':
                    // Obvious
                    ip++;
                    is++;
                    break;
                case '*':
                    // Trailing asterisk means that string matches till the end.
                    // Also, checks if this is last char in the string
                    if(ip+1==lp) return true;

                    // Skip the *
                    do {
                        ip++;
                        chp = pattern.at(ip);
                    } while((ip+1<lp)&&(chp=='*'));

                    // But perform a special check and solve it by recursing
                    // from new position
                    if(chp=='?'&&parse(pattern, ip, str, is)) return true;

                    // Iterate through all possible matches in the test string
                    int i = is;

                    while(i<ls) {
                        // Stupid brute force, but isn't as bad as it seems.
                        // Try all possible matches in the test string.
                        if(parse(pattern, ip, str, i)) return true;

                        i++;
                    }
                    break;
                default:
                    // Literal match
                    if(is==ls||pattern.at(ip)!=str.at(is)) return false;

                    ip++;
                    is++;
                }
            }

            // There could be several * at the end of the pattern, although the
            // test string is at the end.
            while((ip<lp)&&((pattern.at(ip))=='*'))
                ip++;

            // Same condition as with while loop
            return (is==ls)&&(ip==lp);
        }

        bool match(const String pattern, const String str) {
            return parse(pattern, 0, str, 0);
        }

    }
}
