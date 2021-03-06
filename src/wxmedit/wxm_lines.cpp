///////////////////////////////////////////////////////////////////////////////
// vim:         ts=4 sw=4 expandtab
// Name:        wxmedit/wxm_lines.cpp
// Description: The Text/Data Buffer of wxMEdit
// Copyright:   2013-2015  JiaYanwei   <wxmedit@gmail.com>
//              2005-2010  Alston Chen <madedit@gmail.com>
// License:     GPLv3
///////////////////////////////////////////////////////////////////////////////

#include "wxm_lines.h"
#include "../xm/cxx11.h"
#include "../wxm/encoding/encoding.h"
#include "../wxm/encdet.h"
#include "../wxm/def.h"
#include "mad_encdet.h"
#include "wxm_syntax.h"
#include "wxmedit.h"

#ifdef _MSC_VER
# pragma warning( push )
# pragma warning( disable : 4996 )
#endif
// disable 4996 {
#include <wx/filename.h>
// disable 4996 }
#ifdef _MSC_VER
# pragma warning( pop )
#endif

#include <boost/scoped_ptr.hpp>

#include <algorithm>

#ifdef _DEBUG
#include <crtdbg.h>
#define new new(_NORMAL_BLOCK ,__FILE__, __LINE__)
#endif


const int BUFFER_SIZE = 1024 * 256;     // 256KB
const int BUFFER_BITS = 18;
const int BUFFER_MASK = BUFFER_SIZE - 1;
const wxFileOffset BUFFER_BASE_MASK = wxFileOffset(-1) << BUFFER_BITS;
                                    //wxFileOffset(-1) -  BUFFER_MASK;

#define NEXTUCHAR_BUFFER_SIZE (1024 * 10)

//===========================================================================
// MadFileNameIsUTF8, MadDirExists, MadConvFileName_WC2MB_UseLibc
// for Testing/Converting of FileName Encoding under Linux
//===========================================================================

bool g_WC2MB_2_utf8=true;
bool g_MB2WC_is_utf8=false;
bool g_MB2WC_check_dir_filename=false;//check dir, filename separately

// return 0: filename not found, 1:yes, -1:no
int MadFileNameIsUTF8(const wxString& filename)
{
    g_WC2MB_2_utf8=false;
    wxLogNull nolog;
    if(wxFileModificationTime(filename)!=0)
    {
        return -1;//libc or under win32
    }

#ifndef __WXMSW__
    // retry by using utf8 filename
    g_WC2MB_2_utf8=true;
    if(wxFileModificationTime(filename)!=0)
    {
        return 1;//utf8
    }
#endif

    return 0;//not found
}

// return 0: name is not a dir or not found, 1:yes(utf8), -1:yes(libc)
int MadDirExists(const wxString& name)
{
#ifdef __WXMSW__
    if(wxDirExists(name))
        return -1;
#else
    int test=MadFileNameIsUTF8(name);

    g_WC2MB_2_utf8 = (test>0);

    if(test==0)
    {
        if(wxDirExists(name))
            return -1;

        return 0;
    }

    if(wxDirExists(name))
        return test;
#endif

    return 0;
}

#ifndef __WXMSW__
size_t MadConvFileName::MB2WC(wchar_t *outputBuf, const char *psz, size_t outputSize) const
{
    size_t len=strlen(psz);

    if(g_MB2WC_check_dir_filename==false)
    {
        g_MB2WC_is_utf8=wxm::IsUTF8((wxByte *)psz, int(len));

        if(g_MB2WC_is_utf8)
        {
            return wxConvUTF8.MB2WC(outputBuf, psz, outputSize);
        }

        return wxConvLibc.MB2WC(outputBuf, psz, outputSize);
    }

    //check dir, filename separately
    size_t dirlen=0, fnlen=0;
    if(len>0)
    {
        for(int i=int(len-1);i>=0;i--, fnlen++)
        {
            if(psz[i]=='/')
                break;
        }
        dirlen=len-fnlen;
    }

    size_t dirret=0;
    if(dirlen!=0)
    {
        vector<char> cbuf;
        cbuf.resize(dirlen+1);
        char *pbuf = (char *) &cbuf[0];
        memcpy(pbuf, psz, dirlen);
        cbuf[dirlen]=0;

        bool is_utf8=wxm::IsUTF8((wxByte *)pbuf, int(dirlen));
        if(is_utf8)
        {
            dirret=wxConvUTF8.MB2WC(outputBuf, pbuf, outputSize);
        }
        else
        {
            dirret=wxConvLibc.MB2WC(outputBuf, pbuf, outputSize);
        }
    }

    if(dirret==size_t(-1))
        return size_t(-1);

    size_t fnret=0;
    if(fnlen!=0)
    {
        bool is_utf8=wxm::IsUTF8((wxByte *)psz+dirlen, int(fnlen));
        wchar_t *obuf=outputBuf;
        if(outputBuf!=nullptr)
            obuf+=dirret;

        if(is_utf8)
        {
            fnret=wxConvUTF8.MB2WC(obuf, psz+dirlen, outputSize-dirret);
        }
        else
        {
            fnret=wxConvLibc.MB2WC(obuf, psz+dirlen, outputSize-dirret);
        }
    }

    if(fnret==size_t(-1))
        return size_t(-1);

    return dirret+fnret;
}

size_t MadConvFileName::WC2MB(char *outputBuf, const wchar_t *psz, size_t outputSize) const
{
    if(g_WC2MB_2_utf8)
        return wxConvUTF8.WC2MB(outputBuf, psz, outputSize);

    return wxConvLibc.WC2MB(outputBuf, psz, outputSize);
}
#endif

//===========================================================================
// MadMemData
//===========================================================================

MadMemData::MadMemData()
{
    m_Size = 0;
}

MadMemData::~MadMemData()
{
    size_t i, count;
    for(i = 0, count = m_Buffers.size(); i < count; ++i)
    {
        delete []m_Buffers[i];
    }
    m_Buffers.clear();
}

wxByte MadMemData::Get(const wxFileOffset &pos)
{
    wxASSERT((pos >= 0) && (pos < m_Size));

    int buf = int(pos >> BUFFER_BITS);    //pos / BUFFER_SIZE;
    int idx = int(pos & BUFFER_MASK);    //pos % BUFFER_SIZE;

    return m_Buffers[buf][idx];
}

void MadMemData::Get(const wxFileOffset &pos, wxByte *buffer, size_t size)
{
    wxASSERT((pos >= 0) && (size > 0) && ((pos + size) <= m_Size));

    int buf = int(pos >> BUFFER_BITS);    //pos / BUFFER_SIZE;
    int idx = int(pos & BUFFER_MASK);    //pos % BUFFER_SIZE;

    size_t buf_size = BUFFER_SIZE - idx;

    while(true)
    {
        if(buf_size > size) buf_size = size;

        /***
        if(buf_size <= sizeof(int)*40) buggy!!!
        {
            int *src=(int*)buffer;
            int *des=(int*)(m_Buffers[buf] + idx);
            size_t s=buf_size;

            if(s >= sizeof(int)*20)     //max:40
            {
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                s-=sizeof(int)*20;
            }
            if(s >= sizeof(int)*10)     //max:20
            {
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                s-=sizeof(int)*10;
            }
            if(s >= sizeof(int)*10)     //max:10
            {
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                *src++ = *des++; *src++ = *des++;
                s-=sizeof(int)*10;
            }
            if(s >= sizeof(int)*5)      //max:9
            {
                *src++ = *des++;
                *src++ = *des++;
                *src++ = *des++;
                *src++ = *des++;
                *src++ = *des++;
                s-=sizeof(int)*5;
            }
            if(s >= sizeof(int))        //max:4
            {
                *src++ = *des++;
                s-=sizeof(int);
                if(s >= sizeof(int))        //max:3
                {
                    *src++ = *des++;
                    s-=sizeof(int);
                    if(s >= sizeof(int))        //max:2
                    {
                        *src++ = *des++;
                        s-=sizeof(int);
                        if(s >= sizeof(int))        //max:1
                        {
                            *src++ = *des++;
                            s-=sizeof(int);
                        }
                    }
                }
            }
            if(s)       //max:3byte
            {
                buffer=(wxByte*)src;
                wxByte *bdes=(wxByte*)des;

                *buffer++=*bdes++;
                s--;
                if(s)   //max:2b
                {
                    *buffer++=*bdes++;
                    s--;
                    if(s)   //max:1byte
                    {
                        *buffer=*bdes;
                        s--;            //over
                    }
                }
            }

            wxASSERT(s==0);
        }
        else
            ***/
        {
            memcpy(buffer, m_Buffers[buf] + idx, buf_size);
        }

        if((size -= buf_size) ==0) break;

        ++buf;
        idx = 0;
        buffer += buf_size;
        buf_size = BUFFER_SIZE;
    }
}

wxFileOffset MadMemData::Put(wxByte * buffer, size_t size)
{
    wxASSERT(size > 0);

    size_t buf = size_t(m_Size >> BUFFER_BITS);        //m_Size / BUFFER_SIZE;
    int idx = int(m_Size & BUFFER_MASK);            //m_Size % BUFFER_SIZE;
    size_t buf_size = BUFFER_SIZE - idx;
    wxByte *tmp = idx!=0 ? m_Buffers[buf] : 0;

    wxFileOffset pos = m_Size;
    m_Size += size;
    do
    {
        if(idx == 0)
        {
            if(buf < m_Buffers.size())
            {
                tmp = m_Buffers[buf];
            }
            else
            {
                tmp = new wxByte[BUFFER_SIZE];
                m_Buffers.push_back(tmp);                // add a new buffer
            }
            buf_size = BUFFER_SIZE;
        }

        if(size < buf_size) buf_size = size;

        memcpy(tmp + idx, buffer, buf_size);
        buffer += buf_size;
        size -= buf_size;
        idx = 0;
        ++buf;
    }
    while(size > 0);

    return pos;
}


//===========================================================================
// MadFileData
//===========================================================================

MadFileData::MadFileData(const wxString &name)
{
    m_Name = name;
    m_Size = 0;
    m_OpenSuccess = true;
    m_ReadOnly = false;
    m_Buffer1 = nullptr;
    m_Buffer2 = nullptr;

    int utf8test=MadFileNameIsUTF8(name);

    if(utf8test==0)// this file/dir is not exist
    {
        return;
    }

    MadConvFileName_WC2MB_UseLibc uselibc(utf8test<0);

    if(wxDirExists(name))// test this name is a dir or not
    {
        m_OpenSuccess = false;
        return;
    }

    if(OpenFile())
    {
        m_Size = m_File.SeekEnd();    // get size
        m_File.Seek(0);                // move to begin

        m_Buffer1 = new wxByte[BUFFER_SIZE];
        m_Buf1Pos = 0;
        m_Buffer2 = new wxByte[BUFFER_SIZE];
        m_Buf2Pos = -1;

        // read first data-block to buffer1
        size_t size = BUFFER_SIZE;
        if(BUFFER_SIZE > m_Size) size = size_t(m_Size);
        m_File.Read(m_Buffer1, size);
    }
}

MadFileData::~MadFileData()
{
    if(m_File.IsOpened())   m_File.Close();
    if(m_Buffer1)       delete []m_Buffer1;
    if(m_Buffer2)       delete []m_Buffer2;
}

