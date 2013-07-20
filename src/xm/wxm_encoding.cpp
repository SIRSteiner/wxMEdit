///////////////////////////////////////////////////////////////////////////////
// vim:         ts=4 sw=4
// Name:        wxmedit/wxm_encoding.cpp
// Description: define the Encodings which are supported by wxMEdit
// Author:      wxmedit@gmail.com
// Licence:     GPL
///////////////////////////////////////////////////////////////////////////////

#include "wxm_encoding.h"
#include <wx/config.h>
#include <wx/fontmap.h>
#include <boost/foreach.hpp>

#ifdef _DEBUG
#include <crtdbg.h>
#define new new(_NORMAL_BLOCK ,__FILE__, __LINE__)
#endif

#ifdef __WXMSW__
#define MSW_GET_FONT_NAME(cp,fn) MSW_GetFontName(cp,fn)

// get fontname from registry mime database
void MSW_GetFontName(const wxString codepage, wxString &fontname)
{
	const wxString MIMEDB(wxT("HKEY_CLASSES_ROOT\\MIME\\Database\\Codepage\\"));
	wxRegKey *pRegKey = new wxRegKey(MIMEDB + codepage);

	if(!pRegKey->Exists())
	{
		delete pRegKey;
		return;
	}

	long cp;
	if(pRegKey->QueryValue(wxT("Family"), &cp))
	{
		delete pRegKey;
		pRegKey = new wxRegKey(MIMEDB + wxString::Format(wxT("%d"), cp));
	}

	pRegKey->QueryValue(wxT("FixedWidthFont"), fontname);

	delete pRegKey;
}

#else
#define MSW_GET_FONT_NAME(cp,fn)
#endif

wxString GetMSCPFontName(const wxString mscp)
{
	wxString fontname;
#ifdef __WXMSW__
    fontname=wxT("Courier New");
#else
    fontname=wxT("Monospace");
#endif

	if (mscp.IsEmpty())
		return fontname;

	MSW_GET_FONT_NAME(mscp, fontname);
	return fontname;
}

