/*
 * wildcharMatcher.h
 *
 *  Created on: Nov 4, 2010
 *      Author: Miha Vitorovic
 */

#ifndef WILDCHARMATCHER_H_
#define WILDCHARMATCHER_H_

#include <pvType.h>

namespace epics {
    namespace pvAccess {

        /**
         * DFA for parsing set strings. DFA was obtained from JFlex using the rule
         * : macro: CHAR = [^-\]\!] (everything except ], ! and - rule :
         * [!]?(-{CHAR})?(({CHAR}-{CHAR})|({CHAR}))({CHAR}-)?\] Result of
         * optimized NDFA is Character classes: class 0: [0-'
         * ']['"'-',']['.'-'\']['^'-65535]  class 1: [']']  class 2: ['!']  class
         * 3: ['-']  Transition graph (for class goto state) State 0: 0 -> 1, 1 ->
         * 2, 2 -> 3, 3 -> 4 State 1: 0 -> 1, 1 -> 2, 3 -> 5 State [FINAL] State
         * 3: 0 -> 1, 1 -> 2, 3 -> 4 State 4: 0 -> 6 State 5: 0 -> 6, 1 -> 2 State
         * 6: 0 -> 1, 1 -> 2
         *
         * @param pattern DOCUMENT ME!
         * @param offset DOCUMENT ME!
         * @param ch DOCUMENT ME!
         *
         * @return DOCUMENT ME!
         */
        bool testSet(const epics::pvData::String pattern, int offset, const char ch);

        /**
         * Recursive method for parsing the string. To avoid copying the strings,
         * the method accepts offset indices into both parameters.
         *
         * @param pattern Pattern used in parsing
         * @param ofp Offset into pattern string (ofp > 0)
         * @param str String to test
         * @param ofs Offset into test string (ofs > 0);
         *
         * @return boolean Do the strings match
         */
        bool parse(const epics::pvData::String pattern, const int ofp, const epics::pvData::String str,
                const int ofs);

        /**
         * DOCUMENT ME!
         *
         * @param pattern DOCUMENT ME!
         * @param str DOCUMENT ME!
         *
         * @return DOCUMENT ME!
         */
        bool match(const epics::pvData::String pattern, const epics::pvData::String str);

    }
}

#endif /* WILDCHARMATCHER_H_ */