wxByte MadFileData::Get(const wxFileOffset &pos)
{
    wxASSERT((pos >= 0) && (pos < m_Size));

    wxFileOffset idx;
    if(m_Buf1Pos>=0)
    {
        idx = pos - m_Buf1Pos;
        if(idx >= 0 && idx < BUFFER_SIZE)       // wanted byte in buffer1
        {
            return m_Buffer1[(size_t)idx];
        }
    }

    if(m_Buf2Pos>=0)
    {
        idx = pos - m_Buf2Pos;
        if(idx >= 0 && idx < BUFFER_SIZE)       // wanted byte in buffer2
        {
            return m_Buffer2[(size_t)idx];
        }
    }

    if(m_Buf1Pos>=0)
    {
        // swap buffer1 buffer2
        wxByte *tmp=m_Buffer2;
        m_Buffer2=m_Buffer1;
        m_Buffer1=tmp;

        m_Buf2Pos=m_Buf1Pos;
    }

    // read data-block to buffer1
    m_Buf1Pos = pos & BUFFER_BASE_MASK; //(pos >> BUFFER_BITS) << BUFFER_BITS;    //(pos / BUFFER_SIZE) * BUFFER_SIZE;
    m_File.Seek(m_Buf1Pos);

    wxFileOffset size = m_Size - m_Buf1Pos;
    if(size > BUFFER_SIZE)    size = BUFFER_SIZE;

    m_File.Read(m_Buffer1, size_t(size));

    idx = pos - m_Buf1Pos;
    return m_Buffer1[(size_t)idx];
}

void MadFileData::Get(const wxFileOffset &pos, wxByte * buffer, size_t size)
{
    wxASSERT((pos >= 0) && (size > 0) && ((pos + size) <= m_Size));

    wxFileOffset idx;
    if(m_Buf1Pos>=0)
    {
        idx = pos - m_Buf1Pos;
        if(idx >= 0 && wxFileOffset(idx+size)<=BUFFER_SIZE )        // wanted bytes in buffer1
        {
            memcpy(buffer, m_Buffer1+idx, size);
            return;
        }
    }

    if(m_Buf2Pos>=0)
    {
        idx = pos - m_Buf2Pos;
        if(idx >= 0 && wxFileOffset(idx+size)<=BUFFER_SIZE )    // wanted bytes in buffer2
        {
            memcpy(buffer, m_Buffer2+idx, size);
            return;
        }
    }

    idx = pos & BUFFER_BASE_MASK; //(pos >> BUFFER_BITS) << BUFFER_BITS;        //(pos / BUFFER_SIZE) * BUFFER_SIZE;
    if(wxFileOffset(pos+size) <= idx+BUFFER_SIZE)   // wanted bytes in one block
    {
        if(m_Buf1Pos>=0)
        {
            // swap buffer1 buffer2
            wxByte *tmp=m_Buffer2;
            m_Buffer2=m_Buffer1;
            m_Buffer1=tmp;

            m_Buf2Pos=m_Buf1Pos;
        }

        // read data-block to buffer1
        m_Buf1Pos = idx;
        m_File.Seek(m_Buf1Pos);

        wxFileOffset bufsize = m_Size - m_Buf1Pos;
        if(bufsize > BUFFER_SIZE)    bufsize = BUFFER_SIZE;

        m_File.Read(m_Buffer1, size_t(bufsize));

        idx = pos - m_Buf1Pos;
        memcpy(buffer, m_Buffer1+idx, size);
        return;
    }

    // wanted bytes cross more than one block
    m_File.Seek(pos);
    m_File.Read(buffer, size);
}

wxFileOffset MadFileData::Put(wxByte *buffer, size_t size)
{
    wxFileOffset pos=m_SavePos;

    m_File.Seek(m_SavePos);
    m_File.Write(buffer, size);
    m_SavePos+=size;

    if(m_Size < m_SavePos)
    {
        m_Size = m_SavePos;
    }

    m_Buf1Pos=m_Buf2Pos=-1;

    return pos;
}

bool MadFileData::OpenFile()
{
    wxLogNull nolog;// disable error message of file.Open()

    m_ReadOnly = false;
    m_OpenSuccess = m_File.Open(m_Name.c_str(), wxFile::read_write);    // read&write
    if(m_OpenSuccess == false)
    {
        m_OpenSuccess = m_File.Open(m_Name.c_str(), wxFile::read);    // read only
        if(m_OpenSuccess == false)
        {
            return false;
        }

        m_ReadOnly = true;
    }

    return true;
}

bool MadFileData::Rename(const wxString &name)
{
    //wxASSERT(handle_ != INVALID_HANDLE_VALUE);

    m_File.Close();

    MadConvFileName_WC2MB_UseLibc uselibc(MadFileNameIsUTF8(m_Name)<0);

    if(wxRenameFile(m_Name, name))
    {
        m_Name = name;
        OpenFile();
        return true;
    }

    // can't rename the file, reopen the file
    OpenFile();
    return false;
}


//===========================================================================
// MadLine
//===========================================================================

void MadLine::Reset(void)
{
    m_Blocks.clear();
    m_RowIndices.clear();
    m_BracePairIndices.clear();
}

void MadLine::Empty(void)
{
    m_Size = 0;
    m_NewLineSize = 0;
    m_Blocks.resize(1);
    m_Blocks[0].Reset();
    m_RowIndices.resize(2);
    m_RowIndices[0].Reset();
    m_RowIndices[1].Reset();
    m_BracePairIndices.clear();
}

ucs4_t MadLine::LastUCharIsNewLine(wxm::WXMEncoding *encoding)
{
    if(m_Size == 0)
        return 0;

    MadBlockIterator bit = m_Blocks.end();
    --bit;

    wxm::WXMBackwardBlockDumper dumper(bit);

    return encoding->PeekUChar32_Newline(dumper, size_t(m_Size));
}

bool MadLine::FirstUCharIs0x0A(wxm::WXMEncoding *encoding)
{
    if(m_Size == 0) return false;

    MadBlockIterator bit = m_Blocks.begin();

    wxm::WXMForwardBlockDumper dumper(bit);

    return encoding->IsUChar32_LineFeed(dumper, size_t(m_Size));
}


//===========================================================================
// MadLines
//===========================================================================

MadLines::MadLines(MadEdit *madedit)
    : m_manual(false)
{
    m_MadEdit = madedit;
    m_Syntax = madedit->m_Syntax;
    SetEncoding(madedit->m_Encoding);

    // reserve one empty line
    Empty(false);

    //m_Name="";
    m_ReadOnly = false;
    m_MaxLineWidth = 0;

    m_FileData = nullptr;
    m_TmpFileData = nullptr;

    m_MemData = new MadMemData();

    m_WriteBuffer=nullptr;

    m_NextUChar_Buffer = new wxByte[NEXTUCHAR_BUFFER_SIZE+10];
    m_NextUChar_BufferLoadNew=true;
    m_NextUChar_BufferStart=0;
    m_NextUChar_BufferSize=0;
}

MadLines::~MadLines(void)
{
    // clear all lines
    Empty(true);

    if(m_FileData)
        delete m_FileData;

    if(m_TmpFileData)
    {
        wxString tmpfile = m_TmpFileData->m_Name;
        delete m_TmpFileData;
        wxRemoveFile(tmpfile);
    }

    if(m_MemData)
        delete m_MemData;

    delete []m_NextUChar_Buffer;
}


void MadLines::Empty(bool freeAll)
{
    m_Size = 0;
    m_LineCount = 0;
    m_RowCount = 0;

    MadLineIterator iter, end;
    iter = m_LineList.begin();
    end = m_LineList.end();

    if(!freeAll)
    {
        if(m_LineList.empty())
        {
            iter = m_LineList.insert(end, MadLine());
        }

        // reserve one empty line
        iter->Empty();
        // to second line
        ++iter;

        ++m_LineCount;
        ++m_RowCount;
    }

    if(iter != end)
        do
        {
            iter->m_Blocks.clear();
            iter->m_RowIndices.clear();
            iter = m_LineList.erase(iter);
        }
        while(iter != end);

}

void MadLines::Clear(bool freeAll)
{
    Empty(freeAll);

    m_Name = wxT("");
    m_ReadOnly = false;
    m_MaxLineWidth = 0;

    if(m_FileData)
    {
        delete m_FileData;
        m_FileData = nullptr;
    }

    if(m_TmpFileData)
    {
        wxString tmpfile = m_TmpFileData->m_Name;
        delete m_TmpFileData;
        wxRemoveFile(tmpfile);
        m_TmpFileData = nullptr;
    }

    m_MemData->Reset();
}

void MadLines::SetEncoding(wxm::WXMEncoding *encoding)
{
    m_Encoding=encoding;
}

ucs4_t MadLines::GetNewLine(const MadLineIterator &iter)
{
    if(iter->m_NewLineSize == 0) return 0;

    InitNextUChar(iter, iter->m_Size - iter->m_NewLineSize);
    MadUCQueue ucq;
    NextUChar(ucq);
    NextUChar(ucq);

    if(ucq.size()==2) return 0x0D+0x0A;
    return ucq.front().first;
}

void MadLines::LoadNewBuffer()
{
    if(m_NextUChar_BufferSize!=0) // move rest data to begin of buffer
    {
        if(m_NextUChar_BufferSize<=sizeof(int))
        {
            int tmp=*((int*)(m_NextUChar_Buffer+m_NextUChar_BufferStart));
            *((int*)m_NextUChar_Buffer)=tmp;
        }
        else
        {
            memcpy(m_NextUChar_Buffer, m_NextUChar_Buffer+m_NextUChar_BufferStart, m_NextUChar_BufferSize);
        }
    }

    m_NextUChar_BufferStart=0;
    wxFileOffset size=m_NextUChar_LineSize-m_NextUChar_BufferNextPos;

    if(size>NEXTUCHAR_BUFFER_SIZE)
        size=NEXTUCHAR_BUFFER_SIZE;

    wxASSERT(size>0);

    m_NextUChar_LineIter->Get(m_NextUChar_BufferNextPos,
                              m_NextUChar_Buffer + m_NextUChar_BufferSize,
                              size_t(size));

    m_NextUChar_BufferSize += size_t(size);
    m_NextUChar_BufferNextPos += size_t(size);
}

void MadLines::InitNextUChar(const MadLineIterator &iter, const wxFileOffset pos)
{
    m_NextUChar_LineIter = iter;
    m_NextUChar_Pos = pos;
    m_NextUChar_LineSize = iter->m_Size;


    // LoadNewBuffer()
    if(m_NextUChar_BufferLoadNew || m_NextUChar_BufferSize==0)
    {
        m_NextUChar_BufferStart=0;
        wxFileOffset size=m_NextUChar_LineSize-pos;

        if(size>NEXTUCHAR_BUFFER_SIZE)
            size=NEXTUCHAR_BUFFER_SIZE;

        if(size>0)
        {
            m_NextUChar_LineIter->Get(pos, m_NextUChar_Buffer, size_t(size));
        }
        m_NextUChar_BufferSize = size_t(size);
    }

    m_NextUChar_BufferNextPos=pos+m_NextUChar_BufferSize;

}

void MadLines::MoveUChar32Bytes(MadUCQueue &ucqueue, ucs4_t uc, size_t len)
{
    ucqueue.push_back(MadUCPair(uc, len));
    m_NextUChar_Pos += len;
    m_NextUChar_BufferStart += len;
    m_NextUChar_BufferSize -= len;
}
wxByte* MadLines::BufferLoadBytes(wxFileOffset& rest, size_t buf_len)
{
    rest = m_NextUChar_LineSize - m_NextUChar_Pos;
    if (rest <= 0)
        return nullptr;

    //check buffersize
    if (m_NextUChar_BufferNextPos!=m_NextUChar_LineSize && m_NextUChar_BufferSize<buf_len)
    {
        LoadNewBuffer();
    }

    return m_NextUChar_Buffer+m_NextUChar_BufferStart;
}

bool MadLines::NextUChar(MadUCQueue &ucqueue)
{
    return m_Encoding->NextUChar32(ucqueue, *this);
}

bool MadLines::NextUCharIs0x0A(void)
{
    wxFileOffset rest;
    wxByte* buf = BufferLoadBytes(rest, 4);
    if (buf == nullptr)
        return false;

    return m_Encoding->IsUChar32_LineFeed(buf, size_t(rest));
}

