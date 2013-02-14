/*
 * Copyright (c) 2012-2013, The University of Oxford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Oxford nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef OSKAR_OPTION_PARSER_H_
#define OSKAR_OPTION_PARSER_H_

/**
 * @file oskar_OptionParser.h
 */

#include "oskar_global.h"
#include "extern/ezOptionParser-0.2.0/ezOptionParser.hpp"
#include <vector>
#include <string>
#include <cstdio>
#include <stdarg.h>

/**
 * @brief
 * Class to provide a command line passing for OSKAR applications.
 *
 * @details
 * Class providing a wrapper to the ezOptionParser for use with OSKAR
 * applications.
 *
 * Note on the use symbols in option syntax: (the following are advised
 * in order to maintain consistency)
 *
 *  []   = optional (e.g. $ foo [settings file] )
 *  <>   = required (e.g. $ foo <file name> )
 *  ...  = repeating elements, "and so on" (e.g.
 *  |    = mutually exclusive (e.g.
 *
 */
class OSKAR_EXPORT oskar_OptionParser : public ez::ezOptionParser
{
public:
    oskar_OptionParser(const char* title = "")
    {
        this->footer =
                "\n"
                "" + std::string(80, '-') + "\n"
                "OSKAR (version " + OSKAR_VERSION_STR + ")\n"
                "Copyright (C) 2013, The University of Oxford.\n"
                "This program is free and without warranty.\n"
                "" + std::string(80, '-') + "\n";
        setVersion(OSKAR_VERSION_STR, false);
        setTitle(title);
    }
    virtual ~oskar_OptionParser() {}

    // Wrapper to define flags with no arguments
    void addFlag(const char* flag1, const char* help, bool required = false,
            const char* flag2 = 0)
    {
        const char* defaults = "";
        int expectedArgs = 0;
        char delim = 0;
        if (flag2)
            add(defaults, required, expectedArgs, delim, help, flag1, flag2);
        else
            add(defaults, required, expectedArgs, delim, help, flag1);
    }

    // Wrapper to define flags with arguments.
    void addFlag(const char* flag1, const char* help, int expectedArgs,
            const char* defaults = "", bool required = false, const char* flag2 = 0)
    {
        char delim = 0;
        std::string strHelp = help;
        if (strlen(defaults) > 0 && expectedArgs == 1)
            strHelp += " (default = " + std::string(defaults) + ")";
        if (flag2)
            add(defaults, required, expectedArgs, delim, strHelp.c_str(), flag1,
                    flag2);
        else
            add(defaults, required, expectedArgs, delim, strHelp.c_str(), flag1);
    }

    void addRequired(const char* name, const char* help = "")
    {
        // TODO Do something with the help field
        required_.push_back(name);
        requiredHelp_.push_back(help);
    }

