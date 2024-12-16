// An NSIS plugin providing Perl compatible regular expression functions.
// 
// A simple wrapper around the excellent PCRE library which was written by
// Philip Hazel, University of Cambridge.
// 
// For those that require documentation on how to construct regular expressions,
// please see http://www.pcre.org/
// 
// _____________________________________________________________________________
// 
// Copyright (c) 2007 Computerway Business Solutions Ltd.
// Copyright (c) 2005 Google Inc.
// Copyright (c) 1997-2006 University of Cambridge
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
// 
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
// 
//     * Neither the name of the University of Cambridge nor the name of Google
//       Inc. nor the name of Computerway Business Solutions Ltd. nor the names
//       of their contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// 
// Core PCRE Library Written by:       Philip Hazel, University of Cambridge
// C++ Wrapper functions by:           Sanjay Ghemawat, Google Inc.
// Support for PCRE_XXX modifiers by:  Giuseppe Maxia
// NSIS integration by:                Rob Stocks, Computerway Business Solutions Ltd.

#include <windows.h>
#include <stdio.h>
#include "exdll.h"
#include "pcre-7.0\pcre.h"
#include "pcre-7.0\pcrecpp.h"

HINSTANCE g_hInstance;
HWND g_hwndParent;
pcrecpp::RE_Options g_options;
pcrecpp::RE *g_reFaC = NULL;
string g_inputFaC = "";

inline void pushint(int i)
{
	char intstr[32];
	sprintf(intstr, "%d", i);
	pushstring(intstr);
}

inline bool convertboolstr(string s)
{
	return (string("1").compare(s)==0)
			|| (string("true").compare(s)==0)
			|| (string("TRUE").compare(s)==0)
			|| (string("True").compare(s)==0);
}

inline void ResetFind()
{
	if (g_reFaC!=NULL)
	{
		delete g_reFaC;
		g_reFaC = NULL;
	}
}

void SetOrClearOption(string opt, bool set)
{
	if (string("CASELESS").compare(opt)==0) g_options.set_caseless(set);
	if (string("MULTILINE").compare(opt)==0) g_options.set_multiline(set);
	if (string("DOTALL").compare(opt)==0) g_options.set_dotall(set);
	if (string("EXTENDED").compare(opt)==0) g_options.set_extended(set);
	if (string("DOLLAR_ENDONLY").compare(opt)==0) g_options.set_dollar_endonly(set);
	if (string("EXTRA").compare(opt)==0) g_options.set_extra(set);
	if (string("UTF8").compare(opt)==0) g_options.set_utf8(set);
	if (string("UNGREEDY").compare(opt)==0) g_options.set_ungreedy(set);
	if (string("NO_AUTO_CAPTURE").compare(opt)==0) g_options.set_no_auto_capture(set);
	if (string("i").compare(opt)==0) g_options.set_caseless(set);
	if (string("m").compare(opt)==0) g_options.set_multiline(set);
	if (string("s").compare(opt)==0) g_options.set_dotall(set);
	if (string("x").compare(opt)==0) g_options.set_extended(set);
}

bool PerformMatch(pcrecpp::RE *re, string *_subject, pcrecpp::RE::Anchor anchor, int stackoffset, bool requireCaptures)
{
	string* results = NULL;
	int captures;
	bool matched = false;
	pcrecpp::StringPiece subject(*_subject);

	captures = re->NumberOfCapturingGroups();
	if (captures<0) captures = 0;
	if (captures==0 && requireCaptures)
	{
		pushstring("error No capture groups specified in pattern.");
	}
	else
	{
		results = new string[captures];		
		const pcrecpp::Arg* *args = new const pcrecpp::Arg*[captures];
		for (int i = 0; i < captures; i++)
		{
			args[i] = new pcrecpp::Arg(&results[i]);
		}
		int consumed;
		matched = re->DoMatch(subject, anchor, &consumed, args, captures);
		for (int i = 0; i < captures; i++)
		{
			delete args[i];
		}
		delete[] args;

		if (matched)
		{
			subject.remove_prefix(consumed);

			char** savestack = NULL;
			if (stackoffset>0)
			{
				savestack = new char*[stackoffset];
				for (int i = 0; i < stackoffset; i++)
				{
					savestack[i] = new char[g_stringsize];
					popstring(savestack[i]);
				}
			}

			for (int i = (captures-1); i >= 0; i--)
			{
				pushstring(results[i].c_str());
			}

			if (stackoffset>0)
			{
				for (int i = (stackoffset-1); i >= 0; i--)
				{
					pushstring(savestack[i]);
					delete[] savestack[i];
				}
				delete[] savestack;
			}

			delete[] results;
		}

		if (matched) pushint(captures);
		pushstring((matched)?"true":"false");
	}

	subject.CopyToString(_subject);

	return matched;
}


BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	g_hInstance=(HINSTANCE)hInst;
	if (DLL_PROCESS_DETACH == ul_reason_for_call)
	{
		ResetFind();
	}
	return TRUE;
}


extern "C" __declspec(dllexport)
void RECheckPattern(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	char* pattern = new char[string_size];

	popstring(pattern);

	pcrecpp::RE re(pattern, g_options);

	string errorstr = re.error();

	pushstring(errorstr.c_str());

	delete[] pattern;
}

extern "C" __declspec(dllexport)
void REQuoteMeta(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	char* toquote = new char[string_size];

	popstring(toquote);

	string quoted = pcrecpp::RE::QuoteMeta(toquote);

	pushstring(quoted.c_str());

	delete[] toquote;
}


extern "C" __declspec(dllexport)
void REFind(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	if (g_options.no_auto_capture())
	{
		pushstring("error FindAndConsume called with NO_AUTO_CAPTURE");
		return;
	}

	char* pattern = new char[string_size];
	char* subject = new char[string_size];
	char* stackoffsetstr = new char[string_size];
	int stackoffset = 0;

	popstring(pattern);
	popstring(subject);
	popstring(stackoffsetstr);

	if (strlen(stackoffsetstr)>0)
	{
		stackoffset = atoi(stackoffsetstr);
	}

	ResetFind();

	g_reFaC = new pcrecpp::RE(pattern, g_options);
	g_inputFaC = (string)(subject);

	string errorstr = g_reFaC->error();

	if (errorstr.empty())
	{
		if (g_reFaC->NumberOfCapturingGroups()==0)
		{
			string newpattern = "("+g_reFaC->pattern()+")";
			delete g_reFaC;
			g_reFaC = new pcrecpp::RE(newpattern, g_options);
		}
		PerformMatch(g_reFaC, &g_inputFaC, g_reFaC->UNANCHORED, stackoffset, true);
	}
	else
	{
		errorstr.insert(0, "error ");
		pushstring(errorstr.c_str());
	}

	delete[] subject;
	delete[] pattern;
}

extern "C" __declspec(dllexport)
void REFindNext(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	char* stackoffsetstr = new char[string_size];
	int stackoffset = 0;

	popstring(stackoffsetstr);

	if (strlen(stackoffsetstr)>0)
	{
		stackoffset = atoi(stackoffsetstr);
	}

	if (g_reFaC!=NULL)
	{
		PerformMatch(g_reFaC, &g_inputFaC, g_reFaC->UNANCHORED, stackoffset, true);
	}
	else
	{
		pushstring("error FindAndConsume must be called before FindAndConsumeNext.");
	}

	delete[] stackoffsetstr;
}

extern "C" __declspec(dllexport)
void REFindClose(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	ResetFind();
}