MadUCPair MadLines::PreviousUChar(/*IN_OUT*/MadLineIterator &lit, /*IN_OUT*/wxFileOffset &linepos)
{
    wxASSERT(linepos>=0);

    MadUCQueue ucq;

    if(linepos==0)
    {
        if(lit==m_LineList.begin())
            return MadUCPair(0, 0);

        --lit;
        linepos=lit->m_Size-lit->m_NewLineSize;

        InitNextUChar(lit, linepos);

        NextUChar(ucq);     // OD or OA

        if(NextUChar(ucq))  // 0D -> 0A
        {
            linepos+=ucq.front().second;
        }

        return ucq.back();
    }

    wxFileOffset lpos=linepos-4;    // max size of ucs4-char == 4 (utf-8,16,32)
    if(lpos<0) lpos=0;

    do
    {
        InitNextUChar(lit, lpos);
        NextUChar(ucq);

        if(lpos+ucq.back().second == linepos)
        {
            break; //found
        }
    }
    while(++lpos<linepos);

    if(lpos==linepos)
    {
        return MadUCPair(0, 0);
    }

    linepos=lpos;

    return ucq.back();
}


int MadLines::FindStringCase(MadUCQueue &ucqueue, MadStringIterator begin,
                            const MadStringIterator &end, size_t &len)
// 0: none, 1: first, 2:...
{
    wxASSERT(!ucqueue.empty());

    int idx = 1;
    ucs4_t firstuc = ucqueue.front().first;
    size_t ucsize = ucqueue.size();
    bool noNewLine = true;
    if(ucsize > 1)
    {
        ucs4_t uc = ucqueue.back().first;
        if(uc==0x0D || uc==0x0A)
        {
            noNewLine = false;
            --ucsize; // exclude NewLine char
        }
    }

    do
    {
        const wchar_t *cstr = begin->c_str();
        len = begin->Length();

        if(firstuc == (ucs4_t)cstr[0])
        {
            if(len == 1) return idx;

            if(ucsize < len && noNewLine)
            {
                while( NextUChar(ucqueue) )
                {
                    ucs4_t uc = ucqueue.back().first;
                    if(uc==0x0D || uc==0x0A)
                    {
                        noNewLine = false;
                        break;
                    }
                    if((++ucsize) >= len) break;
                }
            }

            if(ucsize >= len)
            {
                MadUCQueueIterator it = ucqueue.begin();
                ++it;
                while(*(++cstr) != 0)
                {
                    if((ucs4_t)*cstr != it->first) break;
                    ++it;
                }
                if(*cstr == 0) return idx;
            }
        }

        ++idx;
    }
    while(++begin != end);

    return 0;
}

int MadLines::FindStringNoCase(MadUCQueue &ucqueue, MadStringIterator begin,
                            const MadStringIterator &end, size_t &len)
// 0: none, 1: first, 2:...
{
    wxASSERT(!ucqueue.empty());

    int idx = 1;
    ucs4_t uc, firstuc = ucqueue.front().first;

    if(firstuc>='A' && firstuc<='Z')
    {
        firstuc |= 0x20; // to lower case
    }

    size_t ucsize = ucqueue.size();
    bool noNewLine = true;
    if(ucsize > 1)
    {
        ucs4_t uc = ucqueue.back().first;
        if(uc==0x0D || uc==0x0A)
        {
            noNewLine = false;
            --ucsize; // exclude NewLine char
        }
    }

    do
    {
        const wchar_t *cstr = begin->c_str();
        len = begin->Length();

        if(firstuc == (ucs4_t)cstr[0])
        {
            if(len == 1) return idx;

            if(ucsize < len && noNewLine)
            {
                while( NextUChar(ucqueue) )
                {
                    ucs4_t uc = ucqueue.back().first;
                    if(uc==0x0D || uc==0x0A)
                    {
                        noNewLine = false;
                        break;
                    }
                    if((++ucsize) >= len) break;
                }
            }

            if(ucsize >= len)
            {
                MadUCQueueIterator it = ucqueue.begin();
                ++it;
                while(*(++cstr) != 0)
                {
                    uc=it->first;
                    if(uc>='A' && uc<='Z')
                    {
                        uc |= 0x20; // to lower case
                    }

                    if((ucs4_t)*cstr != uc) break;
                    ++it;
                }
                if(*cstr == 0) return idx;
            }
        }

        ++idx;
    }
    while(++begin != end);

    return 0;
}