namespace wxm
{
ssize_t WXMEncoding::ms_sysenc_idx = -1;
WXMEncoding* WXMEncoding::ms_sysenc = NULL;

//typedef std::map<std::string, wxFontEncoding> WXEncMap;
WXMEncoding::WXEncMap WXMEncoding::ms_wxenc_map;

std::vector<wxString> WXMEncoding::ms_wxenc_list;
std::vector<std::string> WXMEncoding::ms_enc_list;

WXMEncoding::WXNameEncMap WXMEncoding::ms_wxnameenc_map;
WXMEncoding::WXEncNameMap WXMEncoding::ms_wxencname_map;
WXMEncoding::WXEncTypeMap WXMEncoding::ms_wxenctype_map;
WXMEncoding::WXEncFontMap WXMEncoding::ms_wxencfont_map;
WXMEncoding::WXEncInstMap WXMEncoding::ms_inst_map;

void WXMEncoding::InitEncodings()
{
	wxLogNull nolog;

	ms_wxenc_map["ISO-8859-1"] = wxFONTENCODING_ISO8859_1;
	ms_wxenc_map["ISO-8859-2"] = wxFONTENCODING_ISO8859_2;
	ms_wxenc_map["ISO-8859-3"] = wxFONTENCODING_ISO8859_3;
	ms_wxenc_map["ISO-8859-4"] = wxFONTENCODING_ISO8859_4;
	ms_wxenc_map["ISO-8859-5"] = wxFONTENCODING_ISO8859_5;
	ms_wxenc_map["ISO-8859-6"] = wxFONTENCODING_ISO8859_6;
	ms_wxenc_map["ISO-8859-7"] = wxFONTENCODING_ISO8859_7;
	ms_wxenc_map["ISO-8859-8"] = wxFONTENCODING_ISO8859_8;
	ms_wxenc_map["ISO-8859-9"] = wxFONTENCODING_ISO8859_9;
	ms_wxenc_map["ISO-8859-10"] = wxFONTENCODING_ISO8859_10;
	ms_wxenc_map["ISO-8859-11"] = wxFONTENCODING_ISO8859_11;
	ms_wxenc_map["ISO-8859-13"] = wxFONTENCODING_ISO8859_13;
	ms_wxenc_map["ISO-8859-14"] = wxFONTENCODING_ISO8859_14;
	ms_wxenc_map["ISO-8859-15"] = wxFONTENCODING_ISO8859_15;
	ms_wxenc_map["Windows-874"] = wxFONTENCODING_CP874;
	ms_wxenc_map["Windows-1250"] = wxFONTENCODING_CP1250;
	ms_wxenc_map["Windows-1251"] = wxFONTENCODING_CP1251;
	ms_wxenc_map["Windows-1252"] = wxFONTENCODING_CP1252;
	ms_wxenc_map["Windows-1253"] = wxFONTENCODING_CP1253;
	ms_wxenc_map["Windows-1254"] = wxFONTENCODING_CP1254;
	ms_wxenc_map["Windows-1255"] = wxFONTENCODING_CP1255;
	ms_wxenc_map["Windows-1256"] = wxFONTENCODING_CP1256;
	ms_wxenc_map["Windows-1257"] = wxFONTENCODING_CP1257;
	ms_wxenc_map["CP437"] = wxFONTENCODING_CP437;
	ms_wxenc_map["CP932"] = wxFONTENCODING_CP932;
	ms_wxenc_map["CP936"] = wxFONTENCODING_CP936;
	ms_wxenc_map["CP949"] = wxFONTENCODING_CP949;
	ms_wxenc_map["CP950"] = wxFONTENCODING_CP950;
	ms_wxenc_map["EUC-JP"] = wxFONTENCODING_EUC_JP;
	ms_wxenc_map["UTF-8"] = wxFONTENCODING_UTF8;
	ms_wxenc_map["UTF-16LE"] = wxFONTENCODING_UTF16LE;
	ms_wxenc_map["UTF-16BE"] = wxFONTENCODING_UTF16BE;
	ms_wxenc_map["UTF-32LE"] = wxFONTENCODING_UTF32LE;
	ms_wxenc_map["UTF-32BE"] = wxFONTENCODING_UTF32BE;

    BOOST_FOREACH(const WXEncMap::value_type& val, ms_wxenc_map)
    {
		std::string encname = val.first;
		wxFontEncoding wxenc = val.second;
		wxString wxencname(encname.c_str(), wxConvUTF8);

		ms_wxenc_list.push_back(wxencname);
		ms_enc_list.push_back(encname);
		ms_wxnameenc_map[wxencname] = wxenc;
		ms_wxencname_map[wxenc] = wxencname;
    }

	ms_wxenctype_map[wxFONTENCODING_ISO8859_1] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_2] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_3] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_4] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_5] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_6] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_7] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_8] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_9] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_10] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_11] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_12] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_13] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_ISO8859_15] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP874] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP1250] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP1251] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP1252] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP1253] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP1254] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP1255] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP1256] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP1257] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP437] = etSingleByte;
	ms_wxenctype_map[wxFONTENCODING_CP932] = etDoubleByte;
	ms_wxenctype_map[wxFONTENCODING_CP936] = etDoubleByte;
	ms_wxenctype_map[wxFONTENCODING_CP949] = etDoubleByte;
	ms_wxenctype_map[wxFONTENCODING_CP950] = etDoubleByte;
	ms_wxenctype_map[wxFONTENCODING_EUC_JP] = etDoubleByte;
	ms_wxenctype_map[wxFONTENCODING_UTF8] = etUTF8;
	ms_wxenctype_map[wxFONTENCODING_UTF16LE] = etUTF16LE;
	ms_wxenctype_map[wxFONTENCODING_UTF16BE] = etUTF16BE;
	ms_wxenctype_map[wxFONTENCODING_UTF32LE] = etUTF32LE;
	ms_wxenctype_map[wxFONTENCODING_UTF32BE] = etUTF32BE;

	
	ms_wxencfont_map[wxFONTENCODING_CP874] = GetMSCPFontName(wxT("874"));
	ms_wxencfont_map[wxFONTENCODING_CP932] = GetMSCPFontName(wxT("932"));
	ms_wxencfont_map[wxFONTENCODING_CP936] = GetMSCPFontName(wxT("936"));
	ms_wxencfont_map[wxFONTENCODING_CP949] = GetMSCPFontName(wxT("949"));
	ms_wxencfont_map[wxFONTENCODING_CP950] = GetMSCPFontName(wxT("950"));
	ms_wxencfont_map[wxFONTENCODING_EUC_JP] = GetMSCPFontName(wxT("51932"));

	ms_sysenc_idx = AdjustIndex(ms_sysenc_idx);
}

