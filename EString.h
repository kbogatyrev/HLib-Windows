#pragma once

#include "tchar.h"
#include <Regex>
#include "Exception.h"

using namespace std;

static const wchar_t * szDefaultBreakChars_ = L" \n";
static const wchar_t * szDefaultTabs_ = L"\t";
static const wchar_t * szDefaultPunctuation_ = L".,;:/?<>[]{}~!()-_\'\"\\";
static const wchar_t * szDefaultEscapeChars_ = L"\27";
static const wchar_t * szDefaultVowels_ = L"аеёиоуыэюя";
static unsigned int uiBlockSize_ = 10;
static const unsigned int cuiMaxSize_ = 100000;
static const unsigned int cuiMaxRegexLength_ = 1000;
static const unsigned int cuiMaxVowelsLength_ = 100;
static const unsigned int cuiMaxSeparatorLength_ = 1000;
static const unsigned int cuiMaxConversionLength_ = 129;
static const unsigned int cuiMaxSearchStringLength_ = 1000;
static const unsigned int cuiMaxSearchSetLength_ = 1000;

//
// Helper objects
//
enum ERelation
{
    ecRelFront      = 20001,
    ecEqual,
    ecLess,
    ecGreater,
    ecNotFound      = 2*cuiMaxSize_,
    ecRelBack       = ecGreater + 1
};

enum ETokenType
{
    ecTokenTypeFront        = 10001,
    ecTokenText,
    ecTokenSpace,           // sequence of break chars
    ecTokenBreakChars       = ecTokenSpace,
    ecTokenTab,             // sequence of tabs
    ecTokenDiacritics,
    ecTokenMeta,            // e.g., XML tag
    ecTokenPunctuation,     // sequence of punct. marks i.e. ; or ?!...
    ecTokenEndOfParagraph,  // B6 hex
    ecTokenRegexMatch,
    ecTokenTypeBack         = ecTokenRegexMatch + 1
};

struct StToken
{
    ETokenType eType;
    unsigned int uiOffset;
    unsigned int uiLength;
    unsigned int uiPosition;

    StToken()
    {
        eType = ecTokenTypeFront;
        uiOffset = 0;
        uiLength = 0;
    }

    virtual ~StToken() {};

    //
    // These operators are needed so we can sort tokens:
    //
    bool operator < (const StToken& stToken) const
    {
        return uiOffset < stToken.uiOffset;
    }

    bool operator == (const StToken& stToken) const
    {
        bool bType = (eType == stToken.eType);
        bool bOffset = (uiOffset == stToken.uiOffset);
        bool bLength = (uiLength == stToken.uiLength);

        return (bType && bOffset && bLength);
    }

    bool bIsLinearText() const
    {
        if (ecTokenTypeFront == eType ||
            ecTokenDiacritics == eType ||
            ecTokenMeta == eType ||
            ecTokenTypeBack == eType)
        {
            return false;
        }
        else
        {
            return true;
        }
    }

};    // struct StToken

struct StBreakChars
{
    const wchar_t * m_szDefault;
    StBreakChars() : m_szDefault (szDefaultBreakChars_) {}
};

struct StTabs
{
    const wchar_t * m_szDefault;
    StTabs() : m_szDefault (szDefaultTabs_) {}
};

struct StPunctuation
{
    const wchar_t * m_szDefault;
    StPunctuation() : m_szDefault (szDefaultPunctuation_) {}
};

struct StEscape
{
    const wchar_t * m_szDefault;
    StEscape() : m_szDefault (szDefaultEscapeChars_) {}
};

struct StVowels
{
    const wchar_t * m_szDefault;
    StVowels() : m_szDefault (szDefaultVowels_) {}
};

template<typename SeparatorPolicy>
class CSeparators : public SeparatorPolicy
{
public:
    CSeparators() : m_szVolatile (NULL)
    {}

    ~CSeparators()
    {
        if (m_szVolatile)
        {
            delete[] m_szVolatile;
        }
    }