MadLineState MadLines::Reformat(MadLineIterator iter)
{
    ReformatCount = 1;

    iter->m_BracePairIndices.clear();

    if(iter->m_Size == 0)                    // is a empty line
        return iter->m_State;

    m_NextUChar_BufferLoadNew=false;
    m_NextUChar_BufferStart=0;
    m_NextUChar_BufferSize=0;

    MadLineIterator nextline = iter;
    ++nextline;

    iter->m_State.CommentOff = false;
    MadLineState state = iter->m_State;

    InitNextUChar(iter, 0);
    iter->m_NewLineSize = 0;

    // every line must be one row at first
    m_RowCount -= (iter->RowCount() - 1);
    iter->m_RowIndices.resize(2);

    MadRowIndex rowidx;
    int rowidx_idx = 0;

    MadUCQueue ucqueue;
    int ucwidth;
    size_t rowlen;                                     // byte-length
    size_t wordlength;
    int wordwidth;
    int wordisdelimiter=0;//1:is delimiter, -1:not delimiter, 0:other

    const size_t maxlinelength = m_MadEdit->m_MaxLineLength;
    int maxwidth = m_MadEdit->GetMaxWordWrapWidth();
    size_t bomlen=0;

    NextUChar(ucqueue);

    if (m_Encoding->IsUnicodeEncoding())
    {
        // Byte-Order Mark
        if(ucqueue.front().first == 0xFEFF && iter == m_LineList.begin())
        {
            // ignore BOM in first line
            rowidx.m_Start = ucqueue.front().second;
            bomlen = size_t(rowidx.m_Start);
            ucqueue.pop_front();
            NextUChar(ucqueue);
        }
    }

    ucs4_t firstuc, lastuc, prevuc;
    int index, notSpaceCount;
    size_t firstuclen, length, eatUCharCount, bracepos;

    int *bracexpos;
    int bracexpos_count;

    MadStringIterator sit, sitend;
    vector < wxString > strvec;
    MadSyntaxRange *srange = nullptr;
    if(state.RangeId)
        srange = m_Syntax->GetSyntaxRange(state.RangeId);

    int CheckState = m_Syntax->m_CheckState;

    if(m_Syntax->m_CaseSensitive)
        FindString=&MadLines::FindStringCase;
    else
        FindString=&MadLines::FindStringNoCase;

    if(!ucqueue.empty())
    {
        while(true)
        {
            firstuc = 0;
            wordlength = 0;
            wordwidth = 0;
            rowlen = 0;
            eatUCharCount = 0;

            notSpaceCount=0;
            bracepos=bomlen;    // first line only
            bracexpos=nullptr;
            bracexpos_count=0;
            bomlen=0;

            bool BeginOfLine=true;

            do
            {
                prevuc = firstuc;

                MadUCPair &ucp = ucqueue.front();
                firstuc = ucp.first;
                firstuclen = ucp.second;
                if(firstuc == 0x0D)
                {
                    wxASSERT(ucqueue.size() == 1);

                    if(prevuc != m_Syntax->nw_EscapeChar)
                    {
                        state.LineComment = 0;
                        state.Directive = 0;
                    }

                    if(NextUCharIs0x0A()) // DOS newline chars
                    {
                        NextUChar(ucqueue);
                        m_MadEdit->m_newline = &wxm::g_nl_dos;
                        iter->m_NewLineSize = ucqueue.back().second;
                    }
                    else if (m_MadEdit->m_newline->IsDefault())
                    {
                        m_MadEdit->m_newline = &wxm::g_nl_mac;
                    }

                    iter->m_NewLineSize += (wxByte)firstuclen;
                    ucqueue.clear();

                    break;
                }
                if(firstuc == 0x0A)
                {
                    wxASSERT(ucqueue.size() == 1);

                    if(prevuc != m_Syntax->nw_EscapeChar)
                    {
                        state.LineComment = 0;
                        state.Directive = 0;
                    }

                    if (m_MadEdit->m_newline->IsDefault())
                        m_MadEdit->m_newline = &wxm::g_nl_unix;

                    iter->m_NewLineSize = (wxByte)firstuclen;
                    ucqueue.clear();

                    break;
                }

                if(eatUCharCount == 0)
                {
                    ++eatUCharCount; // = 1;

                    if(CheckState)
                    {
                        if(firstuc < 0x100 && firstuc != 0x20 && firstuc != 0x09)
                        {
                            notSpaceCount++;

                            // check left/right brace
                            if(//state.CommentId==0 && state.StringId==0 &&
                                !m_Syntax->m_LeftBrace.empty())
                            {
                                bool ok=false;
                                index=(this->*FindString)(ucqueue, m_Syntax->m_LeftBrace.begin(),
                                                        m_Syntax->m_LeftBrace.end(), length);
                                if(index != 0)
                                {
                                    ok=true;
                                    ucs4_t uc=ucqueue[length-1].first;
                                    if(!m_Syntax->IsDelimiter(uc) && uc>0x20)// check last char for wholeword
                                    {
                                        if(ucqueue.size()>length || NextUChar(ucqueue))
                                        {
                                            uc = ucqueue[length].first;
                                            if(!m_Syntax->IsDelimiter(uc) && uc>0x20)
                                            {
                                                ok=false;
                                            }
                                        }
                                    }
                                    if(ok) // check first char for wholeword
                                    {
                                        if(!m_Syntax->IsDelimiter(firstuc) && firstuc>0x20)
                                        {
                                            if(!m_Syntax->IsDelimiter(prevuc) && prevuc>0x20)
                                            {
                                                ok=false;
                                            }
                                        }
                                    }
                                }

                                if(ok)
                                {
                                    //eatUCharCount = length;
                                    wxUint16 len=wxUint16(firstuclen);
                                    wxUint16 width=m_MadEdit->GetUCharWidth(firstuc);
                                    if(length>1)
                                    {
                                        size_t idx=1;
                                        do
                                        {
                                            len+=ucqueue[idx].second;
                                            width+=m_MadEdit->GetUCharWidth(ucqueue[idx].first);
                                        }
                                        while(++idx < length);
                                    }
                                    iter->m_BracePairIndices.push_back(BracePairIndex(0, width, bracepos, len, 1, index-1));
                                    bracexpos=&iter->m_BracePairIndices.back().XPos;
                                    bracexpos_count++;
                                }
                                else // check right brace
                                    if((index=(this->*FindString)(ucqueue, m_Syntax->m_RightBrace.begin(),
                                                                m_Syntax->m_RightBrace.end(), length)) != 0 )
                                {
                                    ok=true;
                                    ucs4_t uc=ucqueue[length-1].first;
                                    if(!m_Syntax->IsDelimiter(uc) && uc>0x20)// check last char for wholeword
                                    {
                                        if(ucqueue.size()>length || NextUChar(ucqueue))
                                        {
                                            uc = ucqueue[length].first;
                                            if(!m_Syntax->IsDelimiter(uc) && uc>0x20)
                                            {
                                                ok=false;
                                            }
                                        }
                                    }
                                    if(ok) // check first char for wholeword
                                    {
                                        if(!m_Syntax->IsDelimiter(firstuc) && firstuc>0x20)
                                        {
                                            if(!m_Syntax->IsDelimiter(prevuc) && prevuc>0x20)
                                            {
                                                ok=false;
                                            }
                                        }
                                    }

                                    if(ok)
                                    {
                                        //eatUCharCount = length;
                                        wxUint16 len=wxUint16(firstuclen);
                                        wxUint16 width=m_MadEdit->GetUCharWidth(firstuc);
                                        if(length>1)
                                        {
                                            size_t idx=1;
                                            do
                                            {
                                                len+=ucqueue[idx].second;
                                                width+=m_MadEdit->GetUCharWidth(ucqueue[idx].first);
                                            }
                                            while(++idx < length);
                                        }
                                        iter->m_BracePairIndices.push_back(BracePairIndex(0, width, bracepos, len, 0, index-1));
                                        bracexpos=&iter->m_BracePairIndices.back().XPos;
                                        bracexpos_count++;
                                    }
                                }
                            }

                            if(state.LineComment==0)
                            {
                                // check EscapeChar
                                if(firstuc == m_Syntax->nw_EscapeChar)
                                {
                                    if(ucqueue.size() == 1)
                                        NextUChar(ucqueue);

                                    if(ucqueue.size() != 1)
                                    {
                                        lastuc = ucqueue[1].first;
                                        if(lastuc!=0x0D && lastuc!=0x0A)
                                        {
                                            ++eatUCharCount;//=2
                                        }
                                    }
                                    goto _NOCHECK_;
                                }

                                // check block comment off
                                if(state.CommentId != 0)
                                {
                                    sit = m_Syntax->m_BlockCommentOff.begin();
                                    std::advance(sit, state.CommentId - 1);
                                    sitend = sit;
                                    ++sitend;
                                    index = (this->*FindString)(ucqueue, sit, sitend, length);
                                    if(index != 0)
                                    {
                                        state.CommentId = 0;
                                        eatUCharCount = length;

                                        iter->m_State.CommentOff = true;
                                    }
                                    goto _NOCHECK_;
                                }

                                // check string off
                                if(state.StringId != 0)
                                {
                                    //if(firstuc == m_Syntax->nw_EscapeChar)
                                    //{
                                        //if(ucqueue.size() == 1)
                                            //NextUChar(ucqueue);

                                        //if(ucqueue.size() != 1)
                                        //{
                                            //lastuc = ucqueue[1].first;

                                            //if(lastuc == m_Syntax->nw_EscapeChar
                                                //|| m_Syntax->m_StringChar.Find(lastuc)+1 == state.StringId)
                                            //{
                                                //++eatUCharCount;//=2
                                            //}
                                        //}
                                        //goto _NOCHECK_;
                                    //}

                                    if(m_Syntax->m_StringChar.Find(wxChar(firstuc))+1 == state.StringId)
                                    {
                                        state.StringId = 0;
                                        //eatUCharCount=1;
                                    }
                                    goto _NOCHECK_;

                                }

                                // check string on
                                if(!m_Syntax->m_StringChar.IsEmpty() && m_Syntax->IsInRange(state.RangeId, m_Syntax->m_StringInRange))
                                {
                                    if((index = m_Syntax->m_StringChar.Find(wxChar(firstuc))+1) != 0)
                                    {
                                        state.StringId = index;
                                        //eatUCharCount=1;

                                        goto _NOCHECK_;
                                    }
                                }

                                // check directive on
                                if(!m_Syntax->m_DirectiveLeadingAtBOL || BeginOfLine)
                                {
                                    if(notSpaceCount==1 && !m_Syntax->m_DirectiveLeading.IsEmpty())
                                    {
                                        if((index = m_Syntax->m_DirectiveLeading.Find(wxChar(firstuc))+1) != 0)
                                        {
                                            state.Directive = index;
                                            //eatUCharCount=1;

                                            goto _NOCHECK_;
                                        }
                                    }
                                }

                                // check block comment on
                                if(!m_Syntax->m_BlockCommentOn.empty())
                                {
                                    index =(this->*FindString)(ucqueue, m_Syntax->m_BlockCommentOn.begin(),
                                            m_Syntax->m_BlockCommentOn.end(), length);
                                    if(index != 0)    // got
                                    {
                                        // check InRange
                                        if(m_Syntax->IsInRange(state.RangeId,m_Syntax->m_BlockCommentInRange[index - 1]))
                                        {
                                            state.CommentId = index;
                                            //state.CommentOff = false;
                                            eatUCharCount = length;
                                            goto _NOCHECK_;
                                        }
                                    }
                                }

                                // check line comment
                                if(!m_Syntax->m_LineCommentAtBOL || BeginOfLine)
                                {
                                    if(!m_Syntax->m_LineComment.empty())
                                    {
                                        index =(this->*FindString)(ucqueue, m_Syntax->m_LineComment.begin(),
                                                m_Syntax->m_LineComment.end(), length);
                                        if(index != 0)    // got
                                        {
                                            // check InRange
                                            if(m_Syntax->IsInRange(state.RangeId, m_Syntax->m_LineCommentInRange))
                                            {
                                                if(state.RangeId==0)
                                                {
                                                    eatUCharCount = INT_MAX; // eat all line
                                                }
                                                else
                                                {
                                                    // range-off mark may be beyond linecomment (ex: javascript)
                                                    eatUCharCount = length;
                                                }
                                                state.LineComment = index;
                                                state.Directive = 0;
                                                goto _NOCHECK_;
                                            }
                                        }
                                    }
                                }
                            }

                            // check range off
                            if(state.RangeId != 0)
                            {
                                if(srange)
                                {
                                    strvec.push_back(srange->end);
                                    if((this->*FindString)(ucqueue, strvec.begin(), strvec.end(), length) != 0)
                                    {
                                        eatUCharCount = length;
                                        state.RangeId = 0;
                                        strvec.clear();
                                        goto _NOCHECK_;
                                    }
                                    strvec.clear();
                                }
                            }

                            // check range on
                            if(state.LineComment==0 && state.RangeId == 0 && !m_Syntax->m_RangeBeginString.empty())
                            {
                                index =(this->*FindString)(ucqueue, m_Syntax->m_RangeBeginString.begin(),
                                        m_Syntax->m_RangeBeginString.end(), length);
                                if(index != 0)
                                {
                                    eatUCharCount = length;
                                    state.RangeId = wxByte(m_Syntax->m_CustomRange[index - 1].id);
                                    srange = m_Syntax->GetSyntaxRange(state.RangeId);
                                    //goto _NOCHECK_;
                                }
                            }

                        }
                    }
                }

            _NOCHECK_:
                // eat one front uchar
                --eatUCharCount;

                ucqueue.pop_front();

                bracepos+=firstuclen;


                if(rowlen + int (firstuclen) > maxlinelength)     // wordwrap by line length
                {
                    bool move_word_to_next_line = false;
                    if(wordlength != maxlinelength)
                    {
                        if(wordisdelimiter>0)
                        {
                            if(m_Syntax->IsDelimiter(firstuc))
                                move_word_to_next_line = true;
                        }
                        else if(wordisdelimiter<0)
                        {
                            if(m_Syntax->IsNotDelimiter(firstuc))
                                move_word_to_next_line = true;
                        }
                    }

                    if(move_word_to_next_line)
                    {
                        rowidx.m_Width -= wordwidth;

                        if(bracexpos_count>0) // correct bracexpos after wordwrap
                        {
                            vector <BracePairIndex>::iterator it=iter->m_BracePairIndices.end();
                            do
                            {
                                --it;
                                if(it->XPos>=rowidx.m_Width)
                                {
                                    it->XPos-=rowidx.m_Width;
                                }
                            }
                            while(--bracexpos_count > 0);
                        }
                    }
                    bracexpos_count=0;

                    iter->m_RowIndices[rowidx_idx++] = rowidx;
                    iter->m_RowIndices.push_back(MadRowIndex());
                    m_RowCount++;

                    if(rowidx.m_Width > m_MaxLineWidth)
                        m_MaxLineWidth = rowidx.m_Width;

                    if(move_word_to_next_line)
                    {
                        rowidx.m_Start += rowlen - wordlength;
                        rowidx.m_Width = wordwidth;
                        rowlen = wordlength;
                    }
                    else
                    {
                        rowidx.m_Start += rowlen;
                        rowidx.m_Width = 0;
                        rowlen = 0;

                        wordlength = 0;
                        wordwidth = 0;
                    }
                }

                ucwidth = m_MadEdit->GetUCharWidth(firstuc);
                if(firstuc == 0x09)         // Tab char
                {
                    m_MadEdit->m_HasTab = true;

                    int tabwidth = m_MadEdit->m_TabColumns * m_MadEdit->GetSpaceCharFontWidth();

                    ucwidth = maxwidth - rowidx.m_Width;
                    if(ucwidth == 0)            // Tab at line-end
                    {
                        ucwidth = tabwidth;
                    }
                    else                                    // ucwidth >0
                    {
                        tabwidth = tabwidth - (rowidx.m_Width % tabwidth);
                        if(tabwidth < ucwidth)
                            ucwidth = tabwidth;
                    }
                }

                if(rowidx.m_Width + ucwidth > maxwidth)    // wordwrap by width
                {
                    bool move_word_to_next_line = false;
                    if(wordwidth != rowidx.m_Width && !m_MadEdit->HexPrinting())
                    {
                        if(wordisdelimiter>0)
                        {
                            if(m_Syntax->IsDelimiter(firstuc))
                                move_word_to_next_line = true;
                        }
                        else if(wordisdelimiter<0)
                        {
                            if(m_Syntax->IsNotDelimiter(firstuc))
                                move_word_to_next_line = true;
                        }
                    }

                    if(move_word_to_next_line)
                    {
                        rowidx.m_Width -= wordwidth;

                        if(bracexpos_count>0) // correct bracexpos after wordwrap
                        {
                            vector <BracePairIndex>::iterator it=iter->m_BracePairIndices.end();
                            do
                            {
                                --it;
                                if(it->XPos>=rowidx.m_Width)
                                {
                                    it->XPos-=rowidx.m_Width;
                                }
                            }
                            while(--bracexpos_count > 0);
                        }
                    }
                    bracexpos_count=0;

                    iter->m_RowIndices[rowidx_idx++] = rowidx;
                    iter->m_RowIndices.push_back(MadRowIndex());
                    m_RowCount++;

                    if(rowidx.m_Width > m_MaxLineWidth)
                        m_MaxLineWidth = rowidx.m_Width;

                    if(move_word_to_next_line)
                    {
                        rowidx.m_Start += rowlen - wordlength;
                        rowidx.m_Width = wordwidth;
                        rowlen = wordlength;
                    }
                    else
                    {
                        rowidx.m_Start += rowlen;
                        rowidx.m_Width = 0;
                        rowlen = 0;

                        wordlength = 0;
                        wordwidth = 0;
                    }
                }

                if(wordlength == 0)
                {
                    if(m_Syntax->IsDelimiter(firstuc))
                    {
                        wordisdelimiter = 1;
                        wordlength = firstuclen;
                        wordwidth = ucwidth;
                    }
                    else if(m_Syntax->IsNotDelimiter(firstuc))
                    {
                        wordisdelimiter = -1;
                        wordlength = firstuclen;
                        wordwidth = ucwidth;
                    }
                }
                else
                {
                    if(wordisdelimiter<0)
                    {
                        if(m_Syntax->IsNotDelimiter(firstuc))
                        {
                            wordlength += firstuclen;
                            wordwidth += ucwidth;
                        }
                        else if(m_Syntax->IsDelimiter(firstuc))
                        {
                            wordisdelimiter = 1;
                            wordlength = firstuclen;
                            wordwidth = ucwidth;
                        }
                        else
                        {
                            wordisdelimiter=0;
                            wordlength = 0;
                            wordwidth = 0;
                        }
                    }
                    else if(wordisdelimiter>0)
                    {
                        if(m_Syntax->IsDelimiter(firstuc))
                        {
                            wordlength += firstuclen;
                            wordwidth += ucwidth;
                        }
                        else if(m_Syntax->IsNotDelimiter(firstuc))
                        {
                            wordisdelimiter = -1;
                            wordlength = firstuclen;
                            wordwidth = ucwidth;
                        }
                        else
                        {
                            wordisdelimiter=0;
                            wordlength = 0;
                            wordwidth = 0;
                        }
                    }
                    else
                    {
                        //wordisdelimiter=0;
                        wordlength = 0;
                        wordwidth = 0;
                    }
                }

                if(bracexpos!=nullptr)
                {
                    *bracexpos = rowidx.m_Width;
                    bracexpos=nullptr;
                }

                rowidx.m_Width += ucwidth;
                rowlen += firstuclen;

                BeginOfLine=false;
            }
            while(!ucqueue.empty() || NextUChar(ucqueue));

            wxASSERT(ucqueue.empty());

            if(rowidx.m_Width > m_MaxLineWidth)
                m_MaxLineWidth = rowidx.m_Width;

            // set rowindex
            iter->m_RowIndices[rowidx_idx] = rowidx;
            rowidx.m_Start += rowlen;
            rowidx.m_Width = 0;
            iter->m_RowIndices[++rowidx_idx] = rowidx;

            if(m_NextUChar_Pos >= m_NextUChar_LineSize)
            {
                break;
            }

            iter->m_Size = m_NextUChar_Pos;

            // add a new line
            MadLineIterator nline = m_LineList.insert(nextline, MadLine());
            ++ReformatCount;

            //*** get corresponding block ***//
            MadBlockIterator blockiter = iter->m_Blocks.begin();
            wxFileOffset blockpos = m_NextUChar_Pos;
            if(blockpos >= blockiter->m_Size)
                do
                {
                    blockpos -= blockiter->m_Size;
                    ++blockiter;
                }
                while(blockpos >= blockiter->m_Size);

            if(blockpos != 0)                 //seperate block
            {
                // prev block
                MadBlock block_tmp = *blockiter;
                blockiter = iter->m_Blocks.insert(blockiter, block_tmp);
                blockiter->m_Size = blockpos;

                // this block
                ++blockiter;
                blockiter->m_Pos += blockpos;
                blockiter->m_Size -= blockpos;
            }
            //********************************

            //size_t nn = iter->m_Blocks.size();

            // insert blocks to new line
            nline->m_Blocks.insert(nline->m_Blocks.begin(), blockiter,
                                                        iter->m_Blocks.end());

            // erase blocks from old line
            iter->m_Blocks.erase(blockiter, iter->m_Blocks.end());

            nline->m_RowIndices.resize(2);
            nline->m_Size = m_NextUChar_LineSize - m_NextUChar_Pos;
            //state.CommentOff = false;
            nline->m_State = state;
            m_LineCount++;
            m_RowCount++;

            rowidx_idx = 0;
            rowidx.m_Start = 0;

            iter = nline;
            InitNextUChar(iter, 0);
            NextUChar(ucqueue);
        }
    }
    else
    {
        iter->m_RowIndices[0] = rowidx;
        iter->m_RowIndices[1] = rowidx;
    }

    m_NextUChar_BufferLoadNew=true;

    return state;
}