void WXMEncoding::FreeEncodings()
{
	BOOST_FOREACH(WXEncInstMap::value_type val, ms_inst_map)
		delete val.second;

	if (ms_sysenc)
		delete ms_sysenc;
}

size_t WXMEncoding::GetEncodingsCount()
{
    return ms_wxenc_map.size();
}

wxString WXMEncoding::GetEncodingName(ssize_t idx)
{
	if (idx < 0)
		return wxFontMapper::GetEncodingName(wxLocale::GetSystemEncoding());

    return ms_wxenc_list[idx];
}

wxFontEncoding IdxToEncoding(ssize_t idx)
{
	return WXMEncoding::NameToEncoding(WXMEncoding::GetEncodingName(idx));
}

wxString WXMEncoding::GetEncodingDescription(ssize_t idx)
{
	return wxFontMapper::GetEncodingDescription(IdxToEncoding(idx));
}

wxString WXMEncoding::GetEncodingFontName(ssize_t idx)
{
	WXEncFontMap::const_iterator it = ms_wxencfont_map.find(IdxToEncoding(idx));
	if (it == ms_wxencfont_map.end())
		return GetMSCPFontName(wxString());

	return it->second;
}

wxString WXMEncoding::EncodingToName(wxFontEncoding enc)
{
	WXEncNameMap::const_iterator it = ms_wxencname_map.find(enc);
	if (it == ms_wxencname_map.end())
		return wxFontMapper::GetEncodingName(wxLocale::GetSystemEncoding());

	return it->second;
}

wxFontEncoding WXMEncoding::NameToEncoding(const wxString &name)
{
	WXNameEncMap::const_iterator it = ms_wxnameenc_map.find(name);
	if (it == ms_wxnameenc_map.end())
		return wxLocale::GetSystemEncoding();

	return it->second;
}

WXMEncoding* WXMEncoding::GetSystemEncoding()
{
	if (ms_sysenc == NULL)
		ms_sysenc = CreateWxmEncoding(ms_sysenc_idx);

	return ms_sysenc;
}

ssize_t WXMEncoding::AdjustIndex(ssize_t idx)
{
	wxFontEncoding enc = IdxToEncoding(idx);

    for(size_t i=0; i<ms_wxenc_list.size(); i++)
    {
        if(ms_wxnameenc_map[ms_wxenc_list[i]]==enc)
        {
            return i;
        }
    }

	return idx;
}

WXMEncodingType WXMEncoding::GetIdxEncType(ssize_t idx)
{
	WXEncTypeMap::const_iterator it = ms_wxenctype_map.find(IdxToEncoding(idx));
	if (it == ms_wxenctype_map.end())
		return etSingleByte; //FIXME later

	return it->second;
}

