///////////////////////////////////////////////////////////////////////////////
// vim:         ts=4 sw=4
// Name:        wxmedit/wxm_encoding_unicode.h
// Description: define the Unicode Encodings which are supported by wxMEdit
// Author:      wxmedit@gmail.com
// Licence:     GPL
///////////////////////////////////////////////////////////////////////////////

#ifndef _WXM_ENCODING_UNICODE_H_
#define _WXM_ENCODING_UNICODE_H_

#include "wxm_encoding.h"

#include <unicode/ucnv.h>
#include <boost/array.hpp>

namespace wxm
{

struct WXMEncodingUTF8: public WXMEncoding
{
	virtual size_t UCS4toMultiByte(ucs4_t ucs4, wxByte* buf);
	virtual bool NextUChar32(MadUCQueue &ucqueue, UChar32BytesMapper& mapper);

	virtual bool IsUChar32_LineFeed(const wxByte* buf, size_t len)
	{
		return m_dec.IsUChar32_LineFeed(buf, len);
	}
	virtual bool IsUChar32_LineFeed(WXMBlockDumper& dumper, size_t len)
	{
		return m_dec.IsUChar32_LineFeed(dumper, len);
	}
	virtual ucs4_t PeekUChar32_Newline(WXMBlockDumper& dumper, size_t len)
	{
		return m_dec.PeekUChar32_Newline(dumper, len);
	}

private:
	friend WXMEncoding* WXMEncodingCreator::CreateWxmEncoding(ssize_t idx);
	WXMEncodingUTF8(){}
	~WXMEncodingUTF8(){}

	WXMEncodingDecoderISO646 m_dec;
};

struct WXMEncodingUTF16LE: public WXMEncoding
{
	virtual size_t UCS4toMultiByte(ucs4_t ucs4, wxByte* buf);
	virtual bool NextUChar32(MadUCQueue &ucqueue, UChar32BytesMapper& mapper);
	virtual ucs4_t PeekUChar32_Newline(WXMBlockDumper& dumper, size_t len);
	virtual bool IsUChar32_LineFeed(WXMBlockDumper& dumper, size_t len);
	virtual bool IsUChar32_LineFeed(const wxByte* buf, size_t len)
	{
		return len>=2 && buf[0]==0x0A && buf[1]==0;
	}

private:
	friend WXMEncoding* WXMEncodingCreator::CreateWxmEncoding(ssize_t idx);
	WXMEncodingUTF16LE(){}
	~WXMEncodingUTF16LE(){}
};

struct WXMEncodingUTF16BE: public WXMEncoding
{
	virtual size_t UCS4toMultiByte(ucs4_t ucs4, wxByte* buf);
	virtual bool NextUChar32(MadUCQueue &ucqueue, UChar32BytesMapper& mapper);
	virtual ucs4_t PeekUChar32_Newline(WXMBlockDumper& dumper, size_t len);
	virtual bool IsUChar32_LineFeed(WXMBlockDumper& dumper, size_t len);
	virtual bool IsUChar32_LineFeed(const wxByte* buf, size_t len)
	{
		return len>=2 && buf[1]==0x0A && buf[0]==0;
	}

private:
	friend WXMEncoding* WXMEncodingCreator::CreateWxmEncoding(ssize_t idx);
	WXMEncodingUTF16BE(){}
	~WXMEncodingUTF16BE(){}
};

struct WXMEncodingUTF32LE: public WXMEncoding
{
	virtual size_t UCS4toMultiByte(ucs4_t ucs4, wxByte* buf);
	virtual bool NextUChar32(MadUCQueue &ucqueue, UChar32BytesMapper& mapper);
	virtual ucs4_t PeekUChar32_Newline(WXMBlockDumper& dumper, size_t len);
	virtual bool IsUChar32_LineFeed(WXMBlockDumper& dumper, size_t len);
	virtual bool IsUChar32_LineFeed(const wxByte* buf, size_t len)
	{
		return len>=4 && buf[0]==0x0A && buf[1]==0 && buf[2]==0 && buf[3]==0;
	}

private:
	friend WXMEncoding* WXMEncodingCreator::CreateWxmEncoding(ssize_t idx);
	WXMEncodingUTF32LE(){}
	~WXMEncodingUTF32LE(){}
};

struct WXMEncodingUTF32BE: public WXMEncoding
{
	virtual size_t UCS4toMultiByte(ucs4_t ucs4, wxByte* buf);
	virtual bool NextUChar32(MadUCQueue &ucqueue, UChar32BytesMapper& mapper);
	virtual ucs4_t PeekUChar32_Newline(WXMBlockDumper& dumper, size_t len);
	virtual bool IsUChar32_LineFeed(WXMBlockDumper& dumper, size_t len);
	virtual bool IsUChar32_LineFeed(const wxByte* buf, size_t len)
	{
		return len>=4 && buf[3]==0x0A && buf[2]==0 && buf[1]==0 && buf[0]==0;
	}

private:
	friend WXMEncoding* WXMEncodingCreator::CreateWxmEncoding(ssize_t idx);
	WXMEncodingUTF32BE(){}
	~WXMEncodingUTF32BE(){}
};

size_t UCS4toUTF16LE_U10000(ucs4_t ucs4, wxByte* buf);

};// namespace wxm

#endif // _WXM_ENCODING_UNICODE_H_