// reformat lines [first,last].
// if State isn't OK , may format all rest of lines [first, end)
// return reformated line count
size_t MadLines::Reformat(MadLineIterator first, MadLineIterator last)
{
    MadLineState state;
    MadLineIterator next = first, end = m_LineList.end();
    bool bContinue = true, bIsNotEnd = true, bStateIsNotOkay;
    size_t count = 0;
    do
    {
        if(bContinue && first == last)
            bContinue = false;

        // concatenate the lines that without newline-char or (lastwc=0x0D && firstuc==0x0A)
        if(++next != end)
        {
            ucs4_t uc = first->LastUCharIsNewLine(m_Encoding);
            if(uc == 0 || (uc == 0x0D && next->FirstUCharIs0x0A(m_Encoding)))
                do
                {
                    if(bContinue && next == last)
                    {
                        bContinue = false;
                    }

                    Append(first, next);

                    m_LineCount--;
                    m_RowCount -= next->RowCount();
                    next->Reset();

                    if((next = m_LineList.erase(next)) == end)
                    {
                        bIsNotEnd = false;
                        break;
                    }
                }
                while((uc = first->LastUCharIsNewLine(m_Encoding)) == 0
                            || (uc == 0x0D && next->FirstUCharIs0x0A(m_Encoding)));
        }
        else
            bIsNotEnd = false;

        // reformat this line
        state = Reformat(first);
        count += ReformatCount;

        if(bIsNotEnd)
        {
            // to next line
            first = next;

            if(first->m_State == state)
                bStateIsNotOkay = false;
            else
            {
                first->m_State = state;
                bStateIsNotOkay = true;
            }
        }
        else
        {
            first = next;
            --first;
            if(first->LastUCharIsNewLine(m_Encoding) != 0)
            {
                // add a empty line to end
                first = m_LineList.insert(next, MadLine());
                first->Empty();
                ++m_LineCount;
                ++m_RowCount;
                first->m_State = state;
            }

            m_EndState = state;
            break;
        }
    }
    while(bContinue | bStateIsNotOkay);

    return count;
}

void MadLines::RecountLineWidth(void)
{
    MadLineIterator iter = m_LineList.begin();
    MadLineIterator iterend = m_LineList.end();

    m_RowCount = 0;
    m_MaxLineWidth = 0;

    size_t rowidx_idx;
    MadRowIndex rowidx;
    MadUCQueue ucqueue;
    int ucwidth;
    size_t rowlen;                                     // byte-length
    size_t wordlength;
    int wordwidth;
    int wordisdelimiter=0;//1:is delimiter, -1:not delimiter, 0:other

    ucs4_t firstuc;
    size_t firstuclen;

    const size_t maxlinelength = m_MadEdit->m_MaxLineLength;
    const int maxwidth = m_MadEdit->GetMaxWordWrapWidth();

    BracePairIndex *bpi;
    vector <BracePairIndex>::iterator bpit, bpitend;
    size_t bracepos, bracelen=0, bracemaxlen=0;
    wxUint16 *bracewidth=nullptr;
    vector<int*> bracexpos_thisrow;

    do
    {
        rowidx_idx = 0;

        rowidx = iter->m_RowIndices.front();
        rowidx.m_Width = 0;
        InitNextUChar(iter, rowidx.m_Start);

        if(iter->m_RowIndices.size() != 2)
        {
            iter->m_RowIndices.resize(2);
        }

        // correct BracePairIndex
        if(iter->m_BracePairIndices.size() != 0)
        {
            bpit = iter->m_BracePairIndices.begin();
            bpitend = iter->m_BracePairIndices.end();
            bpi = &(*bpit);
        }
        else
        {
            bpi=nullptr;
        }
        bracexpos_thisrow.clear();

        ++m_RowCount;

        NextUChar(ucqueue);
        if(!ucqueue.empty())
        {
            wordlength = 0;
            wordwidth = 0;
            rowlen = 0;
            bracepos=0;

            do
            {
                MadUCPair &ucp = ucqueue.front();
                firstuc = ucp.first;
                firstuclen = ucp.second;
                if(firstuc == 0x0D)
                {
                    wxASSERT(ucqueue.size() == 1);

                    if(NextUCharIs0x0A()) // DOS newline chars
                    {
                        NextUChar(ucqueue);
                    }
                    ucqueue.clear();

                    break;
                }
                if(firstuc == 0x0A)
                {
                    wxASSERT(ucqueue.size() == 1);

                    ucqueue.clear();
                    break;
                }

                ucqueue.pop_front();

                if(rowlen + int (firstuclen) > maxlinelength)     // wordwrap by line length
                {
                    bool move_word_to_next_line = false;
                    if(wordlength != maxlinelength)
                    {
                        if(wordisdelimiter>0)
                        {
                            if(m_Syntax->IsDelimiter(firstuc))
                                move_word_to_next_line = true;
                        }
                        else if(wordisdelimiter<0)
                        {
                            if(m_Syntax->IsNotDelimiter(firstuc))
                                move_word_to_next_line = true;
                        }
                    }

                    if(move_word_to_next_line)
                    {
                        rowidx.m_Width -= wordwidth;

                        if(!bracexpos_thisrow.empty())
                        {
                            vector<int*>::iterator it=bracexpos_thisrow.begin();
                            do
                            {
                                if(*(*it) >= rowidx.m_Width)
                                    *(*it) -= rowidx.m_Width;
                            }
                            while(++it != bracexpos_thisrow.end());
                        }
                    }
                    bracexpos_thisrow.clear();

                    iter->m_RowIndices[rowidx_idx++] = rowidx;
                    iter->m_RowIndices.push_back(MadRowIndex());
                    m_RowCount++;

                    if(rowidx.m_Width > m_MaxLineWidth)
                        m_MaxLineWidth = rowidx.m_Width;

                    if(move_word_to_next_line)
                    {
                        rowidx.m_Start += rowlen - wordlength;
                        rowidx.m_Width = wordwidth;
                        rowlen = wordlength;
                    }
                    else
                    {
                        rowidx.m_Start += rowlen;
                        rowidx.m_Width = 0;
                        rowlen = 0;

                        wordlength = 0;
                        wordwidth = 0;
                    }
                }

                ucwidth = m_MadEdit->GetUCharWidth(firstuc);
                if(firstuc == 0x09)         // Tab char
                {
                    //m_MadEdit->FHasTab = true;

                    int tabwidth = m_MadEdit->m_TabColumns * m_MadEdit->GetSpaceCharFontWidth();

                    ucwidth = maxwidth - rowidx.m_Width;
                    if(ucwidth == 0)            // Tab at line-end
                    {
                        ucwidth = tabwidth;
                    }
                    else                                    // ucwidth >0
                    {
                        tabwidth = tabwidth - (rowidx.m_Width % tabwidth);
                        if(tabwidth < ucwidth)
                            ucwidth = tabwidth;
                    }
                }

                if(rowidx.m_Width + ucwidth > maxwidth)    // wordwrap by width
                {
                    bool move_word_to_next_line = false;
                    if(wordwidth != rowidx.m_Width)
                    {
                        if(wordisdelimiter>0)
                        {
                            if(m_Syntax->IsDelimiter(firstuc))
                                move_word_to_next_line = true;
                        }
                        else if(wordisdelimiter<0)
                        {
                            if(m_Syntax->IsNotDelimiter(firstuc))
                                move_word_to_next_line = true;
                        }
                    }

                    if(move_word_to_next_line)
                    {
                        rowidx.m_Width -= wordwidth;

                        if(!bracexpos_thisrow.empty())
                        {
                            vector<int*>::iterator it=bracexpos_thisrow.begin();
                            do
                            {
                                if(*(*it) >= rowidx.m_Width)
                                    *(*it) -= rowidx.m_Width;
                            }
                            while(++it != bracexpos_thisrow.end());
                        }
                    }
                    bracexpos_thisrow.clear();

                    iter->m_RowIndices[rowidx_idx++] = rowidx;
                    iter->m_RowIndices.push_back(MadRowIndex());
                    m_RowCount++;

                    if(rowidx.m_Width > m_MaxLineWidth)
                        m_MaxLineWidth = rowidx.m_Width;

                    if(move_word_to_next_line)
                    {
                        rowidx.m_Start += rowlen - wordlength;
                        rowidx.m_Width = wordwidth;
                        rowlen = wordlength;
                    }
                    else
                    {
                        rowidx.m_Start += rowlen;
                        rowidx.m_Width = 0;
                        rowlen = 0;

                        wordlength = 0;
                        wordwidth = 0;
                    }
                }

                if(wordlength == 0)
                {
                    if(m_Syntax->IsDelimiter(firstuc))
                    {
                        wordisdelimiter = 1;
                        wordlength = firstuclen;
                        wordwidth = ucwidth;
                    }
                    else if(m_Syntax->IsNotDelimiter(firstuc))
                    {
                        wordisdelimiter = -1;
                        wordlength = firstuclen;
                        wordwidth = ucwidth;
                    }
                }
                else
                {
                    if(wordisdelimiter<0)
                    {
                        if(m_Syntax->IsNotDelimiter(firstuc))
                        {
                            wordlength += firstuclen;
                            wordwidth += ucwidth;
                        }
                        else if(m_Syntax->IsDelimiter(firstuc))
                        {
                            wordisdelimiter = 1;
                            wordlength = firstuclen;
                            wordwidth = ucwidth;
                        }
                        else
                        {
                            wordisdelimiter = 0;
                            wordlength = 0;
                            wordwidth = 0;
                        }
                    }
                    else if(wordisdelimiter>0)
                    {
                        if(m_Syntax->IsDelimiter(firstuc))
                        {
                            wordlength += firstuclen;
                            wordwidth += ucwidth;
                        }
                        else if(m_Syntax->IsNotDelimiter(firstuc))
                        {
                            wordisdelimiter = -1;
                            wordlength = firstuclen;
                            wordwidth = ucwidth;
                        }
                        else
                        {
                            wordisdelimiter=0;
                            wordlength = 0;
                            wordwidth = 0;
                        }
                    }
                    else
                    {
                        //wordisdelimiter=0;
                        wordlength = 0;
                        wordwidth = 0;
                    }
                }

                if(bpi!=nullptr)
                {
                    wxASSERT(bracepos<=bpi->LinePos);

                    if(bpi->LinePos==(wxFileOffset)bracepos)
                    {
                        bpi->XPos = rowidx.m_Width;

                        bracexpos_thisrow.push_back(&bpi->XPos);

                        bracemaxlen=bpi->Length;
                        bracelen=0;
                        bracewidth=&bpi->Width;
                        *bracewidth=0;

                        if((++bpit)==bpitend)
                        {
                            bpi=nullptr;
                        }
                        else
                        {
                            bpi=&(*bpit);
                        }
                    }
                }
                bracepos+=firstuclen;

                if(bracewidth!=nullptr)
                {
                    *bracewidth+=ucwidth;
                    if((bracelen+=firstuclen)>=bracemaxlen)
                    {
                        bracewidth=nullptr;
                    }
                }

                rowidx.m_Width += ucwidth;
                rowlen += firstuclen;
            }
            while(!ucqueue.empty() || NextUChar(ucqueue));

            if(rowidx.m_Width > m_MaxLineWidth)
                m_MaxLineWidth = rowidx.m_Width;

            // set MadRowIndex
            iter->m_RowIndices[rowidx_idx] = rowidx;
            rowidx.m_Start += rowlen;
            rowidx.m_Width = 0;
            iter->m_RowIndices[++rowidx_idx] = rowidx;
        }
        else
        {
            iter->m_RowIndices.back() = rowidx;
        }

        wxASSERT(bpi==nullptr);
    }
    while(++iter != iterend);

}