WXMEncoding* WXMEncoding::CreateWxmEncoding(ssize_t idx)
{
	wxASSERT(idx<(ssize_t)ms_wxenc_list.size() && idx>=0);
	WXEncInstMap::iterator it = ms_inst_map.find(idx);
	if (it!=ms_inst_map.end() && it->second!=NULL)
		return it->second;

    WXMEncoding* enc = NULL;

	WXMEncodingType t = GetIdxEncType(idx);

    switch(t)
    {
    case etUTF8:
        enc = new WXMEncodingUTF8();
        break;
    case etUTF16LE:
        enc = new WXMEncodingUTF16LE();
        break;
    case etUTF16BE:
        enc = new WXMEncodingUTF16BE();
        break;
    case etUTF32LE:
        enc = new WXMEncodingUTF32LE();
        break;
    case etUTF32BE:
        enc = new WXMEncodingUTF32BE();
        break;
    case etSingleByte:
        enc = new WXMEncodingSingleByte();
        break;
    case etDoubleByte:
        enc = new WXMEncodingDoubleByte();
        break;
    }

    enc->Create(idx);
	ms_inst_map[idx] = enc;

    return enc;
}

WXMEncoding* WXMEncoding::CreateWxmEncoding(wxFontEncoding enc)
{
    size_t idx;
    for(idx=0;idx<ms_wxenc_list.size();idx++)
    {
        if(ms_wxnameenc_map[ms_wxenc_list[idx]]==enc)
        {
            return CreateWxmEncoding(idx);
        }
    }

    return CreateWxmEncoding(ms_sysenc_idx);
}

WXMEncoding* WXMEncoding::CreateWxmEncoding(const wxString &name)
{
    size_t idx;
    for(idx=0;idx<ms_wxenc_list.size();idx++)
    {
		if(ms_wxenc_list[idx].Lower()==name.Lower())
        {
            return CreateWxmEncoding(idx);
        }
    }

    return CreateWxmEncoding(ms_sysenc_idx);
}

void WXMEncoding::Create(ssize_t idx)
{
    wxASSERT(idx<(ssize_t)ms_wxenc_list.size() && idx>=0);

	m_idx = idx;
	m_name = GetEncodingName(idx);
	m_enc = NameToEncoding(m_name);
	m_type = GetIdxEncType(idx);
	m_desc = wxFontMapper::GetEncodingDescription(m_enc);
	m_fontname = WXMEncoding::GetEncodingFontName(m_idx);
}

void WXMEncodingMultiByte::Create(ssize_t idx)
{
	WXMEncoding::Create(idx);

#if defined(__WXGTK__)
    m_CSConv=new wxCSConv(m_name.c_str());
#else //#elif defined(__WXMSW__) || defined(__WXMAC__)
    m_CSConv=new wxCSConv(m_enc);
#endif

    MultiByteInit();
}

void WXMEncodingSingleByte::MultiByteInit()
{
    m_MBtoWC_Table=new ucs2_t[256];
    memset(m_MBtoWC_Table, 0, sizeof(ucs2_t)*256);

    m_WCtoMB_Table=new wxWord[65536];
    memset(m_WCtoMB_Table, 0, sizeof(wxWord)*65536);

   // cache the results of Single-Byte <==> Wide-Char
    wxByte singlebyte[2]={0,0};
    wchar_t wc[2];
    for(wxWord i=0;i<256;i++)
    {
        singlebyte[0]=i;
        if(m_CSConv->MB2WC(wc,(char*)singlebyte,2)==1)
        {
            m_MBtoWC_Table[i]=wc[0];
        }
        else
        {
            m_MBtoWC_Table[i]=i;
            wc[0]=i;
        }
        m_WCtoMB_Table[ wc[0] ]= (i<<8) ;
    }
}

void WXMEncodingDoubleByte::MultiByteInit()
{
    m_MBtoWC_Table=new ucs2_t[65536];
    // value: 0x0000, indicate the column isn't a valid Double-Byte char
    memset(m_MBtoWC_Table, 0, sizeof(ucs2_t)*256);
    // value: 0xFFFF, indicate the column is non-cached
    memset(&m_MBtoWC_Table[256], 0xFF, sizeof(ucs2_t)*(65536-256));
    m_WCtoMB_Table=new wxWord[65536];
    memset(m_WCtoMB_Table, 0xFF, sizeof(wxWord)*65536);
    m_LeadByte_Table=new wxByte[256];
    memset(m_LeadByte_Table, 0, 256);
    m_LeadByte_Table[0]=0xFF;
}

