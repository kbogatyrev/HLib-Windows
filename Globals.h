#pragma once

#include "StdAfx.h"
#include <Regex>
//#include "MainLib_i.h"      // constants used by the parser from IDL declaration

using namespace std;

template <typename T>
wstring static str_ToString (T from)
{
    wstringstream io_;
    io_ << from;
    return io_.str();
};

static wstring str_CyrillicLowercase = L"-абвгдеёжзийклмнопрстуфхцчшщъыьэюя";
static wchar_t * g_szVowels = L"аеёиоуыэюя";
static wchar_t * g_szConsonants = L"бвгджзйклмнпрстфхцчшщ";
static wchar_t * g_szVoicedConsonants = L"бвгджзйлмнр";
static wchar_t * g_szNonVoicedConsonants = L"кпстфхцчшщ";
//static wchar_t * szShSounds = L"шжчщц";

static void operator++ (ET_PartOfSpeech& eo_pos) 
{
    eo_pos = (ET_PartOfSpeech)(eo_pos + 1);
}

static void operator++ (ET_AccentType& eo_at) 
{
    eo_at = (ET_AccentType)(eo_at + 1);
}