    void Set (const wchar_t * szSeparators)
    {
        unsigned int uiLength = wcslen (szSeparators);
        if (uiLength > cuiMaxSeparatorLength_)
        {
            wchar_t * szMsg = L"Separator string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        if (0 == uiLength)
        {
            wchar_t * szMsg = L"Empty separator string.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        if (m_szVolatile)
        {
            delete[] m_szVolatile;
        }

        m_szVolatile = new wchar_t[uiLength+1];
        m_szVolatile[uiLength] = '\0';

        errno_t error = wmemcpy_s (m_szVolatile, cuiMaxSeparatorLength_, szSeparators, uiLength);
        if (error)
        {
            wchar_t * szMsg = L"wmemcpy_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }
    
    }   //  void Set (...)

    const wchar_t * szGet() const
    {
        return m_szVolatile ? m_szVolatile : m_szDefault;
    }

    unsigned int uiLength() const
    {
        return m_szVolatile ? wcslen (m_szVolatile) : wcslen (m_szDefault);
    }

private:
    wchar_t * m_szVolatile;
};

class  CEString
{

private:

    wchar_t * m_szData;

    unsigned int m_uiLength;
    unsigned int m_uiBlocksAllocated;

    wchar_t m_szRegex[cuiMaxRegexLength_];
    
    CSeparators<StBreakChars> m_Breaks;
    CSeparators<StTabs> m_Tabs;
    CSeparators<StPunctuation> m_Punctuation;
    CSeparators<StEscape> m_Escape;
    CSeparators<StVowels> m_Vowels;

    vector<StToken> m_vecTokens;
    vector<StToken> m_vecRegexMatches;

    bool m_bInvalid;

public:

    CEString() : m_uiLength(0), m_uiBlocksAllocated (1), m_bInvalid (true)
    {
        m_szData = new wchar_t[uiBlockSize_];
        m_szData[0] = L'\0';
    }

/*
    CEString (unsigned int uiBlocks) : m_uiLength(0), m_uiBlocksAllocated (uiBlocks), m_bInvalid (true)
    {
        unsigned int uiSize = m_uiBlocksAllocated * uiBlockSize_;
        if (uiSize >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Requested size exceeds maximum allowed.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }
        m_szData = new wchar_t[m_uiBlocksAllocated * uiBlockSize_];
        m_szData[0] = L'\0';
    }
*/

    CEString (const CEString& coSource) : 
        m_szData (NULL), 
        m_uiLength (coSource.m_uiLength), 
        m_uiBlocksAllocated (coSource.m_uiBlocksAllocated),
        m_bInvalid (coSource.m_bInvalid)
    {
        if (coSource.m_uiLength >= cuiMaxSize_ || 
            coSource.m_uiLength >= coSource.m_uiBlocksAllocated * uiBlockSize_)
        {
            wchar_t * szMsg = L"Source string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        m_szData = new wchar_t[m_uiBlocksAllocated * uiBlockSize_];
        errno_t error = wmemmove_s (m_szData, m_uiLength, coSource.m_szData, m_uiLength); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        m_szData[m_uiLength] = L'\0';

        if (!m_bInvalid)
        {
            m_vecTokens = coSource.m_vecTokens;
        }

        m_vecRegexMatches = coSource.m_vecRegexMatches;
    
    }   //  Copy ctor

    CEString (const wchar_t * szSource) : m_szData(NULL), m_uiBlocksAllocated(0), m_bInvalid (true)
    {
        m_uiLength = wcslen (szSource);
        if (m_uiLength >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Source string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        Assign (szSource, m_uiLength);
    
    }   //  CEString (const wchar_t *)

    CEString (wchar_t chrValue) : m_uiLength(1), m_uiBlocksAllocated (1), m_bInvalid (true)
    {
        unsigned int uiSize = m_uiBlocksAllocated * uiBlockSize_;
        if (uiSize >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Requested size exceeds maximum allowed.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }
        m_szData = new wchar_t[m_uiBlocksAllocated * uiBlockSize_];
        m_szData[0] = chrValue;
        m_szData[1] = L'\0';
    
    }   //  CEString (wchar_t chrValue)

    virtual ~CEString()
    {
        delete[] m_szData;
    }


public:

    // Operators

    operator wchar_t *() const
    {
        return m_szData;
    }

    CEString& operator= (const wchar_t * szRhs)
    {
        unsigned int uiSourceLength = wcslen (szRhs);
        Assign (szRhs, uiSourceLength);

        return *this;
    
    }   //  operator= 

    CEString& operator= (const CEString& sRhs)
    {
        if (&sRhs == this)
        {
            return *this;
        }

//        m_uiBlocksAllocated = sRhs.m_uiBlocksAllocated;
//        m_uiLength = sRhs.m_uiLength;
        Assign (sRhs.m_szData, sRhs.m_uiLength);
        
        return *this;
    
    }   //  operator= 

    CEString& operator+= (const wchar_t * szRhs)
    {
        unsigned int uiRhsLength = wcslen (szRhs);
        unsigned int uiNewSize = uiRhsLength + m_uiLength;
        if (uiNewSize >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Right-hand side string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        Concatenate (szRhs, uiRhsLength);

        return *this;

    }   //  operator+=

    CEString& operator+= (const CEString& sRhs)
    {
        unsigned int uiNewSize = sRhs.m_uiLength + m_uiLength;
        if (uiNewSize >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Right-hand side string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        Concatenate (sRhs.m_szData, sRhs.m_uiLength);

        return *this;

    }   //  operator+=

/*
    bool operator== (const wchar_t * szRhs)
    {
        return ecEqual == CEString::eCompare (m_szData, szRhs);
    }

    bool operator== (const CEString& sRhs)
    {
        return ecEqual == CEString::eCompare (m_szData, sRhs);
    }

    bool operator== (CEString& sRhs)
    {
        return ecEqual == CEString::eCompare (m_szData, sRhs);
    }

    bool operator!= (const wchar_t * szRhs)
    {
        return ecEqual != CEString::eCompare (m_szData, szRhs);
    }

    bool operator!= (const CEString& sRhs)
    {
        return ecEqual != CEString::eCompare (m_szData, sRhs);
    }

    bool operator< (const wchar_t * szRhs)
    {
        return ecLess == CEString::eCompare (m_szData, szRhs);
    }

    bool operator< (const CEString& sRhs)
    {
        return ecLess == CEString::eCompare (m_szData, sRhs);
    }

    bool operator> (const wchar_t * szRhs)
    {
        return ecGreater == CEString::eCompare (m_szData, szRhs);
    }

    bool operator> (const CEString& sRhs)
    {
        return ecGreater == CEString::eCompare (m_szData, sRhs);
    }

    bool operator<= (const wchar_t * szRhs)
    {
        ERelation eRet = CEString::eCompare (m_szData, szRhs);
        return (ecLess == eRet || ecEqual == eRet);
    }

    bool operator<= (const CEString& sRhs)
    {
        ERelation eRet = CEString::eCompare (m_szData, sRhs);
        return (ecLess == eRet || ecEqual == eRet);
    }

    bool operator>= (const wchar_t * szRhs)
    {
        ERelation eRet = CEString::eCompare (m_szData, szRhs);
        return (ecGreater == eRet || ecEqual == eRet);
    }

    bool operator>= (const CEString& sRhs)
    {
        ERelation eRet = CEString::eCompare (m_szData, sRhs);
        return (ecGreater == eRet || ecEqual == eRet);
    }
*/

    // Mutators
    void SetBreakChars (const wchar_t * szBreakChars)
    {
        m_Breaks.Set (szBreakChars);
    }

    void SetTabs (const wchar_t * szTabs)
    {
        m_Tabs.Set (szTabs);
    }

    void SetPunctualtion (const wchar_t * szPunctuation)
    {
        m_Punctuation.Set (szPunctuation);
    }
    
    void SetEscapeChars (const wchar_t * szEscapeChars)
    {
        m_Escape.Set (szEscapeChars);
    }

    void SetVowels (const wchar_t * szVowels)
    {
        m_Vowels.Set (szVowels);
    }

    // State/accessors
    bool bIsEmpty() const
    {
        return (0 == m_uiLength);
    }

    unsigned int uiLength() const
    {
        return m_uiLength;
    }

    unsigned char * pToBytes() const
    {
        return (unsigned char *)m_szData;
    }

    // Comparison
    static ERelation eCompare (const wchar_t * szLeft, const wchar_t * szRight)
    {
        int iRet = wcscmp (szLeft, szRight);
        if (0 == iRet)
        {
            return ecEqual;
        }

        return (iRet > 0) ? ecGreater : ecLess;

    }       // eCompare (...)

    static ERelation eCompare (const CEString& sLhs, const wchar_t * szRhs)
    {
        return eCompare (sLhs.m_szData, szRhs);

    }       // eCompare (...)

    static ERelation eCompare (const wchar_t * szLhs, const CEString& sRhs)
    {
        return eCompare (szLhs, sRhs.m_szData);

    }       // eCompare (...)

    static ERelation eCompare (const CEString& sLhs, const CEString& sRhs)
    {
        return eCompare (sLhs.m_szData, sRhs.m_szData);

    }       // eCompare (...)

    static ERelation eCompareNoCase (const wchar_t * szLeft, const wchar_t * szRight)
    {
        wchar_t * szLocale = _wsetlocale (LC_ALL, L"");

        int iRet = _wcsicmp (szLeft, szRight);
        if (0 == iRet)
        {
            return ecEqual;
        }

        return (iRet > 0) ? ecGreater : ecLess;

    }       // eCompare (...)

    // Search
    static bool bIn (const wchar_t chr, const wchar_t * szSearchSet) 
    {
        unsigned int uiSearchSetLength = wcslen (szSearchSet);

        if (uiSearchSetLength > cuiMaxSearchSetLength_)
        {
            wchar_t * szMsg = L"Search set too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        for (int iAt = 0; iAt < 1000; ++iAt)
        {
            if (chr == szSearchSet[iAt])
            {
                return true;
            }

            if (L'\0' == szSearchSet[iAt])
            {
                return false;
            }
        }

        wchar_t * szMsg = L"Search set too long.";
        ERROR_LOG (szMsg);
        throw CException (E_INVALIDARG, szMsg);
    
    }   //  bIn

    unsigned int uiFind (const wchar_t * szRhs) const
    {
        unsigned int uiLength = wcslen (szRhs);

        if (uiLength > m_uiLength)
        {
            return ecNotFound;
        }

        if (uiLength > cuiMaxSearchStringLength_)
        {
            wchar_t * szMsg = L"Right-hand side string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        wchar_t * pPos = wcsstr (m_szData, szRhs);
        if (pPos)
        {
            return pPos - m_szData;
        }

        return ecNotFound;

    }   //  uiFind (...)

    unsigned int uiFindNoCase (const wchar_t * szRhs) const
    {
        wchar_t * szLocale = _wsetlocale (LC_ALL, L"");

        unsigned int uiLength = wcslen (szRhs);

        if (wcslen (szRhs) > m_uiLength)
        {
            return ecNotFound;
        }

        if (uiLength >= cuiMaxSearchStringLength_)
        {
            wchar_t * szMsg = L"Right-hand side string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        wchar_t szCopy[cuiMaxSearchStringLength_];
        errno_t error = wmemmove_s (szCopy, cuiMaxSearchStringLength_, szRhs, uiLength); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s failed.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }
        szCopy[uiLength] = L'\0';
        error = _wcslwr_s (szCopy, cuiMaxSearchStringLength_);
        if (error)
        {
            wchar_t * szMsg = L"_strlwr_s.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }
        
        return uiFind (szCopy);

    }   //  uiFindNoCase (...)

    unsigned int uiRFind (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            ERROR_LOG (L"*** Warning: empty string");
            return ecNotFound;
        }

        unsigned int uiRhsLength = wcslen (szRhs);
        if (uiRhsLength > m_uiLength)
        {
            return ecNotFound;
        }

        for (int iAt = m_uiLength - uiRhsLength; iAt > 0; --iAt)
        {
            if (0 == wmemcmp (&m_szData[iAt], szRhs, uiRhsLength))
            {
                return (unsigned int)iAt;
            }
        }

        return ecNotFound;
    
    }   //  uiRFind

/*
    unsigned int uiRFindNoCase (const wchar_t * szRhs) const
    {
&&&&
        if (0 == m_uiLength)
        {
            ERROR_LOG (L"*** Warning: empty string");
            return ecNotFound;
        }

        unsigned int uiRhsLength = wcslen (szRhs);
        if (uiRhsLength > m_uiLength)
        {
            return ecNotFound;
        }

        for (int iAt = m_uiLength - uiRhsLength; iAt > 0; --iAt)
        {
            if (0 == wcscmp (m_szData, szRhs))
            {
                return (unsigned int)iAt;
            }
        }

        return ecNotFound;
    
    }   //  uiRFindNoCase
*/

    unsigned int uiFindFirstOf (const wchar_t * szSet) const
    {
        size_t uiAt = wcscspn (m_szData, szSet);

        if (m_uiLength == uiAt)
        {
            return ecNotFound;
        }

        return uiAt;
    
    }   //  uiFindFirstOf (...)

/*
    unsigned int uiFindFirstOfNoCase (const wchar_t * szSet) const
    {
&&&&
        size_t uiAt = wcscspn (m_szData, szSet);

        if (m_uiLength == uiAt)
        {
            return ecNotFound;
        }

        return uiAt;
    
    }   //  uiFindFirstOfNoCase (...)
*/

    unsigned int uiFindOneOf (unsigned int uiStartAt, const wchar_t * szSet) const
    {
        if (uiStartAt >= m_uiLength)
        {
            wchar_t * szMsg = L"Search starts past end of string.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        size_t uiAt = wcscspn (&m_szData[uiStartAt], szSet) + uiStartAt;

        if (m_uiLength == uiAt)
        {
            return ecNotFound;
        }

        return uiAt;
    
    }   //  uiFindFirstOf (...)

    unsigned int uiFindLastOf (const wchar_t * szSet) const
    {
        for (int iAt = m_uiLength-1; iAt >= 0; --iAt)
        {
            if (bIn (m_szData[iAt], szSet))
            {
                return (unsigned int)iAt;
            }
        }

        return ecNotFound;
    
    }   //  uiFindLastOf (...)

/*
    unsigned int uiFindLastOfNoCase (const wchar_t * szSet) const
    {
&&&&
        for (int iAt = m_uiLength-1; iAt >= 0; --iAt)
        {
            if (bIn (m_szData[iAt], szSet))
            {
                return (unsigned int)iAt;
            }
        }

        return ecNotFound;
    
    }   //  uiFindLastOfNoCase (...)
*/

    bool bStartsWith (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            return false;
        }

        unsigned int uiRhsLength = wcslen (szRhs);
        if (uiRhsLength > uiRhsLength)
        {
            return false;
        }
        
        int iRet = wcsncmp (m_szData, szRhs, uiRhsLength);

        return (0 == iRet);
    
    }   // bStartsWith (...)

    bool bStartsWithNoCase (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            return false;
        }

        unsigned int uiRhsLength = wcslen (szRhs);
        if (uiRhsLength > uiRhsLength)
        {
            return false;
        }
        
        int i_ret = _wcsnicmp (m_szData, szRhs, uiRhsLength);

        return (0 == i_ret);
    
    }   //  bStartsWithNoCase (...)

    bool bStartsWithOneOf (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            return false;
        }

        return bIn (m_szData[0], szRhs);
    }

    bool bStartsWithOneOfNoCase (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            return false;
        }

        wchar_t cLhs = m_szData[0];
        cLhs = towlower (cLhs);

        unsigned int uiRhsLength = wcslen (szRhs);
        wchar_t * szRhsLowerCase = new wchar_t[uiRhsLength+1];
        wcscpy_s (szRhsLowerCase, uiRhsLength+1, szRhs);
        for (unsigned int uiAt = 0; uiAt < uiRhsLength; ++uiAt)
        {
            szRhsLowerCase[uiAt] = towlower (szRhs[uiAt]);
        }
        int iAt = wcscspn (&cLhs, szRhsLowerCase);

        delete[] szRhsLowerCase;

        return (0 == iAt);
    
    }   //  bEndsWithOneOfNoCase (...)

    bool bEndsWith (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            return false;
        }

        unsigned int uiRhsLength = wcslen (szRhs);
        if (uiRhsLength > uiRhsLength)
        {
            return false;
        }
        
        int iRet = wcsncmp (&m_szData[m_uiLength-uiRhsLength], szRhs, uiRhsLength);
        
        return (0 == iRet);
    
    }   //  bEndsWith (...)

    bool bEndsWithNoCase (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            return false;
        }

        unsigned int uiRhsLength = wcslen (szRhs);
        if (uiRhsLength > uiRhsLength)
        {
            return false;
        }
        
        int iRet = _wcsnicmp (&m_szData[m_uiLength-uiRhsLength], szRhs, uiRhsLength);
        
        return (0 == iRet);
    
    }   //  bEndsWithNoCase (...)

    bool bEndsWithOneOf (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            return false;
        }

        return bIn (m_szData[m_uiLength-1], szRhs);
    }

    bool bEndsWithOneOfNoCase (const wchar_t * szRhs) const
    {
        if (0 == m_uiLength)
        {
            return false;
        }

        wchar_t cLhs = m_szData[m_uiLength-1];
        cLhs = towlower (cLhs);

        unsigned int uiRhsLength = wcslen (szRhs);
        wchar_t * szRhsLowerCase = new wchar_t[uiRhsLength + 1];
        wcscpy_s (szRhsLowerCase, uiRhsLength+1, szRhs);
        for (unsigned int uiAt = 0; uiAt < uiRhsLength; ++uiAt)
        {
            szRhsLowerCase[uiAt] = towlower (szRhs[uiAt]);
        }
        int iAt = wcscspn (&cLhs, szRhsLowerCase);

        delete[] szRhsLowerCase;

        return (0 == iAt);
    
    }   //  bEndsWithOneOfNoCase (...)


    // Manipulations
    CEString& sInsert (unsigned int uiInsertAt, const wchar_t chrInsert)
    {
        if (uiInsertAt > m_uiLength)
        {
            wchar_t * szMsg = L"Bad insersion pos.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        if (L'\0' == chrInsert)
        {
            wchar_t * szMsg = L"Bad insertion char.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        unsigned int uiNewLength = m_uiLength + 1;
        if (uiNewLength >= m_uiBlocksAllocated * uiBlockSize_)
        {
            wchar_t szInsert[2];
            szInsert[0] = chrInsert;
            szInsert[1] = L'\0';
            sInsert (uiInsertAt, szInsert);

            return *this;
        }

        for (unsigned int uiAt = m_uiLength; uiAt > uiInsertAt; --uiAt)
        {
            m_szData[uiAt] = m_szData[uiAt-1];
        }

        m_szData[uiInsertAt] = chrInsert;
        ++m_uiLength;
//        m_szData[++m_uiLength] = L'\0';

        return *this;

    }   // CEString& sInsert (...)

    CEString& sInsert (unsigned int uiInsertAt, const wchar_t * szInsert)
    {
        unsigned int uiCharsToInsert = wcslen (szInsert);
        if (uiInsertAt > m_uiLength)
        {
            wchar_t * szMsg = L"Bad insersion pos.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        if (0 == uiCharsToInsert)
        {
            ERROR_LOG (L"*** Warning: attempt to insert empty string");
            return *this;
        }

        unsigned int uiNewLength = m_uiLength + uiCharsToInsert;
        if (uiNewLength >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Insertions string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        if (uiNewLength >= m_uiBlocksAllocated * uiBlockSize_)
        {
            m_uiBlocksAllocated = ((uiNewLength+1)/uiBlockSize_) + 1;
        }

        wchar_t * szNewData = new wchar_t[m_uiBlocksAllocated * uiBlockSize_];

        errno_t error = wmemmove_s (szNewData, uiNewLength, m_szData, uiInsertAt); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s failed.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        error = wmemmove_s (&szNewData[uiInsertAt], 
                            uiNewLength-uiInsertAt, 
                            szInsert, 
                            uiCharsToInsert); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s failed.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        unsigned int uiPastInsertion = uiInsertAt + uiCharsToInsert;
        error = wmemmove_s (&szNewData[uiPastInsertion], 
                            m_uiLength - uiInsertAt, 
                            &m_szData[uiInsertAt], 
                            m_uiLength - uiInsertAt); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s failed.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        m_uiLength += uiCharsToInsert;
        szNewData[m_uiLength] = L'\0';

        delete[] m_szData;
        m_szData = szNewData;

        return *this;

    }   //  CEString& sInsert (...)

    CEString& sReplace (unsigned int uiStartAt, wchar_t * szReplace)
    {
        unsigned int uiInsertLength = wcslen (szReplace);
        if (uiStartAt + uiInsertLength > m_uiLength)
        {
            wchar_t * szMsg = L"Illegal length.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        errno_t error = wmemcpy_s (&m_szData[uiStartAt], m_uiLength-uiStartAt, szReplace, uiInsertLength);
        if (error)
        {
            wchar_t * szMsg = L"_memccpy failed.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        return *this;
    
    }   //  CEString& sReplace (unsigned int uiStartAt, wchar_t * szReplace)

    CEString& sReplace (unsigned int uiAt, wchar_t chrReplace)
    {
        if (uiAt >= m_uiLength)
        {
            wchar_t * szMsg = L"Illegal position.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        m_szData[uiAt] = chrReplace;

        return *this;

    }   //  CEString& sReplace (unsigned int uiStartAt, wchar_t chrReplace)

    CEString& sReplace (unsigned int uiAt, unsigned int uiMaxCharsToErase, wchar_t * szReplace)
    {
        if (uiAt >= m_uiLength)
        {
            wchar_t * szMsg = L"Illegal insertion point.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        unsigned int uiCharsToInsert = wcslen (szReplace);
        unsigned int uiCharsToErase = min (uiMaxCharsToErase, m_uiLength - uiAt);
        unsigned int uiNewLength = m_uiLength - uiCharsToErase + uiCharsToInsert;
        if (uiNewLength >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Replacement string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        unsigned int uiBlocksToAllocate = ((uiNewLength+1)/uiBlockSize_) + 1;
        unsigned int uiNewSize = uiBlocksToAllocate * uiBlockSize_;
        wchar_t * szNewData = new wchar_t[uiNewSize];
        errno_t error = wmemmove_s (szNewData, uiNewSize, m_szData, uiAt); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        error = wmemmove_s (&szNewData[uiAt], uiNewSize-uiAt, szReplace, uiCharsToInsert); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }
        
        if (m_uiLength > uiAt+uiCharsToErase)
        {
            unsigned int uiRemainder = m_uiLength - (uiAt+uiCharsToInsert);
            error = wmemmove_s (&szNewData[uiAt+uiCharsToInsert], 
                                uiNewSize-uiAt+uiCharsToInsert, 
                                &m_szData[uiAt+uiCharsToErase], 
                                uiRemainder); 
            if (error)
            {
                wchar_t * szMsg = L"wmemmove_s error.";
                ERROR_LOG (szMsg);
                throw CException (error, szMsg);
            }
        }
        delete[] m_szData;
        m_szData = szNewData;
        m_uiLength = m_uiLength - uiCharsToErase + uiCharsToInsert;
        m_szData[m_uiLength] = L'\0';

        return *this;

    }   //  CEString& sReplace (unsigned int uiStartAt, wchar_t chrReplace)

    void Replace (unsigned int uiStartPos, unsigned int uiHowMany, wchar_t cWhat, wchar_t cWithWhat)
    {
        if (0 == m_uiLength)
        {
            ERROR_LOG (L"*** Warning: empty string.");
            return;
        }

        if (uiStartPos >= m_uiLength)
        {
            wchar_t * szMsg = L"Start position beyond string end.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (uiStartPos + uiHowMany > m_uiLength)
        {
            ATLASSERT(0);
            ERROR_LOG (L"*** Warning: attempt to replace beyond string end.");
            uiHowMany = m_uiLength - uiStartPos;
        }

        wchar_t * pcSearchAt = &m_szData[uiStartPos];
        wchar_t * pcReplaceAt = NULL;
        do
        {
            pcReplaceAt = wcschr (&m_szData[uiStartPos], cWhat);
            if (pcReplaceAt)
            {
                *pcReplaceAt = cWithWhat;
                if (L'\0' == *(++pcReplaceAt))
                {
                    pcSearchAt = NULL;
                }
                else
                {
                    pcSearchAt = pcReplaceAt;
                }
            }

        } while (pcReplaceAt);
    
    }   //  void Replace (...)

    void Erase()
    {
        if (m_uiLength >= (m_uiBlocksAllocated * uiBlockSize_))
        {
            wchar_t * szMsg = L"Illegal length.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (L'\0' != m_szData[m_uiLength])
        {
            wchar_t * szMsg = L"Malformed string.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (0 == m_uiLength)
        {
            return;
        }

        m_szData[0] = L'\0';
        m_uiLength = 0;

        Shrink();

    }   //  Erase()

    CEString& sErase (unsigned int uiFirst, unsigned int uiHowMany)
    {
        if (uiFirst >= m_uiLength)
        {
            wchar_t * szMsg = L"Bad offset.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        //if (uiFirst + uiHowMany >= m_uiLength)
        //{
        //    wchar_t * szMsg = L"Bad length.";
        //    ERROR_LOG (szMsg);
        //    throw CException (E_INVALIDARG, szMsg);
        //}

        uiHowMany = min (uiHowMany, m_uiLength - uiFirst);

        unsigned int uiMoveFrom = uiFirst+uiHowMany;
        errno_t error = wmemmove_s (&m_szData[uiFirst], m_uiLength, &m_szData[uiMoveFrom], m_uiLength-uiMoveFrom); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        m_uiLength -= uiHowMany;
        m_szData[m_uiLength] = L'\0';

        Shrink();

        return *this;
        
    }   //  CEString& sErase (unsigned int uiFirst, unsigned int uiHowMany)

    CEString& sErase (unsigned int uiAt)
    {
        if (uiAt >= m_uiLength)
        {
            wchar_t * szMsg = L"Bad offset.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        m_uiLength = uiAt;
        m_szData[m_uiLength] = L'\0';

        Shrink();

        return *this;
    
    }   //  CEString& sErase (...)

    // Case
    void ToLower()
    {
        wchar_t * szLocale = _wsetlocale (LC_ALL, L"");

        errno_t error = _wcslwr_s (m_szData, m_uiLength);
        if (error)
        {
            wchar_t * szMsg = L"_wcslwr_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }
    }       //  ToLower()

    static CEString sToLower (const wchar_t * szSource)
    {   
        wchar_t * szLocale = _wsetlocale (LC_ALL, L"");

        unsigned int uiRhsLength = wcslen (szSource);
        if (uiRhsLength >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Argument string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        wchar_t * szCopy = new wchar_t[uiRhsLength+1];
        szCopy[uiRhsLength] = L'\0';

        errno_t error = wmemmove_s (szCopy, uiRhsLength, szSource, uiRhsLength); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        error = _wcslwr_s (szCopy, uiRhsLength);
        if (error)
        {
            wchar_t * szMsg = L"_wcslwr_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        return CEString (szSource);
    
    }   //  sToLower (const wchar_t * szSource)

    static CEString sToLower (const CEString& sSource)
    {
        wchar_t * szLocale = _wsetlocale (LC_ALL, L"");
        CEString sCopy (sSource);
        sCopy.ToLower();
    }

    void ToUpper()
    {
        wchar_t * szLocale = _wsetlocale (LC_ALL, L"");
        errno_t error = _wcsupr_s (m_szData, m_uiLength);
        if (error)
        {
            wchar_t * szMsg = L"_wcslwr_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }
    }       //  ToUpper()
    
    static CEString sToUpper (const wchar_t * szSource)
    {
        wchar_t * szLocale = _wsetlocale (LC_ALL, L"");

        unsigned int uiRhsLength = wcslen (szSource);
        if (uiRhsLength >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Argument string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        wchar_t * szCopy = new wchar_t[uiRhsLength+1];
        szCopy[uiRhsLength] = L'\0';

        errno_t error = wmemmove_s (szCopy, uiRhsLength, szSource, uiRhsLength); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        error = _wcsupr_s (szCopy, uiRhsLength);
        if (error)
        {
            wchar_t * szMsg = L"_wcslwr_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        return CEString (szSource);    
    
    }   //  static CEstring sToUpper (const wchar_t * szSource)

    static CEString sToUpper (const CEString& sSource)
    {
        wchar_t * szLocale = _wsetlocale (LC_ALL, L"");
        CEString sCopy (sSource);
        sCopy.ToUpper();
    }

    CEString sSubstr (unsigned int uiOffset, unsigned int uiLength = cuiMaxSize_)
    {
        if (uiLength > cuiMaxSize_)
        {
            wchar_t * szMsg = L"Source string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        if (0 == uiLength)
        {
            ERROR_LOG (L"*** Warning: empty string");
            return CEString (L"");
        }

        if (L'\0' != m_szData[m_uiLength])
        {
            wchar_t * szMsg = L"Source string not null-terminated.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (uiOffset >= m_uiLength)
        {
            wchar_t * szMsg = L"Invalid offset.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (uiLength < cuiMaxSize_)
        {
            if (uiOffset + uiLength > m_uiLength)
            {
                wchar_t * szMsg = L"Invalid length.";
                ERROR_LOG (szMsg);
                throw CException (E_UNEXPECTED, szMsg);
            }
        }

        CEString sResult;
        unsigned int uiCharsToMove = min (uiLength, m_uiLength - uiOffset);
        ATLASSERT (uiCharsToMove <= m_uiLength);
        wchar_t chrSave = m_szData[uiOffset + uiCharsToMove];
        m_szData[uiOffset + uiCharsToMove] = L'\0';
        sResult.Assign (&m_szData[uiOffset], uiCharsToMove);
        m_szData[uiOffset + uiCharsToMove] = chrSave;

        return sResult;

    }   //  CEString sSubstr (...)

    // Trim
    void TrimLeft (const wchar_t * szCharsToTrim)
    {
        if (m_uiLength >= (m_uiBlocksAllocated * uiBlockSize_))
        {
            wchar_t * szMsg = L"Illegal length.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (L'\0' != m_szData[m_uiLength])
        {
            wchar_t * szMsg = L"Malformed string.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        unsigned int uiAt = 0;
        for (; uiAt < m_uiLength; ++uiAt)
        {
            if (L'\0' == m_szData[uiAt])
            {
                wchar_t * szMsg = L"Malformed string.";
                ERROR_LOG (szMsg);
                throw CException (E_UNEXPECTED, szMsg);
            }

            if (!bIn (m_szData[uiAt], szCharsToTrim))
            {
                break;
            }
        }

        if (0 == uiAt)
        {
            return;
        }

        if (uiAt >= m_uiLength)
        {
            m_szData[0] = L'\0';
            m_uiLength = 0;
            Shrink();
            return;
        }

        m_uiLength -= uiAt;
        errno_t error = wmemmove_s (m_szData, m_uiLength, &m_szData[uiAt], m_uiLength); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }
        m_szData[m_uiLength] = L'\0';
    
    }       // TrimLeft (...)

    void TrimLeft()
    {
        TrimLeft (m_Breaks.szGet());
    }

    void TrimRight (const wchar_t * szCharsToTrim)
    {
        if (m_uiLength >= m_uiBlocksAllocated * uiBlockSize_)
        {
            wchar_t * szMsg = L"Illegal length.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (L'\0' != m_szData[m_uiLength])
        {
            wchar_t * szMsg = L"Malformed string.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        int iAt = (int)m_uiLength-1;
        for (; iAt >= 0; --iAt)
        {
            if (L'\0' == m_szData[iAt])
            {
                wchar_t * szMsg = L"Malformed string.";
                ERROR_LOG (szMsg);
                throw CException (E_UNEXPECTED, szMsg);
            }

            if (!bIn (m_szData[iAt], szCharsToTrim))
            {
                break;
            }
        }

        if (iAt == (int)m_uiLength-1)
        {
            return;
        }

        m_uiLength = (int)iAt+1;
        m_szData[m_uiLength] = L'\0';
        
        Shrink();

    }   //  void TrimRight()

    void TrimRight()
    {
        TrimRight (m_Breaks.szGet());
    }

    void Trim (wchar_t * szCharsToTrim)
    {
        TrimLeft (szCharsToTrim);
        TrimRight (szCharsToTrim);
    }

    void Trim()
    {
        TrimLeft (m_Breaks.szGet());
        TrimRight (m_Breaks.szGet());
    }

    void Reverse()
    {
        _wcsrev (m_szData);
    }


    // Tokens & fields
    CEString sGetField (int iAt, ETokenType eType = ecTokenText)
    {
// TODO exc handling
        vector<StToken>::iterator itToken = itFindToken (iAt, eType);
        if (m_vecTokens.end() == itToken)
        {
            wchar_t * szMsg = L"Failed to find token.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        return sSubstr ((*itToken).uiOffset, (*itToken).uiLength);
    
    }   // sGetField (...)


    StToken stGetField (int iAt, ETokenType eType = ecTokenText)
    {
// TODO exc handling
        vector<StToken>::iterator itToken = itFindToken (iAt, eType);
        if (m_vecTokens.end() == itToken)
        {
            wchar_t * szMsg = L"Failed to find token.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        return *itToken;
    
    }   //  stGetField (...)

//    ST_Token st_GetFieldFromOffset (int i_offset,
//                                    et_TokenType eo_type = ec_TokenText);

    StToken stGetTokenFromOffset (int iOffset)
    {
        vector<StToken>::iterator it_ = itTokenFromOffset (iOffset);
        return (*it_);
    }

    ETokenType eGetTokenType (unsigned int uiAt)
    {
        Tokenize();

        if (uiAt >= m_vecTokens.size())
        {
            wchar_t * szMsg = L"Token index out of range.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        return m_vecTokens[uiAt].eType;
    
    }   //  eGetTokenType (...)

//    et_TokenType eo_GetTokenType (int i_offset, int i_at);

    StToken stGetToken (unsigned int uiAt)
    {
        Tokenize();

        if (uiAt >= m_vecTokens.size())
        {
            wchar_t * szMsg = L"Token index out of range.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        return m_vecTokens[uiAt];
    
    }   //  stGetToken (...)

    const StToken& rstGetToken (unsigned int uiAt)
    {
        Tokenize();

        if (uiAt >= m_vecTokens.size())
        {
            wchar_t * szMsg = L"Token index out of range.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        return m_vecTokens[uiAt];
    
    }   //  const StToken& rstGetToken (...)

    CEString sGetToken (unsigned int uiAt)
    {
        Tokenize();

        if (uiAt >= m_vecTokens.size())
        {
            wchar_t * szMsg = L"Token index out of range.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        StToken stToken = m_vecTokens[uiAt];
        return CEString (sSubstr (stToken.uiOffset, stToken.uiLength));
    
    }   //  CEString sGetToken (...)

    bool bGetNextToken (StToken& stToken)
    {
        Tokenize();

        vector<StToken>::iterator it_ = find (m_vecTokens.begin(), m_vecTokens.end(), stToken);
        if (m_vecTokens.end() == it_)
        {
            wchar_t * szMsg = L"Token not found.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        ++it_;
        if (m_vecTokens.end() == it_)
        {
            return false;
        }
        else
        {
            stToken = *it_;
        }

        return true;

    }    //  bool bGetNextToken (...)

    bool bGetPrevToken (StToken& stToken)
    {
        Tokenize();

        vector<StToken>::iterator it_ = find (m_vecTokens.begin(), m_vecTokens.end(), stToken);
        if (m_vecTokens.end() == it_)
        {
            wchar_t * szMsg = L"Token not found.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        if (m_vecTokens.begin() == it_)
        {
            return false;
        }

        --it_;
        stToken = *it_;

        return true;

    }    //  bool bGetPrevToken (...)

    unsigned int uiGetTokenNum (const StToken& stToken)
    {
        Tokenize();

        vector<StToken>::iterator it_ = find (m_vecTokens.begin(), m_vecTokens.end(), stToken);
        if (it_ == m_vecTokens.end())
        {
            wchar_t * szMsg = L"Token not found.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        return it_ - m_vecTokens.begin();
    
    }   //  unsigned int uiGetTokenNum (...)

    unsigned int uiGetTokenOffset (unsigned int uiAt) const
    {
        if (uiAt >= m_vecTokens.size())
        {
            wchar_t * szMsg = L"m_vecTokens member index out of range";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        return m_vecTokens[uiAt].uiOffset;
    }

    unsigned int uiGetRegexOffset (unsigned int uiAt) const
    {
        if (uiAt >= m_vecRegexMatches.size())
        {
            wchar_t * szMsg = L"m_vecRegexMatches member index out of range";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        return m_vecRegexMatches[uiAt].uiOffset;
    }

    CEString sGetRegexMatch (unsigned int iAt)
    {
        if (wcslen (m_szRegex))
        {
            ERROR_LOG (L"Warning: Empty regex string");
            return CEString (L"");
        }

        if (iAt >= m_vecRegexMatches.size())
        {
            wchar_t * szMsg = L"m_vecRegexMatches member index out of range";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        StToken stToken = m_vecRegexMatches[iAt];
        return sSubstr (stToken.uiOffset, stToken.uiLength);

    }   //  sGetRegexMatch (...)

    unsigned int uiGetNumOfFields (ETokenType eType = ecTokenText)
    {
        Tokenize();

        int iTokens = 0;
        vector<StToken>::iterator it_ = m_vecTokens.begin();
        for (; it_ != m_vecTokens.end(); ++it_)
        {
            if ((*it_).eType == eType)
            {
                ++iTokens;
            }
        }
        
        return iTokens;
    
    }   //  uiGetNumOfFields (...)

/*
    unsigned int uiGetNumOfFields (unsigned int uiOffset, 
                                   unsigned int uiLength,
                                   ETokenType eType = ecTokenText)
    {
        Tokenize();

        vector<StToken>::iterator it_ = m_vecTokens.begin();
        for (; it_ < m_vecTokens.end(); ++it_)
        {
            if ((*it_).uiOffset >= uiOffset)
            {
                break;
            }
        }
        
        int iTokens = 0;
        for (; it_ != m_vecTokens.end(); ++it_)
        {
            if ((*it_).uiOffset >= uiOffset + uiLength)
            {
                break;
            }

            if ((*it_).eType == eType)
            {
                ++iTokens;
            }
        }
        
        if (m_vecTokens.end() == it_)
        {
            wstring sMsg (L"Token index out of range.");
            ERROR_LOG (sMsg);
            throw CException (E_FAIL, sMsg);
        }

        return iTokens;
    
    }   //  unsigned int uiGetNumOfFields (unsigned int uiOffset, ...)
*/

    unsigned int uiNFields (ETokenType eType = ecTokenText)
    {
        return uiGetNumOfFields (eType);
    }

//    unsigned int uiNFields (unsigned int uiOffset, 
//                            unsigned int uiLength, 
//                            ETokenType eType = ecTokenText)
//    {
//        return uiGetNumOfFields (uiOffset, uiLength, eType);
//    }

    unsigned int uiGetNumOfTokens()
    {
        Tokenize();
        return m_vecTokens.size();
    }

    unsigned int uiNTokens()
    {
        Tokenize();
        return m_vecTokens.size();
    }

    unsigned int uiGetVisibleLength()
    {
        Tokenize();

        unsigned int uiVlength = 0;
        vector<StToken>::iterator it_ = m_vecTokens.begin();
        for (; it_ != m_vecTokens.end(); ++it_)    
        {
            if ((*it_).bIsLinearText())
            {
                uiVlength += (*it_).uiLength;
            }
        }
        return uiVlength;
    
    }   //  uiGetVisibleLength()

    unsigned int uiGetFieldLength (unsigned int uiAt, ETokenType eType = ecTokenText)
    {
        Tokenize();

        if (uiAt >= m_vecTokens.size())
        {
            wchar_t * szMsg = L"Token index out of range.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        vector<StToken>::iterator it = m_vecTokens.begin();
        for (; it != m_vecTokens.end(); ++it)
        {
            if ((*it).eType == eType)
            {
                if (distance (m_vecTokens.begin(), it) >= (int)uiAt)
                {
                    break;
                }
            }
        }
        
        if (m_vecTokens.end() == it)
        {
            wchar_t * szMsg = L"Token not found.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        return (*it).uiLength;

    }   //  uiGetFieldLength (...)

    //unsigned int uiGetFieldLength (unsigned int uiOffset, 
    //                               unsigned int uiAt,
    //                               ETokenType eType = ecTokenText)
    //{
    //}

    unsigned int uiGetNumOfSyllables() const
    {
        if (m_Vowels.uiLength() == 0)
        {
            ATLASSERT(0);
            wchar_t * szMsg = L"Vowels not defined";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        unsigned int uiSyllables = 0;
        unsigned int uiAt = 0;
        do
        {
            unsigned int uiStartAt = uiAt;
            uiAt = wcscspn (&m_szData[uiAt], m_Vowels.szGet());
            uiAt += uiStartAt;
            if (uiAt < m_uiLength)
            {
                ++uiSyllables;
            }

        } while (++uiAt < m_uiLength);

        return uiSyllables;

    }   //  uiGetNumOfSyllables()

    int uiNSyllables() const
    {
        return uiGetNumOfSyllables();
    }

    // Vowels & consonants
    unsigned int uiGetVowelPos (unsigned int uiVowel = 0) const
    {
        if (m_Vowels.uiLength() == 0)
        {
            ATLASSERT(0);
            wchar_t * szMsg = L"Vowels not defined";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        unsigned int uiAt = 0;
        bool bFound = false;
        for (unsigned int ui_ = 0; ui_ <= uiVowel; ++ui_)
        {
            unsigned int uiStartAt = uiAt;
            uiAt = wcscspn (&m_szData[uiAt], m_Vowels.szGet());
            uiAt += uiStartAt;
            if (uiAt >= m_uiLength)
            {
                break;
            }
            if (uiVowel == ui_)
            {
                return uiAt;
            }
            ++uiAt;
        }

        ATLASSERT(0);
        wchar_t * szMsg = L"Vowel position not found";
        ERROR_LOG (szMsg);
        throw CException (E_FAIL, szMsg);
    
    }   //  uiGetVowelPos (...)
    
    unsigned int uiGetSyllableFromVowelPos (unsigned int uiAbsPos) const
    {
        if (m_Vowels.uiLength() == 0)
        {
            ATLASSERT(0);
            wchar_t * szMsg = L"Vowels not defined";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        if (m_uiLength <= uiAbsPos)
        {
            ATLASSERT(0);
            wchar_t * szMsg = L"Vowel position invalid";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        unsigned int uiSyll = 0;
        unsigned int uiVowelPos = 0;
        do
        {
            unsigned int uiStartAt = uiVowelPos;
            uiVowelPos = wcscspn (&m_szData[uiVowelPos], m_Vowels.szGet());
            uiVowelPos += uiStartAt;
            if (uiVowelPos < m_uiLength)
            {
                if (uiVowelPos == uiAbsPos)
                {
                    return uiSyll;
                }

                ++uiSyll;

                if (++uiVowelPos > uiAbsPos)
                {
                    ATLASSERT(0);
                    wchar_t * szMsg = L"Vowel position not found";
                    ERROR_LOG (szMsg);
                    throw CException (E_FAIL, szMsg);
                }
            }

        } while (uiVowelPos < m_uiLength);

        ATLASSERT(0);
        wchar_t * szMsg = L"Vowel position not found";
        ERROR_LOG (szMsg);
        throw CException (E_FAIL, szMsg);

    }   //  uiGetSyllableFromVowelPos (...)

    // Conversions
    template<typename T>
    static CEString sToString (T tSource)
    {
        wchar_t arrStrValue[cuiMaxConversionLength_];
        errno_t iError = _itow_s (tSource, arrStrValue, cuiMaxConversionLength_, 10);
        if (iError)
        {
            wchar_t * szMsg = L"Conversion error.";
            ERROR_LOG (szMsg);
            throw CException (iError, szMsg);
        }
        return CEString (arrStrValue);
    }

    template<>
    static CEString sToString (__int64 llSource)
    {
        wchar_t arrStrValue[cuiMaxConversionLength_];
        errno_t iError = _i64tow_s (llSource, arrStrValue, cuiMaxConversionLength_, 10);
        if (iError)
        {
            wchar_t * szMsg = L"Conversion error.";
            ERROR_LOG (szMsg);
            throw CException (iError, szMsg);
        }
        return CEString (arrStrValue);
    }

    template<>
    CEString sToString (unsigned __int64 ullSource)
    {
        wchar_t arrStrValue[cuiMaxConversionLength_];
        errno_t iError = _ui64tow_s (ullSource, arrStrValue, cuiMaxConversionLength_, 10);
        if (iError)
        {
            wchar_t * szMsg = L"Conversion error.";
            ERROR_LOG (szMsg);
            throw CException (iError, szMsg);
        }
    }

    template<>
    CEString sToString (double dSource)
    {
        wchar_t arrStrValue[cuiMaxConversionLength_];
        errno_t iError = _gcvt_s ((char *)arrStrValue, sizeof (arrStrValue), dSource, 10); 
        if (iError)
        {
            wchar_t * szMsg = L"Conversion error.";
            ERROR_LOG (szMsg);
            throw CException (iError, szMsg);
        }
    }

    // Regex
    bool bRegexMatch (const wchar_t * szRegex)
    {
        return bRegexEvaluate (szRegex, true);
    }

    bool bRegexSearch (const wchar_t * szRegex)
    {
        return bRegexEvaluate (szRegex, false);
    }


// TODO RegexReplace?

////////////////////////////////////////////////////////////////////////////////////////////


private:

//    void Null()
//    {
//        m_szRegex[0] = L'\0';
//          m_szVowels[0] = L'\0';
//        m_szBreakChars[0] = L'\0';
//        m_szTabs[0] = L'\0';
//        m_szPunctuation[0] = L'\0';
//        m_szEscapeChars[0] = L'\0';
//    }

    void Grow (unsigned int uiCharsToAdd)
    {
        unsigned int uiNewLength = m_uiLength + uiCharsToAdd;
        unsigned int uiNewBlocks = (uiNewLength+1)/uiBlockSize_;
        ATLASSERT (uiNewBlocks >= m_uiBlocksAllocated);
        if (uiNewBlocks == m_uiBlocksAllocated)
        {
            return;
        }
        wchar_t * szNewBuffer = new wchar_t[uiNewBlocks * uiBlockSize_];
        if (m_uiLength > 0)
        {
            errno_t error = wmemmove_s (szNewBuffer, m_uiLength, m_szData, m_uiLength); 
            if (error)
            {
                wchar_t * szMsg = L"wmemmove_s failed.";
                ERROR_LOG (szMsg);
                throw CException (error, szMsg);
            }
            szNewBuffer[m_uiLength] = L'\0';
        }
        delete[] m_szData;
        m_szData = szNewBuffer;
    
    }   //  Grow()

    void Shrink()
    {
        unsigned int uiBlocksToFree = ((m_uiBlocksAllocated * uiBlockSize_) - m_uiLength) / uiBlockSize_;
        if (uiBlocksToFree < 2)
        {
            return;
        }

        m_uiBlocksAllocated -= uiBlocksToFree;
        if (m_uiBlocksAllocated > 0)
        {
            wchar_t * szNewBuffer = new wchar_t[m_uiBlocksAllocated * uiBlockSize_];
            errno_t error = wmemmove_s (szNewBuffer, m_uiLength, m_szData, m_uiLength); 
            if (error)
            {
                wchar_t * szMsg = L"wmemmove_s failed.";
                ERROR_LOG (szMsg);
                throw CException (error, szMsg);
            }
            szNewBuffer[m_uiLength] = L'\0';
            delete[] m_szData;
            m_szData = szNewBuffer;
        }
        else
        {
            delete[] m_szData;
            m_szData = NULL;
        }
    
    }   //  Shrink()

    void Concatenate (const wchar_t * szRhs, unsigned int uiRhsLength)
    {
        unsigned int uiNewLength = m_uiLength + uiRhsLength;
        if (uiNewLength >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Right-hand side string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        if (uiNewLength >= m_uiBlocksAllocated * uiBlockSize_)
        {
            m_uiBlocksAllocated = ((uiNewLength+1)/uiBlockSize_) + 1;
            wchar_t * szNewData = new wchar_t[m_uiBlocksAllocated * uiBlockSize_];
            errno_t error = wmemmove_s (szNewData, m_uiLength, m_szData, m_uiLength); 
            if (error)
            {
                wchar_t * szMsg = L"wmemmove_s error.";
                ERROR_LOG (szMsg);
                throw CException (error, szMsg);
            }
            delete[] m_szData;
            m_szData = szNewData;
        }

        errno_t error = wmemmove_s (&m_szData[m_uiLength], uiRhsLength, szRhs, uiRhsLength); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        m_uiLength = uiNewLength;
        m_szData[m_uiLength] = L'\0';

    }   //  void Concatenate (...)

    void Tokenize()
    {
        if (!m_bInvalid)
        {
            return;
        }

        m_bInvalid = false;
        m_vecTokens.clear();

        StToken stToken;
        for (unsigned int uiAt = 0; uiAt < m_uiBlocksAllocated * uiBlockSize_; ++uiAt)
        {
            wchar_t chrCurrent = m_szData[uiAt];

            if (L'\0' == chrCurrent)
            {
                if (uiAt != m_uiLength)
                {
                    m_bInvalid = true;
                    wchar_t * szMsg = L"Unexpected NULL character.";
                    ERROR_LOG (szMsg);
                    throw CException (E_UNEXPECTED, szMsg);
                }

                if (0 == uiAt)
                {
                    return;
                }

                if (ecTokenTypeFront == stToken.eType)
                {
                    m_bInvalid = true;
                    wchar_t * szMsg = L"Illegal token type.";
                    ERROR_LOG (szMsg);
                    throw CException (E_UNEXPECTED, szMsg);
                }

                m_vecTokens.push_back (stToken);
                return;
            }

            if (bIn (chrCurrent, m_Breaks.szGet()))
            {
                Advance (ecTokenBreakChars, uiAt, stToken);
                continue;
            }

            if (bIn (chrCurrent, m_Tabs.szGet()))
            {
                Advance (ecTokenTab, uiAt, stToken);
                continue;
            }

            if (bIn (chrCurrent, m_Punctuation.szGet()))
            {
                Advance (ecTokenPunctuation, uiAt, stToken);
                continue;
            }

            if (bIn (chrCurrent, m_Escape.szGet()))
            {

                ATLASSERT(m_uiLength > 0);
                if (uiAt >= m_uiLength-1)
                {
                    m_bInvalid = true;
                    wchar_t * szMsg = L"Unexpected escape character.";
                    ERROR_LOG (szMsg);
                    throw CException (E_UNEXPECTED, szMsg);
                }

                bool bDoubleEscape = false;
                if (chrCurrent == m_szData[uiAt+1])
                {
                    bDoubleEscape = true;
                }
                else if (uiAt > 0 && (chrCurrent == m_szData[uiAt-1]))
                {
                    bDoubleEscape = true;
                }

                if (!bDoubleEscape)
                {
                    AddTag (uiAt, stToken);
                    continue;
                }

            }   //  for (int uiAt = 0; ...)

            Advance (ecTokenText, uiAt, stToken);
            
        }   //  for ...

        m_bInvalid = true;
        wchar_t * szMsg = L"Tokenizer failed.";
        ERROR_LOG (szMsg);
        throw CException (E_UNEXPECTED, szMsg);

    }   //  Tokenize_ (...)

    void Advance (ETokenType eType, unsigned int uiOffset, StToken& stToken)
    {
        if (eType <= ecTokenTypeFront || eType > ecTokenTypeBack)
        {
            wchar_t * szMsg = L"Unexpected token state.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (eType != stToken.eType)
        {
            if (ecTokenTypeFront != stToken.eType)
            {
                m_vecTokens.push_back (stToken);
            }

            stToken.eType = eType;
            stToken.uiOffset = uiOffset;
            stToken.uiLength = 1;
        }
        else
        {
            ++stToken.uiLength;
        }
    
    }   // void Advance (...)

    void AddTag (unsigned int uiOffset, StToken& stToken)
    {
        if (ecTokenMeta == stToken.eType || uiOffset < 0)
        {
            wchar_t * szMsg = L"Unexpected token state.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (ecTokenTypeFront != stToken.eType)
        {
            m_vecTokens.push_back (stToken);
        }

        stToken.eType = ecTokenMeta;
        stToken.uiOffset = uiOffset;
        stToken.uiLength = 0;

        for (int iAt = uiOffset + 1; iAt < (int)(m_uiBlocksAllocated * uiBlockSize_); ++iAt)
        {
            if (L'\0' == m_szData[iAt])
            {
                wchar_t * szMsg = L"Unterminated escape sequence.";
                ERROR_LOG (szMsg);
                throw CException (E_UNEXPECTED, szMsg);
            }

            if (bIn (m_szData[iAt], m_Escape.szGet()))
            {
                if (uiOffset+1 == iAt)
                {
                    wchar_t * szMsg = L"Empty escape sequence.";
                    ERROR_LOG (szMsg);
                    throw CException (E_UNEXPECTED, szMsg);
                }
                stToken.uiLength = iAt - uiOffset + 1;
                m_vecTokens.push_back (stToken);
                break;
            }
        }
    }       //  AddTag (...)

    // Assign null-terminated string erasing current contents
    // uiSourceLength does not count final '0'
    void Assign (const wchar_t * szSource, unsigned int uiSourceLength)
    {
        if (uiSourceLength >= cuiMaxSize_)
        {
            wchar_t * szMsg = L"Source string too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        if (L'\0' != szSource[uiSourceLength])
        {
            wchar_t * szMsg = L"Source string not null-terminated.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        if (uiSourceLength >= (m_uiBlocksAllocated * uiBlockSize_))
        {
            m_uiBlocksAllocated = ((uiSourceLength+1)/uiBlockSize_) + 1;
            if (m_szData)
            {
                delete[] m_szData;
            }
            m_szData = new wchar_t[uiBlockSize_ * m_uiBlocksAllocated];
        }

        errno_t error = wmemmove_s (m_szData, uiSourceLength, szSource, uiSourceLength); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        m_szData[uiSourceLength] = L'\0';
        m_uiLength = uiSourceLength;
    
    }   //  Assign (...)

    vector<StToken>::iterator itFindToken (unsigned int uiAt, ETokenType eType = ecTokenText)
    {
        Tokenize();

        if (uiAt >= m_vecTokens.size())
        {
            wchar_t * szMsg = L"Token position out of range.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        unsigned int uiField = 0;
        vector<StToken>::iterator it_ = m_vecTokens.begin();
        for (; it_ != m_vecTokens.end(); ++it_)
        {
            if (eType == (*it_).eType)
            {
                if (uiAt == uiField)
                {
                    break;
                }
                ++uiField;
            }
        }

        if (m_vecTokens.end() == it_)
        {
            wchar_t * szMsg = L"Token position out of range.";
            ERROR_LOG (szMsg);
            throw CException (E_UNEXPECTED, szMsg);
        }

        return it_;

    }   //  uiFindToken (...)

    vector<StToken>::iterator itTokenFromOffset (unsigned int uiOffset)
    {
        Tokenize();

        vector<StToken>::iterator it_ = m_vecTokens.begin();
        for (; it_ != m_vecTokens.end(); ++it_)
        {
            if ((*it_).uiOffset > uiOffset)
            {
                break;
            }
        }

        if (m_vecTokens.begin() == it_)
        {
            wchar_t * szMsg = L"Failed to find token.";
            ERROR_LOG (szMsg);
            throw CException (E_FAIL, szMsg);
        }

        return --it_;

    }   //  itTokenFromOffset (...)

    bool bRegexEvaluate (const wchar_t * szRegex, bool bMatchMode = true)
    {
        unsigned int uiRegexLength = wcslen (szRegex);
        if (uiRegexLength > cuiMaxRegexLength_)
        {
            wchar_t * szMsg = L"Regular expression too long.";
            ERROR_LOG (szMsg);
            throw CException (E_INVALIDARG, szMsg);
        }

        errno_t error = wmemmove_s (m_szRegex, uiRegexLength, szRegex, uiRegexLength); 
        if (error)
        {
            wchar_t * szMsg = L"wmemmove_s error.";
            ERROR_LOG (szMsg);
            throw CException (error, szMsg);
        }

        m_szRegex[uiRegexLength] = L'\0';

        bool bRet = false;

        try
        {
            tr1::wregex regex_ (m_szRegex);
            tr1::wsmatch match_;
            wstring wstrData (m_szData);
            if (bMatchMode)
            {
                bRet = tr1::regex_match (wstrData, match_, regex_);
            }
            else
            {
                bRet = tr1::regex_search (wstrData, match_, regex_);
            }

            m_vecRegexMatches.clear();
            for (unsigned int uiAt = 1; uiAt < match_.size(); ++uiAt)
            {
                StToken stToken;
                stToken.eType = ecTokenRegexMatch;
                stToken.uiOffset = static_cast <int> (match_.position (uiAt));
                stToken.uiLength = match_.length (uiAt);
                m_vecRegexMatches.push_back (stToken);
            }
        }
        catch (tr1::regex_error rxError_)
        {
            wstring str_msg (L"Regex error: ");
            str_msg += str_RegexError_ (rxError_.code());
            ERROR_LOG (str_msg.c_str());
// TODO: rethrow as native exc?
        }    

        return bRet;

    }   //  RegexEvaluate (...)

    wstring str_RegexError_ (tr1::regex_constants::error_type eo_errCode)
    {
        wstring str_error;
        switch (eo_errCode)
        {
            case tr1::regex_constants::error_badbrace:
            {
                str_error = L"error_badbrace -- ";
                str_error += L"the expression contained an invalid count in a { } expression";
                break;
            }
            case tr1::regex_constants::error_badrepeat:
            {
                str_error = L"error_badrepeat -- ";
                str_error += L"a repeat expression (one of '*', '?', '+', '{' ";
                str_error += L"in most contexts) was not preceded by an expression";
                break;
            }
            case tr1::regex_constants::error_brace:
            {
                str_error = L"error_brace -- the expression contained an unmatched '{' or '}'";
                break;
            }
            case tr1::regex_constants::error_brack:
            {
                str_error = L"error_brack -- the expression contained an unmatched '[' or ']'"; 
                break;
            }
            case tr1::regex_constants::error_collate:
            {
                str_error = L"error_collate -- ";
                str_error += L"the expression contained an invalid collating element name";
                break;
            }
            case tr1::regex_constants::error_complexity:
            {
                str_error = L"error_complexity -- an attempted match failed because it was too complex";
                break;
            }
            case tr1::regex_constants::error_ctype:
            {
                str_error = L"error_ctype -- the expression contained an invalid character class name";
                break;
            }
            case tr1::regex_constants::error_escape:
            {
                str_error = L"error_escape -- the expression contained an invalid escape sequence";
                break;
            }
            case tr1::regex_constants::error_paren:
            {
                str_error = L"error_paren -- the expression contained an unmatched '(' or ')'";
                break;
            }
            case tr1::regex_constants::error_range:
            {
                str_error = L"error_range -- ";
                str_error += L"the expression contained an invalid character range specifier";
                break;
            }
            case tr1::regex_constants::error_space:
            {
                str_error = L"error_space -- ";
                str_error += L"parsing a regular expression failed because there were not enough ";
                str_error += L"resources available";
                break;
            }
            case tr1::regex_constants::error_stack:
            {
                str_error = L"error_stack -- ";
                str_error += L"an attempted match failed because there was not enough memory available";
                break;
            }
            case tr1::regex_constants::error_backref:
            {
                str_error = L"error_backref -- the expression contained an invalid back reference";
                break;
            }
            case tr1::regex_constants::error_parse:
            {
                str_error = L"error_parse";
                break;
            }
            case tr1::regex_constants::error_syntax:
            {
                str_error = L"error_syntax";
                break;
            }
            default:
            {
                str_error = L"Unknown error";
            }
        }
            return str_error;

    }   //  str_RegexError_ (...)

friend const CEString operator+ (const CEString& sLhs, const wchar_t * szRhs)
{
    return CEString (sLhs) += szRhs;
}

friend const CEString operator+ (const wchar_t * szLhs, const CEString& sRhs)
{
    return CEString (szLhs) += sRhs;
}

friend const CEString operator+ (const CEString& sLhs, const CEString& sRhs)
{
    return CEString (sLhs) += sRhs;
}

friend bool operator== (const CEString& sLhs, const wchar_t * szRhs)
{
    return ecEqual == CEString::eCompare (sLhs, szRhs);
}

friend bool operator== (const wchar_t * szLhs, const CEString& sRhs)
{
    return ecEqual == CEString::eCompare (szLhs, sRhs);
}

friend bool operator== (const CEString& sLhs, const CEString& sRhs)
{
    return ecEqual == CEString::eCompare (sLhs, sRhs);
}

friend bool operator!= (const CEString& sLhs, const wchar_t * szRhs)
{return ecEqual != CEString::eCompare (sLhs, szRhs);
}

friend bool operator!= (const wchar_t * szLhs, const CEString& sRhs)
{
    return ecEqual != CEString::eCompare (szLhs, sRhs);
}

friend bool operator!= (const CEString& sLhs, const CEString& sRhs)
{
    return ecEqual != CEString::eCompare (sLhs, sRhs);
}

friend bool operator< (const CEString& sLhs, const wchar_t * szRhs)
{
    return ecLess == CEString::eCompare (sLhs, szRhs);
}

friend bool operator< (const wchar_t * szLhs, const CEString& sRhs)
{
    return ecLess == CEString::eCompare (szLhs, sRhs);
}

friend bool operator< (const CEString& sLhs, const CEString& sRhs)
{
    return ecLess == CEString::eCompare (sLhs, sRhs);
}

friend bool operator> (const CEString& sLhs, const wchar_t * szRhs)
{
    return ecGreater == CEString::eCompare (sLhs, szRhs);
}

friend bool operator> (const wchar_t * szLhs, const CEString& sRhs)
{
    return ecGreater == CEString::eCompare (szLhs, sRhs);
}

friend bool operator> (const CEString& sLhs, const CEString& sRhs)
{
    return ecGreater == CEString::eCompare (sLhs, sRhs);
}

friend bool operator<= (const CEString& sLhs, const wchar_t * szRhs)
{
    ERelation eRet = CEString::eCompare (sLhs, szRhs);
    return (ecLess == eRet || ecEqual == eRet);
}

friend bool operator<= (const wchar_t * szLhs, const CEString& sRhs)
{
    ERelation eRet = CEString::eCompare (szLhs, sRhs);
    return (ecLess == eRet || ecEqual == eRet);
}

friend bool operator<= (const CEString& sLhs, const CEString& sRhs)
{
    ERelation eRet = CEString::eCompare (sLhs, sRhs);
    return (ecLess == eRet || ecEqual == eRet);
}

friend bool operator>= (const CEString& sLhs, const wchar_t * szRhs)
{
    ERelation eRet = CEString::eCompare (sLhs, szRhs);
    return (ecGreater == eRet || ecEqual == eRet);
}

friend bool operator>= (const wchar_t * szLhs, const CEString& sRhs)
{
    ERelation eRet = CEString::eCompare (szLhs, sRhs);
    return (ecGreater == eRet || ecEqual == eRet);
}

friend bool operator>= (const CEString& sLhs, const CEString& sRhs)
{
    ERelation eRet = CEString::eCompare (sLhs, sRhs);
    return (ecGreater == eRet || ecEqual == eRet);
}


};   //  class  CEString

/*
const CEString operator+ (const CEString& sLhs, const wchar_t * szRhs)
{
    return CEString (sLhs) += szRhs;
}

const CEString operator+ (const wchar_t * szLhs, const CEString& sRhs)
{
    return CEString (szLhs) += sRhs;
}

const CEString operator+ (const CEString& sLhs, const CEString& sRhs)
{
    return CEString (sLhs) += sRhs;
}

bool operator== (const CEString& sLhs, const wchar_t * szRhs)
{
    return ecEqual == CEString::eCompare (sLhs, szRhs);
}

bool operator== (const wchar_t * szLhs, const CEString& sRhs)
{
    return ecEqual == CEString::eCompare (szLhs, sRhs);
}

bool operator== (const CEString& sLhs, const CEString& sRhs)
{
    return ecEqual == CEString::eCompare (sLhs, sRhs);
}

bool operator!= (const CEString& sLhs, const wchar_t * szRhs)
{
    return ecEqual != CEString::eCompare (sLhs, szRhs);
}

bool operator!= (const wchar_t * szLhs, const CEString& sRhs)
{
    return ecEqual != CEString::eCompare (szLhs, sRhs);
}

bool operator!= (const CEString& sLhs, const CEString& sRhs)
{
    return ecEqual != CEString::eCompare (sLhs, sRhs);
}

bool operator< (const CEString& sLhs, const wchar_t * szRhs)
{
    return ecLess == CEString::eCompare (sLhs, szRhs);
}

bool operator< (const wchar_t * szLhs, const CEString& sRhs)
{
    return ecLess == CEString::eCompare (szLhs, sRhs);
}

bool operator< (const CEString& sLhs, const CEString& sRhs)
{
    return ecLess == CEString::eCompare (sLhs, sRhs);
}

bool operator> (const CEString& sLhs, const wchar_t * szRhs)
{
    return ecGreater == CEString::eCompare (sLhs, szRhs);
}

bool operator> (const wchar_t * szLhs, const CEString& sRhs)
{
    return ecGreater == CEString::eCompare (szLhs, sRhs);
}

bool operator> (const CEString& sLhs, const CEString& sRhs)
{
    return ecLess == CEString::eCompare (sLhs, sRhs);
}

bool operator<= (const CEString& sLhs, const wchar_t * szRhs)
{
    ERelation eRet = CEString::eCompare (sLhs, szRhs);
    return (ecLess == eRet || ecEqual == eRet);
}

bool operator<= (const wchar_t * szLhs, const CEString& sRhs)
{
    ERelation eRet = CEString::eCompare (szLhs, sRhs);
    return (ecLess == eRet || ecEqual == eRet);
}

bool operator<= (const CEString& sLhs, const CEString& sRhs)
{
    ERelation eRet = CEString::eCompare (sLhs, sRhs);
    return (ecLess == eRet || ecEqual == eRet);
}

bool operator>= (const CEString& sLhs, const wchar_t * szRhs)
{
    ERelation eRet = CEString::eCompare (sLhs, szRhs);
    return (ecGreater == eRet || ecEqual == eRet);
}

bool operator>= (const wchar_t * szLhs, const CEString& sRhs)
{
    ERelation eRet = CEString::eCompare (szLhs, sRhs);
    return (ecGreater == eRet || ecEqual == eRet);
}

bool operator>= (const CEString& sLhs, const CEString& sRhs)
{
    ERelation eRet = CEString::eCompare (sLhs, sRhs);
    return (ecGreater == eRet || ecEqual == eRet);
}
*/