size_t WXMEncodingMultiByte::UCS4toMultiByte(ucs4_t ucs4, wxByte* buf)
{
    if(ucs4>0xFFFF)
        return 0;

    wxWord mb=m_WCtoMB_Table[ucs4];
    if(mb==0)        // invalid MB char
    {
        return 0;
    }

    size_t len;
    if(mb==0xFFFF)    // non-cached
    {
        wxByte mbs[4];
        wchar_t wc[2]={ucs4,0};
        len=m_CSConv->WC2MB((char*)mbs,wc,4);
        if(len==0 || len==(size_t)-1)
        {
            m_WCtoMB_Table[ucs4]=0;
            return 0;
        }

        wxASSERT( len<=2 );

        //mbs[len]=0;
        mb= (((wxWord)mbs[0])<<8) | mbs[1];
        m_WCtoMB_Table[ucs4]=mb;

        buf[0]=mbs[0];
        if(len==2) buf[1]=mbs[1];

        return len;
    }

    buf[0]=mb>>8;
    len=1;

    if((mb&0xFF)!=0)
    {
        buf[1]=mb&0xFF;
        ++len;//len=2
    }

    return len;
}

size_t WXMEncodingUTF8::UCS4toMultiByte(ucs4_t ucs4, wxByte* buf)
{
    /***  from rfc3629
    Char. number range  |        UTF-8 octet sequence
    (hexadecimal)       |              (binary)
    --------------------+---------------------------------------------
    0000 0000-0000 007F | 0xxxxxxx
    0000 0080-0000 07FF | 110xxxxx 10xxxxxx
    0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
    0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

    UTF8-octets = *( UTF8-char )
    UTF8-char   = UTF8-1 / UTF8-2 / UTF8-3 / UTF8-4
    UTF8-1      = %x00-7F
    UTF8-2      = %xC2-DF UTF8-tail
    UTF8-3      = %xE0 %xA0-BF UTF8-tail / %xE1-EC 2( UTF8-tail ) /
                  %xED %x80-9F UTF8-tail / %xEE-EF 2( UTF8-tail )
    UTF8-4      = %xF0 %x90-BF 2( UTF8-tail ) / %xF1-F3 3( UTF8-tail ) /
                  %xF4 %x80-8F 2( UTF8-tail )
    UTF8-tail   = %x80-BF
    ***/

    if(ucs4<0)
        return 0;

    if(ucs4<=0x7F)
    {
        buf[0]=ucs4;
        return 1;
    }
    if(ucs4<=0x7FF)
    {
        buf[0]= 0xC0 | (ucs4>>6);
        buf[1]= 0x80 | (ucs4&0x3F);
        return 2;
    }
    if(ucs4<=0xFFFF)
    {
        buf[0]= 0xE0 | (ucs4>>12);
        buf[1]= 0x80 | ((ucs4>>6)&0x3F);
        buf[2]= 0x80 | (ucs4&0x3F);
        return 3;
    }
    if(ucs4<=0x10FFFF)
    {
        buf[0]= 0xF0 | (ucs4>>18);
        buf[1]= 0x80 | ((ucs4>>12)&0x3F);
        buf[2]= 0x80 | ((ucs4>>6)&0x3F);
        buf[3]= 0x80 | (ucs4&0x3F);
        return 4;
    }

    return 0;
}

size_t WXMEncodingUTF16LE::UCS4toMultiByte(ucs4_t ucs4, wxByte* buf)
{
    if(ucs4>=0x10000)// to unicode surrogates
    {
        if(ucs4>0x10FFFF) return 0;
        return UCS4toUTF16LE_U10000(ucs4, buf);
    }

    buf[0]=ucs4;
    buf[1]=ucs4>>8;

    return 2;
}
size_t WXMEncodingUTF16BE::UCS4toMultiByte(ucs4_t ucs4, wxByte* buf)
{
    if(ucs4>=0x10000)// to unicode surrogates
    {
        if(ucs4>0x10FFFF) return 0;

        ucs4-=0x10000;
        ucs2_t high=(ucs4>>10)+0xD800;    // high surrogate
        ucs2_t low=(ucs4&0x3FF)+0xDC00;    // low surrogate

        buf[0]=high>>8;
        buf[1]=high;
        buf[2]=low>>8;
        buf[3]=low;

        return 4;
    }

    buf[0]=ucs4>>8;
    buf[1]=ucs4;

    return 2;
}