void MadLines::Append(const MadLineIterator &lit1, const MadLineIterator &lit2)
{
    if(lit2->m_Size == 0)                    // is a empty line
        return;

    lit1->m_Size += lit2->m_Size;

    MadBlockVector &blks1 = lit1->m_Blocks;
    MadBlockVector &blks2 = lit2->m_Blocks;

    MadBlock &blk1 = blks1.back();
    MadBlockIterator bit2 = blks2.begin();

    if(blk1.m_Data == bit2->m_Data && (blk1.m_Pos + blk1.m_Size) == bit2->m_Pos)
    {
        blk1.m_Size += bit2->m_Size;
        ++bit2;
    }

    blks1.insert(blks1.end(), bit2, blks2.end());
}

//===========================================================================

MadSyntax* GetFileSyntax(const wxString& filename, const wxByte* buf, int sz)
{
    wxFileName fn(filename);
    MadSyntax* syntax = MadSyntax::GetSyntaxByExt(fn.GetExt());
    if (syntax != nullptr)
        return syntax;

    syntax = MadSyntax::GetSyntaxByFirstLine(buf, sz);
    if (syntax != nullptr)
        return syntax;

	syntax = MadSyntax::GetSyntaxByFileName(fn.GetName());
    if (syntax != nullptr)
        return syntax;

    return MadSyntax::GetSyntaxByTitle(MadPlainTextTitle);
}

void MadLines::InitFileSyntax()
{
    wxFont *font=m_MadEdit->m_TextFont;
    m_Syntax->InitNextWord1(m_MadEdit->m_Lines, m_MadEdit->m_WordBuffer, m_MadEdit->m_WidthBuffer, font->GetFaceName(), font->GetPointSize(), font->GetFamily());
    m_MadEdit->m_Syntax = m_Syntax;
}

bool MadLines::LoadFromFile(const wxString &filename, const wxString &encoding)
{
    MadFileData *fd = new MadFileData(filename);

    if(!fd->OpenSuccess())
    {
        delete fd;
        return false;
    }

    if(m_FileData)
        delete m_FileData;
    m_FileData = fd;

    if(m_TmpFileData)
    {
        wxString tmpfile = m_TmpFileData->m_Name;
        delete m_TmpFileData;
        wxRemoveFile(tmpfile);
        m_TmpFileData = nullptr;
    }

    m_MemData->Reset();

    // reserve one empty line
    Empty(false);

    m_Name = filename;
    m_ReadOnly = fd->IsReadOnly();
    m_MaxLineWidth = 0;

    m_MadEdit->m_HasTab = false;
    m_MadEdit->m_newline = &wxm::g_nl_default;

    m_MadEdit->m_LoadingFile = true;

    if(m_MadEdit->m_EditMode == emHexMode)
        m_MadEdit->SetEditMode(emTextMode);

    const int max_detecting_size = 4096;
    int sz;
    if(m_FileData->m_Size > max_detecting_size)
        sz = max_detecting_size;
    else
        sz = size_t(m_FileData->m_Size);

    wxString defaultenc;
    if(encoding.IsEmpty())
    {
        m_MadEdit->m_Config->Read(wxT("/wxMEdit/DefaultEncoding"), &defaultenc);
    }
    else
    {
        defaultenc = encoding;
    }

    if(sz == 0)
    {
        delete m_FileData;
        m_FileData = nullptr;

        // reload syntax
        delete m_Syntax;

        m_Syntax = GetFileSyntax(filename, nullptr, 0);

		InitFileSyntax();

        m_MadEdit->m_LoadingFile = false;

        if(m_Syntax->m_Encoding.IsEmpty())
            m_MadEdit->SetEncoding(defaultenc);
        else
            m_MadEdit->SetEncoding(m_Syntax->m_Encoding);

        return true;
    }

    // set size
    m_Size = m_FileData->m_Size;
    MadLineIterator iter = m_LineList.begin();
    iter->m_Size = m_Size;

    // set line's blocks
    iter->m_Blocks[0].m_Size = m_Size;

    // set line's row indices
    iter->m_RowIndices[1].m_Start = m_Size;

    long MaxSizeToLoad;
    m_MadEdit->m_Config->Read(wxT("/wxMEdit/MaxSizeToLoad"), &MaxSizeToLoad, 20*1000*1000);

    wxMemorySize memsize=wxGetFreeMemory();

    wxByte *buf;
    if(m_Size<=wxFileOffset(MaxSizeToLoad) && memsize>0 && wxMemorySize(m_Size * 2 + 15 * 1024 * 1024)<memsize)    // load filedata to MemData
    {
        buf = m_FileData->m_Buffer1;
        int ss = 0;
        int bs = BUFFER_SIZE;
        if(BUFFER_SIZE > m_Size)
        {
            bs = int(m_Size);
        }
        m_MemData->Put(buf, bs);
        ss += bs;

        while(ss < m_Size)
        {
        // now bs == BUFFER_SIZE;
        if(BUFFER_SIZE > (m_Size - ss))
        {
            bs = int(m_Size - ss);
        }

        m_FileData->Get(ss, buf, bs);
        m_MemData->Put(buf, bs);

        ss += bs;
        }

        delete m_FileData;
        m_FileData = nullptr;

        iter->m_Blocks[0].m_Data = m_MemData;
        buf = m_MemData->m_Buffers.front();
    }
    else
    {
        iter->m_Blocks[0].m_Data = m_FileData;
        buf = m_FileData->m_Buffer1;
    }


    // reload syntax
    delete m_Syntax;

    if(m_MadEdit->m_UseDefaultSyntax)
    {
        m_Syntax = new MadSyntax(false);
        if(m_MadEdit->m_SearchWholeWord==false)
        {
            m_Syntax->m_Delimiter.Empty();
        }
        m_Syntax->m_CaseSensitive = true;
    }
    else
    {
        m_Syntax = GetFileSyntax(filename, buf, sz);
    }

	InitFileSyntax();

    long maxtextfilesize;
    wxString oldpath=m_MadEdit->m_Config->GetPath();
    m_MadEdit->m_Config->Read(wxT("/wxMEdit/MaxTextFileSize"), &maxtextfilesize, 10*1000*1000);
    m_MadEdit->m_Config->SetPath(oldpath);

    bool preset=false;
    bool hexmode=false;
    bool skip_utf8=false;

    if(m_Size>=maxtextfilesize)
    {
        hexmode = true;
    }
    else
    {
        preset = PresetFileEncoding(encoding, buf, sz);
        skip_utf8 = !preset;
    }

    if(!preset)
    {
        SetFileEncoding(encoding, defaultenc, buf, sz, skip_utf8);

        if(!hexmode)
            hexmode = IsBinaryData(buf, sz);
    }

    if(hexmode)
    {
        m_MaxLineWidth = -1;       // indicate the data is not text data
        m_MadEdit->SetEditMode(emHexMode);
    }
    else
    {
        Reformat(iter, iter);
    }

    m_MadEdit->m_LoadingFile = false;

    return true;
}


bool MadLines::PresetFileEncoding(const wxString& encoding, const wxByte* buf, size_t sz)
{
    if(!encoding.IsEmpty())
    {
        m_MadEdit->SetEncoding(encoding);
        return true;
    }

    wxString wxmenc;
    if(wxm::MatchWXMEncoding(wxmenc, buf, sz))
    {
        m_MadEdit->SetEncoding(wxmenc);
        return true;
    }

    return false;
}

void MadLines::SetFileEncoding(const wxString& encoding, const wxString& defaultenc, 
                               const wxByte* buf, size_t sz, bool skip_utf8)
{
    if(!encoding.IsEmpty())
    {
        m_MadEdit->SetEncoding(encoding);
        return;
    }

    if(!m_Syntax->m_Encoding.IsEmpty())
    {
        m_MadEdit->SetEncoding(m_Syntax->m_Encoding);
        return;
    }

    wxm::WXMEncodingID enc=m_MadEdit->m_Encoding->GetEncoding();
    if(wxm::WXMEncodingManager::IsSimpleUnicodeEncoding(enc))
    {
        // use default encoding
        enc=wxm::WXMEncodingManager::Instance().NameToEncoding(defaultenc);
    }

    // use Encoding Detector
    wxm::DetectEncoding(buf, sz, enc, skip_utf8);

    m_MadEdit->SetEncoding(wxm::WXMEncodingManager::Instance().EncodingToName(enc));
}

//===========================================================================

