/*
 * logger.cpp
 *
 *  Created on: Dec 10, 2010
 *      Author: Miha Vitorovic
 */

#include "logger.h"

#include <noDefaultMethods.h>
#include <lock.h>
#include <pvType.h>

#include <epicsExit.h>
#include <errlog.h>

#include <fstream>
#include <iostream>
#include <time.h>
#include <cstring>

using namespace epics::pvData;
using std::ofstream;
using std::ios;
using std::endl;

namespace epics {
    namespace pvAccess {

        class FileLogger : public NoDefaultMethods {
        public:
            FileLogger(String name) {
                logFile.open(name.data(), ios::app);
            }

            ~FileLogger() {
                logFile.close();
            }

            void logMessage(const char* message) {
                time_t rawtime;
                time(&rawtime);
                char* timeStr = ctime(&rawtime);
                timeStr[strlen(timeStr)-1]='\0'; // remove newline

                logFile<<timeStr<<"\t"<<message; // the newline is added by the caller
            }
        private:
            ofstream logFile;

        };

        static FileLogger* fileLogger = NULL;

        static void errLogFileListener(void* pPrivate, const char *message) {
            fileLogger->logMessage(message);
        }

        static void exitFileLoggerHandler(void* pPrivate) {
            errlogFlush();
            delete fileLogger;
        }

        void createFileLogger(String fname) {
            static Mutex mutex = Mutex();
            Lock xx(&mutex);

            if(fileLogger==NULL) {
                fileLogger = new FileLogger(fname);
                errlogInit(2048);
                errlogAddListener(errLogFileListener, NULL);
                epicsAtExit(exitFileLoggerHandler, NULL);
            }
        }

    }
}