size_t WXMEncodingUTF32LE::UCS4toMultiByte(ucs4_t ucs4, wxByte* buf)
{
    wxASSERT(ucs4>=0 && ucs4<=0x10FFFF);

#if wxBYTE_ORDER == wxBIG_ENDIAN
    wxByte *p = (wxByte*)&ucs4;
    buf[0]=p[3];
    buf[1]=p[2];
    buf[2]=p[1];
    buf[3]=p[0];
#else
    *((ucs4_t*)buf) = ucs4;
#endif
    return 4;
}
size_t WXMEncodingUTF32BE::UCS4toMultiByte(ucs4_t ucs4, wxByte* buf)
{
    wxASSERT(ucs4>=0 && ucs4<=0x10FFFF);

#if wxBYTE_ORDER == wxBIG_ENDIAN
    *((ucs4_t*)buf) = ucs4;
#else
    wxByte *p = (wxByte*)&ucs4;
    buf[0]=p[3];
    buf[1]=p[2];
    buf[2]=p[1];
    buf[3]=p[0];
#endif
    return 4;
}

ucs4_t WXMEncodingSingleByte::MultiBytetoUCS4(wxByte* buf)
{
    return m_MBtoWC_Table[ *buf ];
}

// return 0 if it is not a valid DB char
ucs4_t WXMEncodingDoubleByte::MultiBytetoUCS4(wxByte* buf)
{
    if(m_LeadByte_Table[ buf[0] ] == 0)
        IsLeadByte(buf[0]);

    register unsigned int w = (((unsigned int)buf[0]) << 8) | buf[1];
    return m_MBtoWC_Table[w];
}

bool WXMEncodingDoubleByte::IsLeadByte(wxByte byte)
{
    if(m_LeadByte_Table[byte]==0)
    {
        wxWord db= ((wxWord)byte)<<8 ;
        wxByte dbs[4]={byte,0,0,0};
        wchar_t wc[4];

        // check first byte
        if(m_CSConv->MB2WC(wc,(char*)dbs,4)==1)
        {
            m_MBtoWC_Table[ db ] = wc[0];
            m_WCtoMB_Table[ wc[0] ] = db;
        }
        else
        {
            m_MBtoWC_Table[ db ] = 0;
        }

        ++db;
        ++dbs[1];
        for(int i=1; i<=0xFF; ++i, ++db, ++dbs[1])
        {
            if(m_CSConv->MB2WC(wc,(char*)dbs,4)==1)
            {
                m_MBtoWC_Table[ db ]    = wc[0];
                m_WCtoMB_Table[ wc[0] ] = db;
                m_LeadByte_Table[byte]  = 1;       // is lead-byte
            }
            else
            {
                m_MBtoWC_Table[ db ]=0;
            }
        }

        if(m_LeadByte_Table[byte]==0)
        {
            m_LeadByte_Table[byte]=0xFF;
            return false;
        }
    }

    return m_LeadByte_Table[byte]==1;
}

size_t UCS4toUTF16LE_U10000(ucs4_t ucs4, wxByte* buf)
{
	//ucs4=(highChar -0xD800) * 0x400 + (lowChar -0xDC00) + 0x10000
	//if(ucs4>0x10FFFF) return 0;

	//wxASSERT(ucs4>=0x10000 && ucs4<=0x10FFFF);

	ucs4-=0x10000;
	ucs2_t high=(ucs4>>10)+0xD800;    // high surrogate
	ucs2_t low=(ucs4&0x3FF)+0xDC00;    // low surrogate

	buf[0]=high;
	buf[1]=high>>8;
	buf[2]=low;
	buf[3]=low>>8;

	return 4;
}

};// namespace wxm