/*-----------------------------------------------------------------------------

    This is a part of the Microsoft Source Code Samples.
    Copyright (C) 1995 Microsoft Corporation.
    All rights reserved.
    This source code is only intended as a supplement to
    Microsoft Development Tools and/or WinHelp documentation.
    See these sources for detailed information regarding the
    Microsoft samples programs.

    MODULE: Reader.c

    PURPOSE: Read from comm port

    FUNCTIONS:
        OutputABufferToWindow - process incoming data destined for tty window
        OutputABufferToFile   - process incoming data destined for a file
        OutputABuffer         - called when data is read from port

-----------------------------------------------------------------------------*/

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "MTTTY.h"

/*
    Prototypes for functions call only within this file
*/
void OutputABufferToFile( HANDLE, char *, DWORD );

void OutputACharToWindow(HWND hTTY, char c)
{
    RECT rect;

    /*
        update screen buffer with new character
        need to do a character by character check
        for special characters
    */
    switch (c)
    {
        case ASCII_BEL:                // BELL CHAR
            MessageBeep( 0 ) ;
        break ;

        case ASCII_BS:                 // Backspace CHAR
            if (COLUMN( TTYInfo ) > 0) COLUMN( TTYInfo ) -- ;
        break ;

        case ASCII_CR:                 // Carriage Return
            COLUMN( TTYInfo ) = 0 ;
            if (!NEWLINE( TTYInfo )) break;

            //
            // FALL THROUGH
            //

        case ASCII_LF:                 // Line Feed
            if (ROW( TTYInfo )++ == MAXROWS - 1)
            {
                MoveMemory( (LPSTR) (SCREEN( TTYInfo )), (LPSTR) (SCREEN( TTYInfo ) + MAXCOLS), (MAXROWS - 1) * MAXCOLS ) ;
                FillMemory((LPSTR) (SCREEN( TTYInfo ) + (MAXROWS - 1) * MAXCOLS), MAXCOLS,  ' ' ) ;
                InvalidateRect( hTTY, NULL, FALSE ) ;
                ROW( TTYInfo )-- ;
            }
        break ;

        default:                       // standard character
            SCREENCHAR(TTYInfo, COLUMN(TTYInfo), ROW(TTYInfo)) = c;

            rect.left = (COLUMN( TTYInfo ) * XCHAR( TTYInfo )) - XOFFSET( TTYInfo ) ;
            rect.right = rect.left + XCHAR( TTYInfo ) ;
            rect.top = (ROW( TTYInfo ) * YCHAR( TTYInfo )) - YOFFSET( TTYInfo ) ;
            rect.bottom = rect.top + YCHAR( TTYInfo ) ;
            InvalidateRect( hTTY, &rect, FALSE ) ;

            //
            // Line wrap
            //
            if (COLUMN( TTYInfo ) < MAXCOLS-1 ) COLUMN( TTYInfo )++ ;
            else if (AUTOWRAP( TTYInfo ))
            {
                OutputACharToWindow(hTTY, '\r') ;
                if (!NEWLINE( TTYInfo )) OutputACharToWindow(hTTY, '\n') ;
            }
        break;
    }

    MoveTTYCursor(hTTY);
}


/*-----------------------------------------------------------------------------

FUNCTION: OutputABufferToWindow(HWND, char *, DWORD)

PURPOSE: Updates TTY Buffer with characters just received.

PARAMETERS:
    hTTY     - handle to the TTY child window
    lpBuf    - address of data buffer
    dwBufLen - size of data buffer

HISTORY:   Date       Author      Comment
            5/ 8/91   BryanW      Wrote it
           10/27/95   AllenD      Modified for MTTTY Sample

-----------------------------------------------------------------------------*/
void OutputABufferToWindow(HWND hTTY, char * lpBuf, DWORD dwBufLen)
{
    /*
        update screen buffer with new buffer
        need to do a character by character check
        for special characters
    */
    int i;
    char hexbuff[8];

    for ( i = 0 ; i < (int) dwBufLen; i++)
    {
        if(DISPLAYHEX( TTYInfo ))
        {
            sprintf(hexbuff, "%02x", lpBuf[ i ]);

            OutputACharToWindow(hTTY, hexbuff[0]) ;
            OutputACharToWindow(hTTY, hexbuff[1]) ;
        }
        else if(NONPRINTHEX( TTYInfo ))
        {
            unsigned c = lpBuf[ i ];
            if(c < ' ' || c > '~')
            {
                sprintf(hexbuff, "<%02x>", lpBuf[ i ]);

                OutputACharToWindow(hTTY, hexbuff[0]) ;
                OutputACharToWindow(hTTY, hexbuff[1]) ;
                OutputACharToWindow(hTTY, hexbuff[2]) ;
                OutputACharToWindow(hTTY, hexbuff[3]) ;
            }
            else OutputACharToWindow(hTTY, c ) ;
        }
        else OutputACharToWindow(hTTY, lpBuf[ i ] ) ;
    }
}

/*-----------------------------------------------------------------------------

FUNCTION: OutputABufferToFile(HANDLE, char *, DWORD)

PURPOSE: Output a rec'd buffer to a file

PARAMETERS:
    hFile    - handle of file save data into
    lpBuf    - address of data buffer
    dwBufLen - size of data buffer

HISTORY:   Date:      Author:     Comment:
           10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void OutputABufferToFile(HANDLE hFile, char * lpBuf, DWORD dwBufLen)
{
    DWORD dwWritten;

    //
    // place buffer into file, report any errors
    //
    if (!WriteFile(hFile, lpBuf, dwBufLen, &dwWritten, NULL))
        ErrorReporter("WriteFile in file capture");

    if (dwBufLen != dwWritten)
        ErrorReporter("WriteFile");

    //
    // update transfer progress bar
    //
    PostMessage(GetDlgItem(ghWndStatusDlg, IDC_TRANSFERPROGRESS), PBM_STEPIT, 0, 0);

    return;
}

/*-----------------------------------------------------------------------------

FUNCTION: OutputABuffer(HWND, char *, DWORD)

PURPOSE: Send a rec'd buffer to the approprate location

PARAMETERS:
    hTTY     - handle to the TTY child window
    lpBuf    - address of data buffer
    dwBufLen - size of data buffer

COMMENTS: If buffer is 0 length, then do nothing.

HISTORY:   Date:      Author:     Comment:
           10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void OutputABuffer(HWND hTTY, char * lpBuf, DWORD dwBufLen)
{
    if (dwBufLen == 0) {
        OutputDebugString("NULL Buffer in OutputABuffer\n\r");
        return;
    }

    switch(gdwReceiveState)
    {
        case RECEIVE_TTY:
            OutputABufferToWindow(hTTY, lpBuf, dwBufLen);
            break;

        case RECEIVE_CAPTURED:
            OutputABufferToFile(ghFileCapture, lpBuf, dwBufLen);
            break;

        default:
            OutputDebugString("Unknown receive state!\n\r");
    }

    return;
}