    void addOptional(const char* name, const char* help = "")
    {
        // TODO Do something with the help field
        optional_.push_back(name);
        optionalHelp_.push_back(help);
    }
    void setTitle(const char* text)
    {
        this->title = text;
    }
    void setVersion(const char* version, bool show = true)
    {
        if (show)
            this->version = version;
        version_ = version;
    }
    void setDescription(const char* description)
    {
        this->description = description;
    }
    //    void setSyntax(const char* text)
    //    {
    //        this->syntax = "\n  " + std::string(text);
    //    }
    void addExample(const char* text)
    {
        this->example += "  " + std::string(text) + "\n";
    }
    void printUsage()
    {
        std::string usage;
        this->getUsage(usage);
        std::cout << usage;
    }
    void getUsage(std::string& usage)
    {
        this->syntax = this->title + " [OPTIONS]";
        for (int i = 0; i < (int)required_.size(); ++i)
            this->syntax += " <" + required_[i] + ">";
        for (int i = 0; i < (int)optional_.size(); ++i)
            this->syntax += " [" + optional_[i] + "]";
        // TODO overload here rather than editing the library header...!
        ez::ezOptionParser::getUsage(usage);
    }
    int numArgs() const
    {
        return (((int)firstArgs.size()-1) + (int)lastArgs.size());
    }
    std::vector<std::string> getArgs() const
            {
        std::vector<std::string> args;
        for (int i = 1; i < (int)firstArgs.size(); ++i)
            args.push_back(*this->firstArgs[i]);
        for (int i = 0; i < (int)lastArgs.size(); ++i)
            args.push_back(*this->lastArgs[i]);
        return args;
            }
    const char* getArg(int i = 0) const
    {
        if ((int)firstArgs.size()-1 > i)
            return (*this->firstArgs[i+1]).c_str();
        // Requested index is in the last argument set.
        else if (((int)firstArgs.size()-1 + (int)lastArgs.size()) > i)
            return (*this->lastArgs[i-((int)firstArgs.size()-1)]).c_str();
        return 0;
    }
    std::vector<std::string> getInputFiles(int minRequired = 2) const
            {
        std::vector<std::string> files;
        // Note: minRequired+1 because firstArg[0] == binary name
        bool filesFirst = ((int)this->firstArgs.size() >= minRequired+1) &&
                ((int)this->lastArgs.size() == 0);
        if (filesFirst)
        {
            // Note: starts at 1 as index 0 == the binary name.
            for (int i = 1; i < (int)this->firstArgs.size(); ++i)
            {
                files.push_back(*this->firstArgs[i]);
            }
        }
        else
        {
            for (int i = 0; i < (int)this->lastArgs.size(); ++i)
            {
                files.push_back(*this->lastArgs[i]);
            }
        }
        return files;
            }
    bool check_options(int argc, char** argv)
    {
        addFlag("--help", "Display usage instructions and exit.", false);
        addFlag("--version", "Display the program name/version banner and exit.",
                false);
        this->parse(argc, argv);
        numArgs();
        if (isSet("--help")) {
            printUsage();
            return false;
        }
        if (isSet("--version")) {
            std::cout << version_ << std::endl;
            return false;
        }
        std::vector<std::string> badOpts;
        if (!gotRequired(badOpts)) {
            for (int i = 0; i < (int)badOpts.size(); ++i) {
                error("Missing required option: %s", badOpts[i].c_str());
                return false;
            }
        }
        if (!gotExpected(badOpts)) {
            for (int i = 0; i < (int)badOpts.size(); ++i)
            {
                error("Got unexpected number of arguments for option: %s",
                        badOpts[i].c_str());
                return false;
            }
        }
        //        std::cout << std::endl;
        //        std::cout << __PRETTY_FUNCTION__ << std::endl;
        //        std::cout << "firstArgs.size() = " << firstArgs.size() << std::endl;
        //        std::cout << "lastArgs.size()  = " << lastArgs.size() << std::endl;
        //        std::cout << "firstArgs[0]     = " << *firstArgs[0] << std::endl;

        int numReqArgs = (int)required_.size();
        int numOptArgs = (int)optional_.size();
        if (numArgs() < numReqArgs || numArgs() > (numReqArgs +  numOptArgs))
        {
            if (numOptArgs > 0)
                error("Expected %i to %i input argument(s), %i given.",
                        numReqArgs, numReqArgs + numOptArgs, numArgs());
            else
                error("Expected %i input argument(s), %i given", numReqArgs,
                        numArgs());

            return false;
        }
        return true;
    }

    void error(const char* format, ...)
    {
        std::cerr << "ERROR:\n";
        std::cerr << "  ";
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        std::cerr << "\n\n";
        this->printUsage();
    }

private:
    std::vector<std::string> optional_;
    std::vector<std::string> optionalHelp_;
    std::vector<std::string> required_;
    std::vector<std::string> requiredHelp_;
    const char* version_;
};

#endif /* OSKAR_OPTION_PARSER_H_ */