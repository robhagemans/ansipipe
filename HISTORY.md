ANSI|pipe code history
======================
https://github.com/robhagemans/ansipipe

ANSI|pipe is derived from the following sources:

- Win32::Console::ANSI Perl extension by J-L Morel
- dualsubsystem by gaber...@gmail.com, based on work by Richard Eperjesi
- further additions and modifications by Rob Hagemans


dualsubsystem
=============
https://code.google.com/p/dualsubsystem/

Modifications to dualsysystem code:  

(RH 2015-01-28)

-   Change whitespace and naming conventions for readability
-   Replace command-line parsing code
-   Implement Unicode support, UTF8 conversions
-   Send stdout pipe output to ansi parser


Win32::Console::ANSI
====================
http://www.bribes.org/perl/wANSIConsole.html

Modifications to Win32::Console::ANSI code:  

(RH 2015-01-27)

-   Remove DLL injection and API hooking code
-   Remove Win9x support code
-   Add #define macros to allow compilation under MinGW GCC  

(RH 2015-01-28)

-   Change signature of ParseAndPrint to work with one hDev only
-   Remove codepage-conversion sequences ('\x1b(')  

(RH 2015-01-30)

-   Add escape sequences for scrolling, terminal resizing, xterm bright colours
-   Add ANSI input routine (sends sequences for special keys)
-   Refactor code to allow multiple parsers (for cerr and cout)  

In other words, I have extracted only the escape sequence parser from the
original source and then modified that.

The original code was released under PERL's licensing terms.
The author, J-L Morel, has kindly relicensed his code under an MIT-style licence upon my request.
I reproduce his message here (original addresses removed):

    Date: Thu, 03 Sep 2015 15:50:22 +0200
    From: Jean-Louis Morel <>
    To: Rob Hagemans <>
    Subject: Re: licence for project based on Win32::Console::ANSI

    Le 03/09/2015 10:22, Rob Hagemans a écrit :
    > Dear J-L Morel,
    >
    > I write to you because I have used some of the code from your excellent
    > project Win32::Console::ANSI in ANSI|pipe
    > (https://github.com/robhagemans/ansipipe). It is somewhat similar to
    > Jason Hood's ANSICON, but with a different way of communicating with the
    > executable, support for mixed GUI/console applications and the addition
    > of UTF-8 support. Its main use is with my other project PC-BASIC,
    > however I am releasing ANSI|pipe as a separate project in the hope it
    > may be useful to others.
    >
    > I am currently releasing under GPL 2 & 3 and Perl Artistic Licence.
    > However for this tool I would prefer to use an MIT-style license such as
    > the ISC (http://opensource.org/licenses/ISC). However, as
    > Win32::Console::ANSI is GPL/Artistic licensed and I make heavy use of
    > your code, I do not think I can currently do so. It does seem that
    > ANSICON is permissively licensed, so it would seem Jason Hood has
    > received your permission to use your code in that context. However,
    > since I cannot find documentation of this, I prefer to ask you directly.
    >
    > My request is the following: would you consider licensing the relevant
    > code under the ISC licence (or a similar MIT-style licence of your
    > choice)? There would be no need to re-release the project, you would
    > just have to give me permission to do so (that is, licence the code to
    > me under those terms) and I could publish my code with the revised
    > licence and a mention of this permission.
    >
    > Of course, this is all entirely up to you; if you feel you cannot
    > license under those terms I'd be happy to continue to licence my project
    > under the existing GPL/Artistic terms.
    > In all cases, thank you very much for your work on Win32::Console::ANSI.
    >
    > Kind regards,
    > Rob Hagemans

    Hi Rob,

    There is no problem.

    I give you permission to use the code of Win32::Console::ANSI under the
    open-source license of your choice.

    I am happy if my code can be reused in other projects : this is proof
    that it is not so bad ;-)

    Have a nice day.

    --
    JLM
    http://www.bribes.org/perl