extern "C" __declspec(dllexport)
void REMatches(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	char* pattern = new char[string_size];
	char* subject = new char[string_size];
	char* partialstr = new char[string_size];
	char* stackoffsetstr = new char[string_size];
	int stackoffset = 0;

	popstring(pattern);
	popstring(subject);
	popstring(partialstr);
	popstring(stackoffsetstr);

	if (strlen(stackoffsetstr)>0)
	{
		stackoffset = atoi(stackoffsetstr);
	}

	pcrecpp::RE re(pattern, g_options);

	string errorstr = re.error();

	if (errorstr.empty())
	{
		string input(subject);
		pcrecpp::RE::Anchor a = pcrecpp::RE::Anchor::ANCHOR_BOTH;
		if (convertboolstr(partialstr) || g_options.multiline())
			a = pcrecpp::RE::Anchor::UNANCHORED;
		PerformMatch(&re, &input, a, stackoffset, false);
	}
	else
	{
		errorstr.insert(0, "error ");
		pushstring(errorstr.c_str());
	}
	delete[] stackoffsetstr;
	delete[] partialstr;
	delete[] pattern;
	delete[] subject;
}

extern "C" __declspec(dllexport)
void REReplace(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	char* pattern = new char[string_size];
	char* subject = new char[string_size];
	char* replacement = new char[string_size];
	char* replaceall = new char[string_size];

	popstring(pattern);
	popstring(subject);
	popstring(replacement);
	popstring(replaceall);

	pcrecpp::RE re(pattern, g_options);

	string errorstr = re.error();
	if (errorstr.empty())
	{
		string subj = (string)subject;
		string replall = (string)replaceall;
		bool success = false;
		
		if (convertboolstr(replall))
		{
			success = (re.GlobalReplace(replacement, &subj)>0);
		}
		else
		{
			success = re.Replace(replacement, &subj);
		}

		if (success)
		{	
			pushstring(subj.c_str());
			pushstring("true");
		}
		else
		{
			pushstring("false");
		}
	}
	else
	{
		errorstr.insert(0, "error ");
		pushstring(errorstr.c_str());
	}

	delete[] replaceall;
	delete[] replacement;
	delete[] pattern;
	delete[] subject;
}

extern "C" __declspec(dllexport)
void REClearAllOptions(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	g_options.set_all_options(0);
}

extern "C" __declspec(dllexport)
void REClearOption(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	char* pcreopt = new char[string_size];
	popstring(pcreopt);

	SetOrClearOption(pcreopt, false);

	delete[] pcreopt;
}

extern "C" __declspec(dllexport)
void RESetOption(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	char* pcreopt = new char[string_size];
	popstring(pcreopt);

	SetOrClearOption(pcreopt, true);

	delete[] pcreopt;
}

extern "C" __declspec(dllexport)
void REGetOption(HWND hwndParent, int string_size, char *variables, stack_t **stacktop, extra_parameters *extra)
{
	g_hwndParent=hwndParent;

	EXDLL_INIT();

	char* pcreopt = new char[string_size];
	bool set = false;

	popstring(pcreopt);

	if (string("CASELESS").compare(pcreopt)==0)
		set = g_options.caseless();
	else if (string("MULTILINE").compare(pcreopt)==0)
		set = g_options.multiline();
	else if (string("DOTALL").compare(pcreopt)==0)
		set = g_options.dotall();
	else if (string("EXTENDED").compare(pcreopt)==0)
		set = g_options.extended();
	else if (string("DOLLAR_ENDONLY").compare(pcreopt)==0)
		set = g_options.dollar_endonly();
	else if (string("EXTRA").compare(pcreopt)==0)
		set = g_options.extra();
	else if (string("UTF8").compare(pcreopt)==0)
		set = g_options.utf8();
	else if (string("UNGREEDY").compare(pcreopt)==0)
		set = g_options.ungreedy();
	else if (string("NO_AUTO_CAPTURE").compare(pcreopt)==0)
		set = g_options.no_auto_capture();
	else if (string("i").compare(pcreopt)==0)
		set = g_options.caseless();
	else if (string("m").compare(pcreopt)==0)
		set = g_options.multiline();
	else if (string("s").compare(pcreopt)==0)
		set = g_options.dotall();
	else if (string("x").compare(pcreopt)==0)
		set = g_options.extended();

	delete[] pcreopt;

	pushstring(set?"true":"false");
}