bool TruncateFile(const wxString &filename, wxFileOffset size)
{
#ifdef __WXMSW__

    HANDLE handle = CreateFile(filename.c_str(),      // file to open
                               GENERIC_WRITE,         // open for writing
                               0,                     // do not share
                               nullptr,               // default security
                               OPEN_ALWAYS,           // open the file, if not exist then create it
                               FILE_ATTRIBUTE_NORMAL, // normal file
                               NULL);

    if(handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    LARGE_INTEGER oldsize;
    //if(GetFileSizeEx(handle, &oldsize)==FALSE)

    oldsize.LowPart = GetFileSize(handle, (DWORD *) & oldsize.HighPart);
    if(oldsize.LowPart==INVALID_FILE_SIZE && GetLastError()!=NO_ERROR)
    {
        CloseHandle(handle);
        return false;
    }

    if(size==oldsize.QuadPart) // same size, do nothing
    {
        CloseHandle(handle);
        return true;
    }

    if(size>oldsize.QuadPart)
    {
        /**
        if(SetFileValidData(handle, size)==FALSE)
        {
            CloseHandle(handle);
            return false;
        }
        **/

        wxFileOffset s=size-oldsize.QuadPart;

        DWORD ret=SetFilePointer(handle, 0, nullptr, FILE_END);
        if(ret==INVALID_SET_FILE_POINTER && GetLastError()!=NO_ERROR)
        {
            CloseHandle(handle);
            return false;
        }

        static char *buf=nullptr;
        static vector<char> buffervector;
        if(buf==nullptr)
        {
            buffervector.resize(256*1024);
            buf=&buffervector[0];
            ZeroMemory(buf, 256*1024);
        }

        DWORD len=256*1024, wlen;

        do
        {
            if(len>s) len = DWORD(s);
            WriteFile(handle, buf, len, &wlen, nullptr);
        }
        while((s-=len)>0);

    }
    else    // less
    {
        oldsize.QuadPart=size;
        DWORD ret=SetFilePointer(handle, oldsize.LowPart, &oldsize.HighPart, FILE_BEGIN);
        if(ret==INVALID_SET_FILE_POINTER && GetLastError()!=NO_ERROR)
        {
            CloseHandle(handle);
            return false;
        }
    }

    bool ok= (SetEndOfFile(handle)!=FALSE);
    CloseHandle(handle);
    return ok;

#else

    int utf8test=MadFileNameIsUTF8(filename);
    MadConvFileName_WC2MB_UseLibc uselibc(utf8test<0);
    if(utf8test==0)
    {
        // create a new file
        wxFile f(filename, wxFile::write);
    }

    return 0==truncate( wxConvFile.cWX2MB(filename.c_str()), size );

#endif
}

void MadLines::WriteBlockToData(MadOutData *data, const MadBlockIterator &bit)
{
    wxASSERT(data!=nullptr);

    size_t bs = BUFFER_SIZE;
    wxFileOffset size = bit->m_Size;
    wxFileOffset pos = bit->m_Pos;

    do
    {
        if(BUFFER_SIZE > size)
            bs = size_t(size);

        bit->m_Data->Get(pos, m_WriteBuffer, bs);
        pos += bs;

        data->Put(m_WriteBuffer, bs);
    }
    while((size -= bs) > 0);
}

void MadLines::WriteToFile(wxFile &file, MadFileData *oldfd, MadFileData *newfd)
{
    if(m_Size > 0)
    {
        if(m_WriteBuffer == nullptr)
        {
            m_WriteBufferVector.resize(BUFFER_SIZE);
            m_WriteBuffer = &m_WriteBufferVector[0];
        }

        MadLineIterator lit = m_LineList.begin();
        wxFileOffset filepos=0;

        do
        {
            if(lit->m_Size != 0)
            {
                MadBlockIterator bit=lit->m_Blocks.begin();
                size_t count=lit->m_Blocks.size();
                do
                {
                    size_t bs = BUFFER_SIZE;
                    wxFileOffset size = bit->m_Size;
                    wxFileOffset pos = bit->m_Pos;

                    do
                    {
                        if(BUFFER_SIZE > size)
                            bs = size_t(size);

                        bit->m_Data->Get(pos, m_WriteBuffer, bs);
                        pos += bs;

                        file.Write(m_WriteBuffer, bs);
                    }
                    while((size -= bs) > 0);

                    if(bit->m_Data == oldfd)
                    {
                        bit->m_Data = newfd;
                        bit->m_Pos = filepos;
                    }
                    filepos+=bit->m_Size;

                    ++bit;
                }
                while(--count != 0);
            }
        }
        while(++lit != m_LineList.end());
    }
}

wxFileOffset MadLines::GetMaxTempSize(const wxString &filename)
{
    if(m_FileData==nullptr || filename!=m_Name || m_Size==0)
        return 0;

    wxFileOffset maxsize=0, temppos=0, tempsize=0, filepos=0;
    wxFileOffset writable_size=0, writepos=0;

    MadLineIterator write_lit;
    MadBlockIterator write_bit, write_bitend;

    MadLineIterator file_lit;
    MadBlockIterator file_bit, file_bitend;

    bool file_lit_not_set=true, ignore_overlap=false;

    write_lit=m_LineList.begin();
    write_bit=write_lit->m_Blocks.begin();
    write_bitend=write_lit->m_Blocks.end();

    do  // per block
    {
        // find the block that must write back to file
        while(writable_size==0 && write_bit->m_Data == m_FileData &&
              write_bit->m_Pos  == writepos )
        {
            // this block is not necessary to write back
            writepos+=write_bit->m_Size;
            file_lit_not_set=true;

            if(++write_bit == write_bitend)
            {
                if(writepos==m_Size) // end of file
                    break;

                wxASSERT(writepos<m_Size);

                ++write_lit;
                write_bit=write_lit->m_Blocks.begin();
                write_bitend=write_lit->m_Blocks.end();
            }
        }

        if(writepos==m_Size) // end of file
            break;

        // make sure that writable_size >= write_bit->m_Size
        if(writable_size < write_bit->m_Size)
        {
            if(file_lit_not_set)
            {
                file_lit=write_lit;
                file_bit=write_bit;
                file_bitend=write_bitend;
                file_lit_not_set=false;
                ignore_overlap=false;
            }

            do
            {
                if(file_bit->m_Data==m_FileData)
                {
                    filepos=file_bit->m_Pos;

                    if(ignore_overlap==false)
                    {
                        ignore_overlap=true;

                        wxFileOffset overlap_size= file_bit->m_Pos - (writepos+writable_size);
                        if( (writable_size+=overlap_size) >= write_bit->m_Size )
                        {
                            break;
                        }
                    }

                    // move the block to tempdata

                    file_bit->m_Data=nullptr; // temp data

                    tempsize+=file_bit->m_Size;
                    writable_size+=file_bit->m_Size;

                    if(tempsize>maxsize)
                    {
                        maxsize=tempsize;
                    }

                }

                ignore_overlap=false;

                // to next block
                if(++file_bit==file_bitend)
                {
                    if(++file_lit != m_LineList.end() && file_lit->m_Size!=0)
                    {
                        file_bit=file_lit->m_Blocks.begin();
                        file_bitend=file_lit->m_Blocks.end();
                    }
                    else // at end of file
                    {
                        writable_size+=m_Size-(writepos+writable_size);
                        wxASSERT(writable_size >= write_bit->m_Size);
                    }

                }

            }
            while(writable_size < write_bit->m_Size);
        }

        // write the block to file

        if(write_bit->m_Data==m_FileData)
        {
            if(file_lit_not_set==false && write_bit->m_Pos >= filepos)
            {
                file_lit_not_set=true;
            }
        }
        else if(write_bit->m_Data==nullptr)  // this block is in the tempdata
        {
            write_bit->m_Data=m_FileData;

            temppos+=write_bit->m_Size;
            tempsize-=write_bit->m_Size;

            wxASSERT(tempsize>=0);

            if(tempsize==0)
            {
                temppos=0;
            }
        }

        writepos+=write_bit->m_Size;
        writable_size-=write_bit->m_Size;

        if(file_lit_not_set==false && writable_size==0 && tempsize==0)
        {
            file_lit_not_set=true;
        }

        // to next block
        if(++write_bit == write_bitend)
        {
            if(writepos==m_Size) // end of file
                break;

            wxASSERT(writepos<m_Size);

            ++write_lit;
            write_bit=write_lit->m_Blocks.begin();
            write_bitend=write_lit->m_Blocks.end();
        }

    }
    while(writepos<m_Size);

    wxASSERT(tempsize==0 && writable_size>=0);

    return maxsize;
}

void MadLines::DetectSyntax(const wxString &filename)
{
    size_t sz = 0;
    wxByte buf[wxm::FIRSTLINE_SYNTAXPATTEN_MAXLEN] = {'\0'};
    if (!m_LineList.empty())
    {
        MadLine& line = m_LineList.front();
        sz = std::min(wxm::FIRSTLINE_SYNTAXPATTEN_MAXLEN, (size_t)line.m_Size);
        line.Get(0, buf, sz);
    }

    boost::scoped_ptr<MadSyntax> tmp_Syntax(GetFileSyntax(filename, buf, (int)sz));
    if(tmp_Syntax->GetTitle() != m_Syntax->GetTitle())
        m_MadEdit->SetSyntax(tmp_Syntax->GetTitle());
}

bool MadLines::SaveToFile(const wxString &filename, const wxString &tempdir)
{
    if (!m_manual)
        DetectSyntax(filename);

    if(m_FileData == nullptr)
    {
        int utf8test=MadFileNameIsUTF8(filename);
#ifndef __WXMSW__
        if(utf8test==0) // not found
        {
            // test parent dir
            wxFileName fn(filename);
            utf8test=MadDirExists(fn.GetPath(wxPATH_GET_VOLUME));
        }
#endif
        MadConvFileName_WC2MB_UseLibc uselibc(utf8test<0);

        wxFile file(filename, wxFile::write);
        if(!file.IsOpened())
        {
            return false;   // open error;
        }

        wxFileOffset filesize=file.SeekEnd(0);

        file.Seek(0);
        WriteToFile(file, nullptr, nullptr);
        file.Close();

        if(filesize>m_Size)
        {
            TruncateFile(filename, m_Size);
        }

        if(filename != m_Name)
        {
            m_Name = filename;
            m_ReadOnly = false;
        }

        return true;
    }

    if(filename != m_Name) // save to another file
    {
        int utf8test=MadFileNameIsUTF8(filename);
        bool createnew=false;
        if(utf8test==0) // not found
        {
#ifndef __WXMSW__
            // test parent dir
            wxFileName fn(filename);
            utf8test=MadDirExists(fn.GetPath(wxPATH_GET_VOLUME));
#endif
            createnew=true;
        }
        MadConvFileName_WC2MB_UseLibc uselibc(utf8test<0);

        if(createnew)
        {
            // create it
            wxFile file(filename, wxFile::write);
        }

        MadFileData *fd = new MadFileData(filename);

        if(!fd->OpenSuccess() || fd->IsReadOnly())
        {
            delete fd;
            return false;
        }

        wxFileOffset filesize=fd->m_File.SeekEnd(0);

        fd->m_File.Seek(0);
        WriteToFile(fd->m_File, m_FileData, fd);    // replace old fd by new fd

        if(filesize>m_Size)
        {
            fd->m_File.Close();
            TruncateFile(filename, m_Size); // truncate the file
            fd->OpenFile();                 // reopen the file
        }

        fd->m_Size=m_Size;
        fd->m_Buf1Pos = -1;
        fd->m_Buf2Pos = -1;

        delete m_FileData;
        m_FileData=fd;

        m_Name=filename;
        m_ReadOnly=false;

        return true;
    }


    // write back to the same file

    if(m_ReadOnly) return false;

    if(m_Size==0)
    {
        if(m_FileData->m_File.SeekEnd(0)!=0)
        {
            m_FileData->m_File.Close();
            TruncateFile(filename, m_Size); // truncate the file
            m_FileData->OpenFile();         // reopen the file
            m_FileData->m_Buf1Pos = -1;
            m_FileData->m_Buf2Pos = -1;
        }
        return true;
    }

    if(m_WriteBuffer == nullptr)
    {
        m_WriteBufferVector.resize(BUFFER_SIZE);
        m_WriteBuffer = &m_WriteBufferVector[0];
    }

    MadConvFileName_WC2MB_UseLibc uselibc(MadFileNameIsUTF8(filename)<0);

    wxString tempfilename;

    if(!tempdir.IsEmpty())
    {
        wxFileName fn(filename);
        tempfilename=tempdir+fn.GetFullName()+wxT(".wxMEdit.tmp");

        int id=0;
        while(wxFileExists(tempfilename + wxString::Format(wxT("%d"), id))) ++id;
        tempfilename<<id;   // append "id"
    }

    MadFileData *tempfiledata=nullptr;
    MadMemData *tempmemdata=nullptr;
    MadInData *tempindata=nullptr;
    MadOutData *tempoutdata=nullptr;

#ifdef _DEBUG
    wxFileOffset maxsize=0;
#endif

    wxFileOffset temppos=0, tempsize=0, filepos=0;
    wxFileOffset writable_size=0, writepos=0;

    MadLineIterator write_lit;
    MadBlockIterator write_bit, write_bitend;

    MadLineIterator file_lit;
    MadBlockIterator file_bit, file_bitend;

    bool file_lit_not_set=true, ignore_overlap=false;

    write_lit=m_LineList.begin();
    write_bit=write_lit->m_Blocks.begin();
    write_bitend=write_lit->m_Blocks.end();

    do  // per block
    {
        // find the block that must write back to file
        while(writable_size==0 && write_bit->m_Data == m_FileData &&
              write_bit->m_Pos  == writepos )
        {
            // this block is not necessary for writing back
            writepos+=write_bit->m_Size;
            file_lit_not_set=true;

            if(++write_bit == write_bitend)
            {
                if(writepos==m_Size) // end of file
                    break;

                wxASSERT(writepos<m_Size);

                ++write_lit;
                write_bit=write_lit->m_Blocks.begin();
                write_bitend=write_lit->m_Blocks.end();
            }
        }

        if(writepos==m_Size) // end of file
            break;

        // make sure that writable_size >= write_bit->m_Size
        if(writable_size < write_bit->m_Size)
        {
            if(file_lit_not_set)
            {
                file_lit=write_lit;
                file_bit=write_bit;
                file_bitend=write_bitend;
                file_lit_not_set=false;
                ignore_overlap=false;
            }

            do
            {
                if(file_bit->m_Data==m_FileData)
                {
                    filepos=file_bit->m_Pos;

                    if(ignore_overlap==false)
                    {
                        ignore_overlap=true;

                        wxFileOffset overlap_size= file_bit->m_Pos - (writepos+writable_size);
                        if( (writable_size+=overlap_size) >= write_bit->m_Size )
                        {
                            break;
                        }
                    }

                    // move the block to tempdata
                    if(tempoutdata==nullptr)
                    {
                        if(tempfilename.IsEmpty())
                        {
                            tempmemdata=new MadMemData();

                            tempoutdata=tempmemdata;
                            tempindata=tempmemdata;
                        }
                        else
                        {
                            wxFile tf(tempfilename, wxFile::write);
                            tf.Close();
                            tempfiledata=new MadFileData(tempfilename);

                            tempoutdata=tempfiledata;
                            tempindata=tempfiledata;
                        }
                    }

                    wxFileOffset pos0=temppos+tempsize;

                    if(tempfiledata!=nullptr)
                    {
                        tempfiledata->m_SavePos=pos0;
                    }
                    else
                    {
                        wxASSERT(pos0 <= tempmemdata->m_Buffers.size()*BUFFER_SIZE);
                        tempmemdata->m_Size=pos0;
                    }
                    WriteBlockToData(tempoutdata, file_bit);

                    file_bit->m_Data=tempindata;
                    file_bit->m_Pos=pos0;


                    tempsize+=file_bit->m_Size;
                    writable_size+=file_bit->m_Size;

#ifdef _DEBUG
                    if(tempsize>maxsize)
                    {
                        maxsize=tempsize;
                    }
#endif
                }

                ignore_overlap=false;

                // to next block
                if(++file_bit==file_bitend)
                {
                    if(++file_lit != m_LineList.end() && file_lit->m_Size!=0)
                    {
                        file_bit=file_lit->m_Blocks.begin();
                        file_bitend=file_lit->m_Blocks.end();
                    }
                    else // at end of file
                    {
                        writable_size+=m_Size-(writepos+writable_size);
                        wxASSERT(writable_size >= write_bit->m_Size);
                    }

                }

            }
            while(writable_size < write_bit->m_Size);
        }

        // write the block to file
        m_FileData->m_SavePos=writepos;
        WriteBlockToData(m_FileData, write_bit);

        if(write_bit->m_Data==m_FileData)
        {
            if(file_lit_not_set==false && write_bit->m_Pos >= filepos)
            {
                file_lit_not_set=true;
            }

            write_bit->m_Pos=writepos;
        }
        else if(write_bit->m_Data==tempindata) // this block is in the tempdata
        {
            write_bit->m_Data=m_FileData;
            write_bit->m_Pos=writepos;


            temppos+=write_bit->m_Size;
            tempsize-=write_bit->m_Size;

            wxASSERT(tempsize>=0);

            if(tempsize==0)
            {
                temppos=0;

                if(tempmemdata!=nullptr)
                {
                    tempmemdata->Reset();
                }
            }
        }

        writepos+=write_bit->m_Size;
        writable_size-=write_bit->m_Size;

        if(file_lit_not_set==false && writable_size==0 && tempsize==0)
        {
            file_lit_not_set=true;
        }

        // to next block
        if(++write_bit == write_bitend)
        {
            if(writepos==m_Size) // end of file
                break;

            wxASSERT(writepos<m_Size);

            ++write_lit;
            write_bit=write_lit->m_Blocks.begin();
            write_bitend=write_lit->m_Blocks.end();
        }

    }
    while(writepos<m_Size);

    wxASSERT(tempsize==0 && writable_size>=0);

//#ifdef _DEBUG
//    maxsize += 0;
//#endif

    if(m_FileData->m_Size > m_Size)
    {
        m_FileData->m_File.Close();
        TruncateFile(filename, m_Size);     // truncate the file
        m_FileData->OpenFile();             // reopen the file
    }

    m_FileData->m_Size=m_Size;
    m_FileData->m_Buf1Pos=-1;
    m_FileData->m_Buf2Pos=-1;

    if(tempmemdata!=nullptr)
    {
        delete tempmemdata;
    }
    else if(tempfiledata!=nullptr)
    {
        delete tempfiledata;
        wxRemoveFile(tempfilename);
    }

    return true;
}

MadLineList::MadLineList()
{
}

// Toggle bookmark from given position.
// If there is a bookmark on the given position, remove it. If there is not, add it.
void MadLineList::ToggleBookmark(MadLineIterator position)
{
    if ( m_BookmarkList.empty() )
    {
        m_BookmarkList.push_front( position );
        return;
    }

    MadBookmarkIterator bmkIter = find( m_BookmarkList.begin(), m_BookmarkList.end(), position );
    if ( bmkIter != m_BookmarkList.end() )
    {
        m_BookmarkList.erase( bmkIter );   // we remove this bookmark
        return;
    }

    // we keep the bookmarks sorted, we have to find out where to insert the new one

    bmkIter = m_BookmarkList.begin();
    MadLineIterator bookmark = *bmkIter;
    MadLineIterator iter;

    for ( iter = begin(); iter != end(); iter++ )
    {
        if ( iter == position )
            break;

        if ( iter == bookmark )
        {
            if ( ++bmkIter == m_BookmarkList.end() )
                break;
            bookmark = *bmkIter;
        }
    }

    wxASSERT( iter != end() );
    m_BookmarkList.insert( bmkIter, position );
}

void MadLineList::ClearAllBookmarks()
{
    m_BookmarkList.clear();
}

// Return line number, or -1 if there are no bookmars.
//
int MadLineList::GetNextBookmark( MadLineIterator position )
{
    if ( m_BookmarkList.empty() )
        return -1;

    int lineNumFirstBmk = -1;
    int lineNum = 1;
    bool positionFound = false;

    MadBookmarkIterator bmkIter = m_BookmarkList.begin();
    MadLineIterator bookmark = *bmkIter;

    MadLineIterator iter;
    for ( iter = begin(); iter != end(); ++lineNum, ++iter )
    {
        if ( iter == bookmark )
        {
            if ( positionFound )
                break;   // we found the next bookmark

            if ( lineNumFirstBmk < 0 )
                lineNumFirstBmk = lineNum;

            if ( ++bmkIter == m_BookmarkList.end() )
                return lineNumFirstBmk; // no more bookmarks, we return the position of the first one

            bookmark = *bmkIter;
        }
        if ( iter == position )
            positionFound = true;
    }

    wxASSERT( iter != end() );  // this can be triggered if bookmark list is not sorted
    return lineNum;
}


// Return opposite line number (from the end to the beginning, i.e. the last line as N= 1),
// or -1 if there are no bookmars.
//
int MadLineList::GetPreviousBookmark( MadLineIterator position )
{
    if ( m_BookmarkList.empty() )
        return -1;

    int lineNumFirstBmk = -1;
    int lineNum = 1;
    bool positionFound = false;

    list<MadLineIterator>::reverse_iterator bmkIter = m_BookmarkList.rbegin();
    MadLineIterator bookmark = *bmkIter;

    list<MadLine>::reverse_iterator iter;
    for ( iter = rbegin(); iter != rend(); ++lineNum, ++iter )
    {
        MadLineIterator frwiter = iter.base();
        --frwiter;

        if ( frwiter == bookmark )
        {
            if ( positionFound )
                break;   // we found the next bookmark

            if ( lineNumFirstBmk < 0 )
                lineNumFirstBmk = lineNum;

            if ( ++bmkIter == m_BookmarkList.rend() )
                return lineNumFirstBmk; // no more bookmarks, we return the position of the first one

            bookmark = *bmkIter;
        }
        if ( frwiter == position )
            positionFound = true;
    }

    wxASSERT( iter != rend() );  // this can be triggered if bookmark list is not sorted
    return lineNum;
}


MadLineIterator MadLineList::erase( MadLineIterator position )
{
    // using remove() is simpler, but we plan more sophisticated bookmars later
    MadBookmarkIterator found = find( m_BookmarkList.begin(), m_BookmarkList.end(), position );
    if ( found != m_BookmarkList.end() )
        m_BookmarkList.erase( found );

    return list<MadLine>::erase( position );
}


bool MadLineList::Bookmarked( MadLineIterator position )
{
    MadBookmarkIterator found = find( m_BookmarkList.begin(), m_BookmarkList.end(), position );
    return found != m_BookmarkList.end();
}

LineNumberList MadLineList::SaveBookmarkLineNumberList() const
{
    list<MadLineIterator>::const_iterator bmkIter = m_BookmarkList.begin();
    list<MadLineIterator>::const_iterator bmkEnd = m_BookmarkList.end();

    LineNumberList linenums;
    size_t linenum = 0;
    for(list<MadLine>::const_iterator iter = begin(); bmkIter!=bmkEnd && iter!=end(); ++iter)
    {
        ++linenum;

        if (*bmkIter != iter)
            continue;

        linenums.push_back(linenum);
        ++bmkIter;
    }

    return linenums;
}

void MadLineList::RestoreBookmarkByLineNumberList(const LineNumberList& linenums)
{
    if (linenums.empty())
        return;

    m_BookmarkList.clear();

    size_t max_bmklinenum = *(linenums.rbegin());
    size_t linenum = 0;
    size_t i = 0;
    for(MadLineIterator iter = begin(); linenum<max_bmklinenum && iter!=end(); ++iter)
    {
        ++linenum;
        if (linenum < linenums[i])
            continue;

        m_BookmarkList.push_back(iter);
        ++i;
    }
}
