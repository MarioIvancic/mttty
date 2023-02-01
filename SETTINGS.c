/*-----------------------------------------------------------------------------
    This is a part of the Microsoft Source Code Samples.
    Copyright (C) 1995 Microsoft Corporation.
    All rights reserved.
    This source code is only intended as a supplement to
    Microsoft Development Tools and/or WinHelp documentation.
    See these sources for detailed information regarding the
    Microsoft samples programs.

    MODULE: Settings.c

    PURPOSE: Controls all dialog controls in the Settings Dialog as well
	     as the comm events dialog, flow control dialog, and timeouts
	     dialog.  The module also controls the tty settings based on
	     these dialogs and also control comm port settings using
	     SetCommState and SetCommTimeouts.

    FUNCTIONS:
	OpenSettingsToolbar - Creates the Settings Dialog
	ChangeConnection    - Modifies menus & controls based on connection state
	UpdateTTYInfo       - Modifies TTY data from Settings Dialog and
			      if connected, updates open comm port settings
	FillComboBox        - Fills a combo box with strings
	SetComboBox         - Selects an entry from a combo box
	SettingDlgInit      - Initializes settings dialog
	GetdwTTYItem        - returns a DWORD value from a dialog control
	GetbTTYItem         - returns a BYTE value from a dialog control
	ToolbarProc         - Dialog procedure for Settings Dialog
	InitHexControl      - Places a byte value into edit controls of a dialog
	GetHexControl       - returns hex data from a control and converts to a char
	InitCommEventsDlg   - Initializes Comm Events Dialog
	SaveCommEventsDlg   - Saves comm events flag if changed
	CommEventsProc      - Dialog procedure for Comm Events Dialog
	SaveFlowControlDlg  - Saves flow control settings if changed
	InitFlowControlDlg  - Inititlizes Flow Control Dialog
	FlowDefault         - sets "hardware" or "software" flow control
	FlowControlProc     - Dialog procedure for Flow Control Dialog
	InitTimeoutsDlg     - Initializes Timeouts Dialog
	SaveTimeoutsDlg     - Saves comm timeouts from Timeouts Dialog
	TimeoutsProc        - Dialog procedure for Timeouts Dialog

-----------------------------------------------------------------------------*/

#include <windows.h>
#include <stdio.h>
#include "mttty.h"

#define MAXLEN_TEMPSTR  20

/*
    Prototypes for functions called only within this file
*/
void FillComboBox( HWND, const char **, DWORD *, WORD, DWORD );
BOOL SettingsDlgInit( HWND );
DWORD GetdwTTYItem( HWND, int, const char **, DWORD *, int );
BYTE GetbTTYItem( HWND, int, const char **, DWORD *, int);
BOOL CALLBACK CommEventsProc( HWND, UINT, WPARAM, LPARAM );
BOOL CALLBACK ToolbarProc( HWND, UINT, WPARAM, LPARAM );
void InitHexControl(HWND, WORD, WORD, char);
char GetHexControl(HWND, WORD, WORD);
void SaveCommEventsDlg( HWND );
void InitCommEventsDlg( HWND, DWORD );
BOOL CALLBACK FlowControlProc( HWND, UINT, WPARAM, LPARAM );
void InitFlowControlDlg( HWND );
void SaveFlowControlDlg( HWND );
void FlowDefault(HWND hdlg, WORD wId);
BOOL CALLBACK TimeoutsProc( HWND, UINT, WPARAM, LPARAM );
void InitTimeoutsDlg( HWND, COMMTIMEOUTS );
void SaveTimeoutsDlg( HWND );
BOOL CALLBACK GetADWORDProc( HWND, UINT, WPARAM, LPARAM );
// Mario Ivanèiæ, 2018
BOOL CALLBACK SetMacrosProc(HWND hdlg, UINT uMessage, WPARAM wparam, LPARAM lparam);
// convert COMnn to \\.\COMnn
void adjust_com_port_name(char* name);

/*
    GLOBALS for this file
    The string arrays are the items in the dialog list controls.
*/

DCB dcbTemp;

const char * szBaud[] = {
	    "110", "300", "600", "1200", "2400",
	    "4800", "9600", "14400", "19200",
	    "38400", "56000", "57600", "115200",
	    "128000", "256000"
	};

DWORD   BaudTable[] =  {
	    CBR_110, CBR_300, CBR_600, CBR_1200, CBR_2400,
	    CBR_4800, CBR_9600, CBR_14400, CBR_19200, CBR_38400,
	    CBR_56000, CBR_57600, CBR_115200, CBR_128000, CBR_256000
	} ;

const char * szParity[] =   {   "None", "Even", "Odd", "Mark", "Space" };

DWORD   ParityTable[] = {  NOPARITY, EVENPARITY, ODDPARITY, MARKPARITY, SPACEPARITY  } ;

const char * szStopBits[] =  {  "1", "1.5", "2"  };

DWORD   StopBitsTable[] =  { ONESTOPBIT, ONE5STOPBITS, TWOSTOPBITS } ;

const char * szDTRControlStrings[] = { "Enable", "Disable", "Handshake" };

DWORD   DTRControlTable[] = { DTR_CONTROL_ENABLE, DTR_CONTROL_DISABLE, DTR_CONTROL_HANDSHAKE };

const char * szRTSControlStrings[] = { "Enable", "Disable", "Handshake", "Toggle" };

DWORD   RTSControlTable[] = {   RTS_CONTROL_ENABLE, RTS_CONTROL_DISABLE,
				RTS_CONTROL_HANDSHAKE, RTS_CONTROL_TOGGLE };

DWORD   EventFlagsTable[] = {
	    EV_BREAK, EV_CTS, EV_DSR, EV_ERR, EV_RING,
	    EV_RLSD, EV_RXCHAR, EV_RXFLAG, EV_TXEMPTY
	};

// Mario Ivanèiæ, 2018
#define MACRO_BUFF_SIZE 1024
typedef struct
{
    unsigned char buff[MACRO_BUFF_SIZE];     // za slanje
    char text[MACRO_BUFF_SIZE];     // za prikaz
    int len;    // buffer text len
    int tlen;   // text len
    int hex;
} macro_buffer_t;

macro_buffer_t macro_buffer[10];


int get_hex_value(int c)
{
    if('0' <= c && c <= '9') return c - '0';
    if('a' <= c && c <= 'f') return c - 'a' + 10;
    if('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

void set_macro_buffer(unsigned i, HWND hdlg)
{
    if(i >= 10) return;
    int hex = (IsDlgButtonChecked(hdlg, IDC_MACRO1HEXRB + 2 * i) == BST_CHECKED) ? 1 : 0;
    char *text = macro_buffer[i].text;
    unsigned char *buff = (unsigned char*)macro_buffer[i].buff;
    macro_buffer[i].tlen = 0;
    int tlen = GetDlgItemText(hdlg, IDC_MACRO1 + i, text, sizeof(macro_buffer[i].text) - 1);
    if(!tlen && GetLastError()) return; // error
    text[tlen] = 0;
    macro_buffer[i].tlen = tlen;

    if(hex)
    {
        macro_buffer[i].hex = 1;
        int j, c, hi = 1, len = 0;
        unsigned h = 0;
        for(j = 0; j < tlen; j++)
        {
            c = get_hex_value(text[j]);
            if(c == -1) continue;
            if(hi)
            {
                h = (unsigned)c;
                h <<= 4;
                hi = 0;
            }
            else
            {
                h += c;
                buff[len++] = h;
                hi = 1;
            }
        }
        if(!hi) buff[len++] = h;
        macro_buffer[i].len = len;
    }
    else
    {
        macro_buffer[i].hex = 0;
        int j, esc = 0, len = 0;
        for(j = 0; j < tlen; j++)
        {
            int c = text[j];
            if(esc)
            {
                esc = 0;
                switch(c)
                {
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case '0': c = '\0'; break;
                    case 'x':
                        j++;
                        c = 0;
                        if(j == tlen) break;
                        c = get_hex_value(text[j]);
                        if(c == -1) c = 0;
                        j++;
                        if(j == tlen) break;
                        if(get_hex_value(text[j]) == -1) c *= 16;
                        else c = 16 * c + get_hex_value(text[j]);
                    break;
                }
                macro_buffer[i].buff[len++] = c;
            }
            else if(c == '\\') esc = 1;
            else macro_buffer[i].buff[len++] = c;
        }
        macro_buffer[i].len = len;
    }
}


void init_macro_text(HWND hdlg, unsigned i)
{
    if(i >= 10) return;
    char *text = macro_buffer[i].text;
    int tlen = macro_buffer[i].tlen;
    int hex = macro_buffer[i].hex;

    text[tlen] = 0;

    SetDlgItemText(hdlg, IDC_MACRO1 + i, text);
    if(hex) CheckRadioButton(hdlg, IDC_MACRO1ASCIIRB + 2 * i, IDC_MACRO1ASCIIRB + 2 * i + 1, IDC_MACRO1ASCIIRB + 2 * i + 1);
    else    CheckRadioButton(hdlg, IDC_MACRO1ASCIIRB + 2 * i, IDC_MACRO1ASCIIRB + 2 * i + 1, IDC_MACRO1ASCIIRB + 2 * i);
}



void save_macros_to_file(const char* filename)
{
    FILE * fp = fopen(filename, "wb");
    if(!fp) return;
    int i;
    for(i = 0; i < 10; i++)
    {
        if(macro_buffer[i].hex) fwrite("1 ", 2, 1, fp);
        else fwrite("0 ", 2, 1, fp);
        fwrite(macro_buffer[i].text, macro_buffer[i].tlen, 1, fp);
        fwrite("\n", 1, 1, fp);
    }
    fclose(fp);
}


void load_macros_from_file(const char* filename)
{
    FILE * fp = fopen(filename, "rb");
    if(!fp) return;
    int i;
    char text[MACRO_BUFF_SIZE + 4];
    for(i = 0; i < 10; i++)
    {
        text[0] = 0;
        fgets(text, MACRO_BUFF_SIZE + 4, fp);
        if(text[0] == '0') macro_buffer[i].hex = 0;
        else if(text[0] == '1') macro_buffer[i].hex = 1;
        else continue;

        if(text[1] != ' ') continue;
        int tlen = strlen(text + 2);
        if(text[tlen + 2 - 1] == '\n')
        {
            text[tlen + 2 - 1] = 0;
            tlen--;
        }
        macro_buffer[i].tlen = tlen;
        strcpy(macro_buffer[i].text, text + 2);
    }
    fclose(fp);
}


/*-----------------------------------------------------------------------------

FUNCTION: OpenSettingsToolbar(HWND)

PURPOSE: Open Settings Dialog

PARAMETERS:
    hWnd - dialog owner window handle

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void OpenSettingsToolbar(HWND hWnd)
{
    ghWndToolbarDlg = CreateDialog(ghInst, MAKEINTRESOURCE(IDD_TOOLBARSETTINGS), hWnd, ToolbarProc);

    if (ghWndToolbarDlg == NULL)
	ErrorReporter("CreateDialog (Toolbar Dialog)");

    return;
}

/*-----------------------------------------------------------------------------

FUNCTION: ChangeConnection(HWND, BOOL)

PURPOSE: Modifies connection appearance

PARAMETERS:
    hwnd       - menu owner windows
    fConnected - TRUE sets connection appearance to connected
		 FALSE sets appearance to not connected

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void ChangeConnection( HWND hwnd, BOOL fConnected )
{
    HMENU hMenu;
    int i;

    if (fConnected)
    {
        /*
            The port is connected.  Need to :
            Disable connect menu and enable disconnect menu.
            Enable file transfer menu
            Disable comm port selection box
            Disable no writing, no reading, no events, no status check boxes
            Enable status check boxes
            Set focus to the child tty window
        */
        hMenu = GetMenu( hwnd ) ;
        EnableMenuItem( hMenu, ID_FILE_CONNECT,
                   MF_GRAYED | MF_DISABLED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_FILE_DISCONNECT,
                   MF_ENABLED | MF_BYCOMMAND ) ;

        EnableMenuItem( hMenu, ID_TRANSFER_SENDFILETEXT,
                   MF_ENABLED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_TRANSFER_RECEIVEFILETEXT,
                   MF_ENABLED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_TRANSFER_SENDREPEATEDLY,
                   MF_ENABLED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_TRANSFER_ABORTSENDING,
                   MF_DISABLED | MF_GRAYED | MF_BYCOMMAND );
        EnableMenuItem( hMenu, ID_TRANSFER_ABORTREPEATEDSENDING,
                   MF_DISABLED | MF_GRAYED | MF_BYCOMMAND );

        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_PORTCOMBO), FALSE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_NOWRITINGCHK), FALSE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_NOREADINGCHK), FALSE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_NOEVENTSCHK),  FALSE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_NOSTATUSCHK),  FALSE);

        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_OPENBTN), FALSE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_CLOSEBTN), TRUE);

        for (i = IDC_STATCTS; i <= IDC_STATRLSD; i++)
            EnableWindow( GetDlgItem(ghWndStatusDlg, i), TRUE );

        for (i = IDC_CTSHOLDCHK; i <= IDC_RXCHAREDIT; i++)
            EnableWindow( GetDlgItem(ghWndStatusDlg, i), TRUE);

        SetFocus(ghWndTTY);
    }
    else
    {
        //
        // Not connected, do opposite of above.
        //
        hMenu = GetMenu( hwnd ) ;
        EnableMenuItem( hMenu, ID_FILE_CONNECT,
                   MF_ENABLED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_FILE_DISCONNECT,
                   MF_GRAYED | MF_DISABLED | MF_BYCOMMAND ) ;

        EnableMenuItem( hMenu, ID_TRANSFER_SENDFILETEXT,
                   MF_DISABLED | MF_GRAYED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_TRANSFER_RECEIVEFILETEXT,
                   MF_DISABLED | MF_GRAYED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_TRANSFER_SENDREPEATEDLY,
                   MF_DISABLED | MF_GRAYED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_TRANSFER_ABORTSENDING,
                   MF_DISABLED | MF_GRAYED | MF_BYCOMMAND ) ;
        EnableMenuItem( hMenu, ID_TRANSFER_ABORTREPEATEDSENDING,
                   MF_DISABLED | MF_GRAYED | MF_BYCOMMAND );

        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_PORTCOMBO), TRUE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_NOWRITINGCHK), TRUE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_NOREADINGCHK), TRUE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_NOEVENTSCHK),  TRUE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_NOSTATUSCHK),  TRUE);

        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_OPENBTN), TRUE);
        EnableWindow( GetDlgItem(ghWndToolbarDlg, IDC_CLOSEBTN), FALSE);

        for (i = IDC_STATCTS; i <= IDC_STATRLSD; i++) {
            CheckDlgButton(ghWndStatusDlg, i, 0);
            EnableWindow( GetDlgItem(ghWndStatusDlg, i), FALSE );
        }

        for (i = IDC_CTSHOLDCHK; i <= IDC_RXCHAREDIT; i++) {
            if (i != IDC_TXCHAREDIT && i != IDC_RXCHAREDIT)
            CheckDlgButton(ghWndStatusDlg, i, 0);
            else
            SetDlgItemInt(ghWndStatusDlg, i, 0, FALSE);

            EnableWindow( GetDlgItem(ghWndStatusDlg, i), FALSE);
        }

        SetFocus(ghwndMain);
    }

    return;
}


/*-----------------------------------------------------------------------------

FUNCTION: UpdateTTYInfo(void)

PURPOSE: Modifies TTY data based on the settings and calls UpdateConnection

COMMENTS: Modifies the data based on the dialog. If connected,
	  calls UpdateConnection.

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it
	    1/ 9/96   AllenD      Split DCB settings to new function

-----------------------------------------------------------------------------*/
void UpdateTTYInfo()
{
    //
    // update globals from dialog settings
    //
    GetDlgItemText(ghWndToolbarDlg, IDC_PORTCOMBO, gszPort, sizeof(gszPort));
    adjust_com_port_name(gszPort);

    BAUDRATE(TTYInfo) = GetdwTTYItem( ghWndToolbarDlg,
					IDC_BAUDCOMBO,
					szBaud,
					BaudTable,
					sizeof(BaudTable)/sizeof(BaudTable[0]));

    PARITY(TTYInfo) = GetbTTYItem( ghWndToolbarDlg,
				     IDC_PARITYCOMBO,
				     szParity,
				     ParityTable,
				     sizeof(ParityTable)/sizeof(ParityTable[0]));

    STOPBITS(TTYInfo) = GetbTTYItem( ghWndToolbarDlg,
				       IDC_STOPBITSCOMBO,
				       szStopBits,
				       StopBitsTable,
				       sizeof(StopBitsTable)/sizeof(StopBitsTable[0]));

    LOCALECHO(TTYInfo) = IsDlgButtonChecked(ghWndToolbarDlg, IDC_LOCALECHOCHK);
    BYTESIZE(TTYInfo) = GetDlgItemInt(ghWndToolbarDlg, IDC_DATABITSCOMBO, NULL, FALSE);
    NEWLINE(TTYInfo) = IsDlgButtonChecked(ghWndToolbarDlg, IDC_LFBTN);
    AUTOWRAP(TTYInfo) = IsDlgButtonChecked(ghWndToolbarDlg, IDC_AUTOWRAPCHK);
    DISPLAYERRORS(TTYInfo) = IsDlgButtonChecked(ghWndToolbarDlg, IDC_DISPLAYERRORSCHK);

    NOREADING(TTYInfo) = IsDlgButtonChecked(ghWndToolbarDlg, IDC_NOREADINGCHK);
    NOWRITING(TTYInfo) = IsDlgButtonChecked(ghWndToolbarDlg, IDC_NOWRITINGCHK);
    NOEVENTS(TTYInfo)  = IsDlgButtonChecked(ghWndToolbarDlg, IDC_NOEVENTSCHK);
    NOSTATUS(TTYInfo)  = IsDlgButtonChecked(ghWndToolbarDlg, IDC_NOSTATUSCHK);

    NONPRINTHEX(TTYInfo)  = IsDlgButtonChecked(ghWndToolbarDlg, IDC_NONPRINTHEXCHK);
    DISPLAYHEX(TTYInfo)  = IsDlgButtonChecked(ghWndToolbarDlg, IDC_ALLASHEXCHK);

    if (CONNECTED(TTYInfo))      // if connected, then update port state
	UpdateConnection();

    return;
}

/*-----------------------------------------------------------------------------

FUNCTION: UpdateConnection( void )

PURPOSE: Sets port state based on settings from the user

COMMENTS: Sets up DCB structure and calls SetCommState.
	  Sets up new timeouts by calling SetCommTimeouts.

HISTORY:   Date:      Author:     Comment:
	    1/ 9/96   AllenD      Wrote it

-----------------------------------------------------------------------------*/
BOOL UpdateConnection()
{
    DCB dcb;
    memset(&dcb, 0, sizeof(DCB));

    dcb.DCBlength = sizeof(dcb);

    //
    // get current DCB settings
    //
    if (!GetCommState(COMDEV(TTYInfo), &dcb))
	ErrorReporter("GetCommState");

    //
    // update DCB rate, byte size, parity, and stop bits size
    //
    dcb.BaudRate = BAUDRATE(TTYInfo);
    dcb.ByteSize = BYTESIZE(TTYInfo);
    dcb.Parity   = PARITY(TTYInfo);
    dcb.StopBits = STOPBITS(TTYInfo);

    //
    // update event flags
    //
    if (EVENTFLAGS(TTYInfo) & EV_RXFLAG)
	dcb.EvtChar = FLAGCHAR(TTYInfo);
    else
	dcb.EvtChar = '\0';

    //
    // update flow control settings
    //
    dcb.fDtrControl     = DTRCONTROL(TTYInfo);
    dcb.fRtsControl     = RTSCONTROL(TTYInfo);

    dcb.fOutxCtsFlow    = CTSOUTFLOW(TTYInfo);
    dcb.fOutxDsrFlow    = DSROUTFLOW(TTYInfo);
    dcb.fDsrSensitivity = DSRINFLOW(TTYInfo);
    dcb.fOutX           = XONXOFFOUTFLOW(TTYInfo);
    dcb.fInX            = XONXOFFINFLOW(TTYInfo);
    dcb.fTXContinueOnXoff = TXAFTERXOFFSENT(TTYInfo);
    dcb.XonChar         = XONCHAR(TTYInfo);
    dcb.XoffChar        = XOFFCHAR(TTYInfo);
    dcb.XonLim          = XONLIMIT(TTYInfo);
    dcb.XoffLim         = XOFFLIMIT(TTYInfo);

    //
    // DCB settings not in the user's control
    //
    dcb.fParity = TRUE;

    //
    // set new state
    //
    if (!SetCommState(COMDEV(TTYInfo), &dcb))
	ErrorReporter("SetCommState");

    //
    // set new timeouts
    //
    if (!SetCommTimeouts(COMDEV(TTYInfo), &(TIMEOUTSNEW(TTYInfo))))
	ErrorReporter("SetCommTimeouts");

    return TRUE;
}


/*-----------------------------------------------------------------------------

FUNCTION: FillComboBox(HWND, char **, DWORD *, WORD, DWORD)

PURPOSE: Populates dialog controls with proper strings

PARAMETERS:
    hCtrlWnd         - window handle of control being filled
    szString         - string table contains strings to fill control with
    npTable          - table of values corresponding to strings
    wTableLen        - length of the string table
    dwCurrentSetting - initialz combo box selection

COMMENTS: This function originally found in the Win32 COMM sample
	  Written by BryanW.  Modified for Win32 MTTTY Sample.

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Modified for MTTTY

-----------------------------------------------------------------------------*/
void FillComboBox( HWND hCtrlWnd, const char ** szString,
			DWORD * npTable, WORD wTableLen, DWORD dwCurrentSetting )
{
    WORD wCount, wPosition ;

    for (wCount = 0; wCount < wTableLen; wCount++) {
	wPosition = LOWORD( SendMessage( hCtrlWnd, CB_ADDSTRING, 0,
					(LPARAM) (LPSTR) szString[wCount] ) ) ;

	//
	// use item data to store the actual table value
	//
	SendMessage( hCtrlWnd, CB_SETITEMDATA, (WPARAM) wPosition,
		     (LPARAM) *(npTable + wCount) ) ;

	//
	// if this is our current setting, select it
	//
	if (*(npTable + wCount) == dwCurrentSetting)
	    SendMessage( hCtrlWnd, CB_SETCURSEL, (WPARAM) wPosition, 0L ) ;
    }
    return ;
}

/*-----------------------------------------------------------------------------

FUNCTION: SetComboBox(HWND, WORD, DWORD)

PURPOSE: Selects an entry from a dialog combobox

PARAMETERS:
    hCtrlWnd     - windows handle of control
    wTableLen    - length of value table for this control
    dwNewSetting - new item to base selection on

HISTORY:   Date:      Author:     Comment:
	   11/20/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void SetComboBox( HWND hCtrlWnd, WORD wTableLen, DWORD dwNewSetting )
{
    WORD wCount, wItemData ;

    for (wCount = 0; wCount < wTableLen; wCount++) {
	wItemData = LOWORD( SendMessage( hCtrlWnd, CB_GETITEMDATA, (WPARAM) wCount, 0L ) );

	if (wItemData == dwNewSetting) {
	    SendMessage( hCtrlWnd, CB_SETCURSEL, (WPARAM) wCount, 0L ) ;
	break;
	}
    }
    return ;
}



int is_com_port(int i)
{
    char name[16];
    HANDLE hCom = NULL;

    if (i < 10) sprintf(name, "COM%d", i);
    else        sprintf(name, "\\\\.\\COM%d", i);

    hCom = CreateFile(name,
            GENERIC_READ|GENERIC_WRITE, // desired access should be read&write
            0,                          // COM port must be opened in non-sharing mode
            NULL,                       // don't care about the security
            OPEN_EXISTING,              // IMPORTANT: must use OPEN_EXISTING for a COM port
            0,                          // usually overlapped but non-overlapped for existance test
            NULL);                      // always NULL for a general purpose COM port

    if (INVALID_HANDLE_VALUE == hCom)
    {
        if (ERROR_ACCESS_DENIED == GetLastError())
        {   // then it exists and currently opened
            return 1;
        }
    }
    else
    {   // COM port exist
        CloseHandle(hCom);
        return 1;
    }
    return 0;
}


// convert COMnn to \\.\COMnn
void adjust_com_port_name(char* name)
{
    if(!name) return;
    if(name[0] != 'C' || name[1] != 'O' || name[2] != 'M') return;
    if(name[3] < '1' || name[3] > '9') return;
    if(name[4] == 0) return;    // COM1 .. COM9
    if(name[4] < '1' || name[4] > '9') return;
    int i = 10 * (name[3] - '0') + name[4] - '0';
    sprintf(name, "\\\\.\\COM%d", i);
}



/*-----------------------------------------------------------------------------

FUNCTION: SettingsDlgInit(HWND)

PURPOSE: Initializes Settings Dialog

PARAMETERS:
    hDlg - Dialog window handle

RETURN: always TRUE

COMMENTS: This function originally found in the Win32 COMM sample
	  Written by BryanW.  Modified for Win32 MTTTY Sample.

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Modified for MTTTY

-----------------------------------------------------------------------------*/
BOOL SettingsDlgInit( HWND hDlg )
{
    char szBuffer[ MAXLEN_TEMPSTR ], szTemp[ MAXLEN_TEMPSTR ] ;
    WORD wCount, wMaxCOM, wPosition ;

    wMaxCOM = MAXPORTS ;
    strcpy(szTemp, "COM");

    //
    // fill port combo box and make initial selection
    //
    for (wCount = 1; wCount <= wMaxCOM; wCount++)
    {
        if(!is_com_port(wCount)) continue;
        wsprintf( szBuffer, "%s%d", (LPSTR) szTemp, wCount) ;
        SendDlgItemMessage( hDlg, IDC_PORTCOMBO, CB_ADDSTRING, 0, (LPARAM) (LPSTR) szBuffer ) ;
    }

    SendDlgItemMessage( hDlg, IDC_PORTCOMBO, CB_SETCURSEL, (WPARAM) (PORT( TTYInfo ) - 1), 0L ) ;

    GetDlgItemText(hDlg, IDC_PORTCOMBO, gszPort, sizeof(gszPort));
    adjust_com_port_name(gszPort);

    //
    // fill baud combo box and make initial selection
    //
    FillComboBox( GetDlgItem( hDlg, IDC_BAUDCOMBO ),
		  szBaud, BaudTable,
		  sizeof( BaudTable ) / sizeof( BaudTable[ 0 ] ),
		  BAUDRATE( TTYInfo ) ) ;

    //
    // fill data bits combo box and make initial selection
    //
    for (wCount = 5; wCount < 9; wCount++) {
	wsprintf( szBuffer, "%d", wCount ) ;
	wPosition = LOWORD( SendDlgItemMessage( hDlg, IDC_DATABITSCOMBO,
						CB_ADDSTRING, 0,
						(LPARAM) (LPSTR) szBuffer ) ) ;

	//
	// if wCount is current selection, tell the combo box
	//
	if (wCount == BYTESIZE( TTYInfo ))
	    SendDlgItemMessage( hDlg, IDC_DATABITSCOMBO, CB_SETCURSEL,
				(WPARAM) wPosition, 0L ) ;
    }

    //
    // fill parity combo box and make initial selection
    //
    FillComboBox(   GetDlgItem( hDlg, IDC_PARITYCOMBO ),
		    szParity, ParityTable,
		    sizeof( ParityTable ) / sizeof( ParityTable[ 0 ] ),
		    PARITY( TTYInfo ) ) ;

    //
    // fill stop bits combo box and make initial selection
    //
    FillComboBox(   GetDlgItem( hDlg, IDC_STOPBITSCOMBO ),
		    szStopBits, StopBitsTable,
		    sizeof( StopBitsTable ) / sizeof ( StopBitsTable[ 0 ] ),
		    STOPBITS( TTYInfo ) ) ;
    //
    // set check marks based on TTY data
    //
    CheckDlgButton( hDlg, IDC_LOCALECHOCHK, LOCALECHO( TTYInfo ) ) ;
    CheckDlgButton( hDlg, IDC_DISPLAYERRORSCHK, DISPLAYERRORS( TTYInfo ) );
    CheckDlgButton( hDlg, IDC_LFBTN, NEWLINE( TTYInfo ) );
    CheckDlgButton( hDlg, IDC_AUTOWRAPCHK, AUTOWRAP( TTYInfo ) );

    CheckDlgButton( hDlg, IDC_NOWRITINGCHK, NOWRITING( TTYInfo ) );
    CheckDlgButton( hDlg, IDC_NOREADINGCHK, NOREADING( TTYInfo ) );
    CheckDlgButton( hDlg, IDC_NOSTATUSCHK,  NOSTATUS( TTYInfo ) );
    CheckDlgButton( hDlg, IDC_NOEVENTSCHK,  NOEVENTS( TTYInfo ) );

    CheckDlgButton( hDlg, IDC_NONPRINTHEXCHK,  NONPRINTHEX( TTYInfo ) );
    CheckDlgButton( hDlg, IDC_ALLASHEXCHK,  DISPLAYHEX( TTYInfo ) );

    EnableWindow( GetDlgItem(hDlg, IDC_OPENBTN), TRUE);
    EnableWindow( GetDlgItem(hDlg, IDC_CLOSEBTN), FALSE);

    return ( TRUE ) ;

} // end of SettingsDlgInit()


/*-----------------------------------------------------------------------------

FUNCTION: GetdwTTYItem(HWND, int, char **, DWORD *, int)

PURPOSE: Returns a DWORD item from a dialog control

PARAMETERS:
    hDlg      - Dialog window handle
    idControl - id of control to get data from
    szString  - table of strings that the control displays
    pTable    - table of data associated with strings
    iNumItems - size of table

RETURN:
    DWORD item corresponding to control selection
    0 if item not found correctly

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
DWORD GetdwTTYItem(HWND hDlg, int idControl, const char ** szString, DWORD * pTable, int iNumItems)
{
    int i;
    char szItem[MAXLEN_TEMPSTR];

    //
    // Get current selection (a string)
    //
    GetDlgItemText(hDlg, idControl, szItem, sizeof(szItem));

    /*
	Compare current selection with table to find index of item.
	If index is found, then return the DWORD item from table.
    */
    for (i = 0; i < iNumItems; i++)
    {
        if (strcmp(szString[i], szItem) == 0) return pTable[i];
    }

    return 0;
}

/*-----------------------------------------------------------------------------

FUNCTION: GetbTTYItem(HWND, int, char **, DWORD *, int)

PURPOSE: Returns a BYTE item from a dialog control

PARAMETERS:
    hDlg      - Dialog window handle
    idControl - id of control to get data from
    szString  - table of strings that the control displays
    pTable    - table of data associated with strings
    iNumItems - size of table

RETURN:
    BYTE item from corresponding to control selection
    0 if item data not found

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
BYTE GetbTTYItem(HWND hDlg, int idControl, const char ** szString, DWORD * pTable, int iNumItems)
{
    int i;
    char szItem[MAXLEN_TEMPSTR];

    //
    // Get current selection (a string)
    //
    GetDlgItemText(hDlg, idControl, szItem, sizeof(szItem));

    /*
	Compare current selection with table to find index of item.
	If index is found, then return the BYTE item from table.
    */
    for (i = 0; i < iNumItems; i++)
    {
        if (strcmp(szString[i], szItem) == 0) return (BYTE) pTable[i];
    }

    return 0;
}


/*-----------------------------------------------------------------------------

FUNCTION: ToolbarProc(HWND, UINT, WPARAM, LPARAM)

PURPOSE: Dialog Procedure for Settings Dialog

PARAMETERS:
    hWndDlg - Dialog window handle
    uMsg    - Window message
    wParam  - message parameter (depends on message)
    lParam  - message parameter (depends on message)

RETURN:
    TRUE if message is handled
    FALSE if message is not handled
    Exception is WM_INITDIALOG: returns FALSE since focus is not set

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
BOOL CALLBACK ToolbarProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    BOOL fRet = FALSE;

    switch(uMsg)
    {
	case WM_INITDIALOG:     // setup dialog with defaults
	    SettingsDlgInit(hWndDlg);
	    break;

	case WM_COMMAND:
	    {
		switch(LOWORD(wParam))
		{
		    case IDC_FONTBTN:       // font button pressed
			{
			    CHOOSEFONT cf;
			    memset(&cf, 0, sizeof(CHOOSEFONT));
			    LOGFONT lf;

			    lf = LFTTYFONT(TTYInfo);
			    cf.lStructSize = sizeof(CHOOSEFONT);
			    cf.hwndOwner = hWndDlg;
			    cf.lpLogFont = &lf;
			    cf.rgbColors = FGCOLOR(TTYInfo);
			    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_FIXEDPITCHONLY | \
				       CF_EFFECTS;

			    if (!ChooseFont(&cf))
				break;

			    InitNewFont(lf, cf.rgbColors);

			    //
			    // fix scroll bar sizes since we may have more or less pixels per
			    // character now
			    //
			    SizeTTY(ghWndTTY, (WORD)XSIZE(TTYInfo), (WORD)YSIZE(TTYInfo));

			    //
			    // repaint screen contents
			    //
			    InvalidateRect(ghWndTTY, NULL, TRUE);

			    //
			    // kill old cursor
			    //
			    KillTTYFocus(ghWndTTY);

			    //
			    // create new cursor
			    //
			    SetTTYFocus(ghWndTTY);
			}
			fRet = FALSE;
			break;

		    case IDC_COMMEVENTSBTN:     // comm events button pressed
			DialogBox(ghInst, MAKEINTRESOURCE(IDD_COMMEVENTSDLG), ghwndMain, CommEventsProc);
			fRet = FALSE;
			break;

		    case IDC_FLOWCONTROLBTN:
			DialogBox(ghInst, MAKEINTRESOURCE(IDD_FLOWCONTROLDLG), ghwndMain, FlowControlProc);
			fRet = FALSE;
			break;

		    case IDC_TIMEOUTSBTN:
			DialogBox(ghInst, MAKEINTRESOURCE(IDD_TIMEOUTSDLG), ghwndMain, TimeoutsProc);
			fRet = FALSE;
			break;

			// Mario Ivanèiæ, 2018
			case IDC_SETMACROSBTN:
			DialogBox(ghInst, MAKEINTRESOURCE(IDD_SETMACROS), ghwndMain, SetMacrosProc);
			fRet = FALSE;
			break;

			case IDC_MACRO1BTN:
			case IDC_MACRO2BTN:
			case IDC_MACRO3BTN:
			case IDC_MACRO4BTN:
			case IDC_MACRO5BTN:
			case IDC_MACRO6BTN:
			case IDC_MACRO7BTN:
			case IDC_MACRO8BTN:
			case IDC_MACRO9BTN:
			case IDC_MACRO10BTN:
			if (CONNECTED(TTYInfo))
            {
                int i = LOWORD(wParam) - IDC_MACRO1BTN;
                unsigned char* p = macro_buffer[i].buff;
                int len = macro_buffer[i].len;
                if(len)
                {
                    WriterAddNewNode(WRITE_BLOCK, len, 0, p, NULL, NULL);
                    if (LOCALECHO(TTYInfo)) OutputABufferToWindow(ghWndTTY, p, len);
                }
            }
			fRet = FALSE;
			break;

			case IDC_OPENBTN:
                if (SetupCommPort() != NULL) ChangeConnection(ghwndMain, CONNECTED(TTYInfo));
                fRet = FALSE;
			break;

			case IDC_CLOSEBTN:
                if (BreakDownCommPort()) ChangeConnection(ghwndMain, CONNECTED(TTYInfo));
                fRet = FALSE;
			break;

			case IDC_CLEARBTN:
                ClearTTYContents();
                InvalidateRect(ghWndTTY, NULL, TRUE);
                fRet = FALSE;
			break;

		    default:                    // some other control has been modified
			if (CONNECTED(TTYInfo))
			    UpdateTTYInfo();
			break;
		}
	    }
	    break;

	default:
	    break;
    }

    return fRet;
}



/*-----------------------------------------------------------------------------

FUNCTION: SetMacrosProc(HWND, UINT, WPARAM, LPARAM)

PURPOSE: Dialog Procedure for setting macros

PARAMETERS:
    hdlg     - Dialog window handle
    uMessage - window message
    wparam   - message parameter (depends on message)
    lparam   - message parameter (depends on message)

RETURN:
    TRUE if message is handled
    FALSE if message is not handled
    Exception is WM_INITDIALOG: returns FALSE since focus is not set

HISTORY:   Date:      Author:           Comment:
	   2018-03-16   Mario Ivanèiæ       Wrote it

-----------------------------------------------------------------------------*/
BOOL CALLBACK SetMacrosProc(HWND hdlg, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
    char* appdata = getenv("APPDATA");
    char config_file[MAX_PATH];
    config_file[0] = 0;
    if(appdata) strcpy(config_file, appdata);
    strcat(config_file, "\\MTTTY.cfg");

    switch(uMessage)
    {
        case WM_INITDIALOG:     // init controls
            load_macros_from_file(config_file);
            init_macro_text(hdlg, 0);
            init_macro_text(hdlg, 1);
            init_macro_text(hdlg, 2);
            init_macro_text(hdlg, 3);
            init_macro_text(hdlg, 4);
            init_macro_text(hdlg, 5);
            init_macro_text(hdlg, 6);
            init_macro_text(hdlg, 7);
            init_macro_text(hdlg, 8);
            init_macro_text(hdlg, 9);
            set_macro_buffer(0, hdlg);
            set_macro_buffer(1, hdlg);
            set_macro_buffer(2, hdlg);
            set_macro_buffer(3, hdlg);
            set_macro_buffer(4, hdlg);
            set_macro_buffer(5, hdlg);
            set_macro_buffer(6, hdlg);
            set_macro_buffer(7, hdlg);
            set_macro_buffer(8, hdlg);
            set_macro_buffer(9, hdlg);
        break;

        case WM_COMMAND:
            switch(LOWORD(wparam))
            {
                case IDOK:
                    //SaveTimeoutsDlg(hdlg);
                    set_macro_buffer(0, hdlg);
                    set_macro_buffer(1, hdlg);
                    set_macro_buffer(2, hdlg);
                    set_macro_buffer(3, hdlg);
                    set_macro_buffer(4, hdlg);
                    set_macro_buffer(5, hdlg);
                    set_macro_buffer(6, hdlg);
                    set_macro_buffer(7, hdlg);
                    set_macro_buffer(8, hdlg);
                    set_macro_buffer(9, hdlg);

                    save_macros_to_file(config_file);

                    //
                    // FALL THROUGH
                    //

                case IDCANCEL:
                    EndDialog(hdlg, LOWORD(wparam));
                    return TRUE;

                case IDC_MACRO1ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO1ASCIIRB, IDC_MACRO1HEXRB, IDC_MACRO1ASCIIRB);
                    return FALSE;
                case IDC_MACRO2ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO2ASCIIRB, IDC_MACRO2HEXRB, IDC_MACRO2ASCIIRB);
                    return FALSE;
                case IDC_MACRO3ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO3ASCIIRB, IDC_MACRO3HEXRB, IDC_MACRO3ASCIIRB);
                    return FALSE;
                case IDC_MACRO4ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO4ASCIIRB, IDC_MACRO4HEXRB, IDC_MACRO4ASCIIRB);
                    return FALSE;
                case IDC_MACRO5ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO5ASCIIRB, IDC_MACRO5HEXRB, IDC_MACRO5ASCIIRB);
                    return FALSE;
                case IDC_MACRO6ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO6ASCIIRB, IDC_MACRO6HEXRB, IDC_MACRO6ASCIIRB);
                    return FALSE;
                case IDC_MACRO7ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO7ASCIIRB, IDC_MACRO7HEXRB, IDC_MACRO7ASCIIRB);
                    return FALSE;
                case IDC_MACRO8ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO8ASCIIRB, IDC_MACRO8HEXRB, IDC_MACRO7ASCIIRB);
                    return FALSE;
                case IDC_MACRO9ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO9ASCIIRB, IDC_MACRO9HEXRB, IDC_MACRO9ASCIIRB);
                    return FALSE;
                case IDC_MACRO10ASCIIRB:
                    CheckRadioButton(hdlg, IDC_MACRO10ASCIIRB, IDC_MACRO10HEXRB, IDC_MACRO10ASCIIRB);
                    return FALSE;

                case IDC_MACRO1HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO1ASCIIRB, IDC_MACRO1HEXRB, IDC_MACRO1HEXRB);
                    return FALSE;
                case IDC_MACRO2HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO2ASCIIRB, IDC_MACRO2HEXRB, IDC_MACRO2HEXRB);
                    return FALSE;
                case IDC_MACRO3HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO3ASCIIRB, IDC_MACRO3HEXRB, IDC_MACRO3HEXRB);
                    return FALSE;
                case IDC_MACRO4HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO4ASCIIRB, IDC_MACRO4HEXRB, IDC_MACRO4HEXRB);
                    return FALSE;
                case IDC_MACRO5HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO5ASCIIRB, IDC_MACRO5HEXRB, IDC_MACRO5HEXRB);
                    return FALSE;
                case IDC_MACRO6HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO6ASCIIRB, IDC_MACRO6HEXRB, IDC_MACRO6HEXRB);
                    return FALSE;
                case IDC_MACRO7HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO7ASCIIRB, IDC_MACRO7HEXRB, IDC_MACRO7HEXRB);
                    return FALSE;
                case IDC_MACRO8HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO8ASCIIRB, IDC_MACRO8HEXRB, IDC_MACRO8HEXRB);
                    return FALSE;
                case IDC_MACRO9HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO9ASCIIRB, IDC_MACRO9HEXRB, IDC_MACRO9HEXRB);
                    return FALSE;
                case IDC_MACRO10HEXRB:
                    CheckRadioButton(hdlg, IDC_MACRO10ASCIIRB, IDC_MACRO10HEXRB, IDC_MACRO10HEXRB);
                    return FALSE;
            }
        break;
    }

    return FALSE;
}


/*-----------------------------------------------------------------------------

FUNCTION: InitHexControl(HWND, WORD, WORD, char)

PURPOSE: Places byte value into two edit boxes of the dialog

PARAMETERS:
    hdlg         - Dialog Handle
    wIdNumberBox - Edit control ID ; displays hex
    wIdCharBox   - Edit control ID ; displays char
    chData       - data to display

COMMENTS: Some dialogs may have an edit control designed to accept
	  hexidecimal input from the user.  This function initializes
	  such edit controls.  First, the byte (char) is placed into a
	  zero terminated string.  This is set as the item text of one
	  of the controls.  Next, the byte is converted to a hexidecimal
	  string.  This is set as the item text of the other control.

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void InitHexControl(HWND hdlg, WORD wIdNumberBox, WORD wIdCharBox, char chData)
{
    char szFlagText[3] = {0};
    char szFlagChar[2] = {0};

    //
    // put character into char edit display control
    //
    szFlagChar[0] = chData;
    SetDlgItemText(hdlg, wIdCharBox, szFlagChar);

    //
    // put flag character into hex numeric edit control
    //
    wsprintf(szFlagText, "%02x", 0x000000FF & chData);
    SetDlgItemText(hdlg, wIdNumberBox, szFlagText);

    return;
}


/*-----------------------------------------------------------------------------

FUNCTION: GetHexControl(HWND, WORD, WORD)

PURPOSE: Get hex data from control and convert to character

PARAMETERS:
    hdlg         - Dialog Handle
    wIdNumberBox - Edit control ID ; contains hex string
    wIdCharBox   - Edit control ID ; displays the char

RETURN:
    0 if can't get hex string from edit control
    byte value of hex string otherwise

COMMENTS: Function does the following:
	  1) Gets first two characters from edit control
	  2) Converts hex string to numeric value
	  3) Displays ascii char of numeric value
	  4) Returns numeric value

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
char GetHexControl(HWND hdlg, WORD wIdNumberBox, WORD wIdCharBox)
{
    UINT uFlagValue;
    char chFlagEntered[3] = {0};
    char chFlag[2] = {0};

    //
    // get numeric value from control
    //
    if (0 == GetDlgItemText(hdlg, wIdNumberBox, chFlagEntered, 3))
	return 0;

    sscanf(chFlagEntered, "%x", &uFlagValue);

    chFlag[0] = (char) uFlagValue;
    SetDlgItemText(hdlg, wIdCharBox, chFlag); // display character

    return chFlag[0];
}


/*-----------------------------------------------------------------------------

FUNCTION: InitCommEventsDlg(HWND, DWORD)

PURPOSE: Initializes Comm Event Dialog Control

PARAMETERS:
    hdlg         - Dialog window handle
    dwEventFlags - event flag to set controls to

COMMENTS: Since controls are checked based on the dwEventFlags parameter,
	  it is easy to init control based on current settings,
	  or default settings.

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void InitCommEventsDlg(HWND hdlg, DWORD dwEventFlags)
{
    int i,j;

    for (i = IDC_EVBREAKBTN, j = 0; i <= IDC_EVTXEMPTYBTN; i++, j++)
	CheckDlgButton( hdlg, i, dwEventFlags & EventFlagsTable[j]) ;

    InitHexControl(hdlg, IDC_FLAGEDIT, IDC_FLAGCHAR, FLAGCHAR(TTYInfo));

    EnableWindow(GetDlgItem(hdlg, IDC_FLAGEDIT),  dwEventFlags & EV_RXFLAG);
    EnableWindow(GetDlgItem(hdlg, IDC_FLAGCHAR),  dwEventFlags & EV_RXFLAG);

    return;
}


/*-----------------------------------------------------------------------------

FUNCTION: SaveCommEventsDlg(HWND)

PURPOSE: Saves new Comm Events Flag

PARAMETERS:
    hdlg - Dialog window handle

COMMENTS: Builds a new flag based on current dialog control.
	  If the new flag differs from old, then new is updated.

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void SaveCommEventsDlg(HWND hdlg)
{
    int i,j;
    DWORD dwNew = {0};
    char chNewFlag = '\0';
    BOOL fChangingRXFLAG;

    //
    // create a flag out of dialog selections
    //
    for (i = IDC_EVBREAKBTN, j = 0; i <= IDC_EVTXEMPTYBTN; i++, j++) {
	if (IsDlgButtonChecked(hdlg, i))
	    dwNew |= EventFlagsTable[j];
    }

    //
    // get current flag character from dialog
    //
    chNewFlag = GetHexControl(hdlg, IDC_FLAGEDIT, IDC_FLAGCHAR);
    fChangingRXFLAG = (EVENTFLAGS(TTYInfo) & EV_RXFLAG) != (dwNew & EV_RXFLAG);
    if (chNewFlag != FLAGCHAR(TTYInfo) || fChangingRXFLAG) {
	FLAGCHAR(TTYInfo) = chNewFlag;
	UpdateTTYInfo();
    }

    //
    // if new flags have been selected, or
    //
    if (dwNew != EVENTFLAGS(TTYInfo))
	EVENTFLAGS(TTYInfo) = dwNew;

    return;
}


/*-----------------------------------------------------------------------------

FUNCTION: CommEventsProc(HWND, UINT, WPARAM, LPARAM)

PURPOSE: Dialog Procedure for Comm Events Dialog

PARAMETERS:
    hdlg     - Dialog window handle
    uMessage - window message
    wparam   - message parameter (depends on message)
    lparam   - message parameter (depends on message)

RETURN:
    TRUE if message is handled
    FALSE if message is not handled
    Exception is WM_INITDIALOG: returns FALSE since focus is not set

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
BOOL CALLBACK CommEventsProc(HWND hdlg, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
    switch(uMessage)
    {
	case WM_INITDIALOG:     // init controls
	    InitCommEventsDlg(hdlg, EVENTFLAGS(TTYInfo));
	    break;

	case WM_COMMAND:
	    switch(LOWORD(wparam))
	    {
		case IDOK:
		    SaveCommEventsDlg(hdlg);

		    //
		    // FALL THROUGH
		    //

		case IDCANCEL:
		    EndDialog(hdlg, LOWORD(wparam));
		    return TRUE;

		case IDC_DEFAULTSBTN:
		    InitCommEventsDlg(hdlg, EVENTFLAGS_DEFAULT);
		    return TRUE;

		case IDC_EVRXFLAGBTN:
		    EnableWindow(GetDlgItem(hdlg, IDC_FLAGEDIT),  IsDlgButtonChecked(hdlg, IDC_EVRXFLAGBTN));
		    EnableWindow(GetDlgItem(hdlg, IDC_FLAGCHAR),  IsDlgButtonChecked(hdlg, IDC_EVRXFLAGBTN));
		    return TRUE;

		case IDC_FLAGEDIT:
		    GetHexControl(hdlg, IDC_FLAGEDIT, IDC_FLAGCHAR);
		    return TRUE;
	    }
	    break;
    }

    return FALSE;
}


/*-----------------------------------------------------------------------------

FUNCTION: SaveFlowControlDlg(HWND)

PURPOSE: Sets TTY flow control settings based on dlg controls

PARAMETERS:
    hdlg - Dialog window handle

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void SaveFlowControlDlg(HWND hdlg)
{
    BOOL    fNewCTSOut, fNewDSROut, fNewDSRIn, fNewXOut, fNewXIn, fNewTXafterXoffSent;
    DWORD   dwNewDTRControl, dwNewRTSControl;
    WORD    wNewXONLimit, wNewXOFFLimit;
    char    chNewXON, chNewXOFF;
    BOOL    fSuccess;

    BOOL fUpdateDCB = FALSE;

    //
    // update DTR and RTS control if needed
    //
    dwNewDTRControl = GetdwTTYItem( hdlg, IDC_DTRCONTROLCOMBO,
				    szDTRControlStrings, DTRControlTable,
				    sizeof(DTRControlTable)/sizeof(DTRControlTable[0]));

    dwNewRTSControl = GetdwTTYItem( hdlg, IDC_RTSCONTROLCOMBO,
				    szRTSControlStrings, RTSControlTable,
				    sizeof(RTSControlTable)/sizeof(RTSControlTable[0]));
    if (dwNewRTSControl != RTSCONTROL(TTYInfo) ||
	    dwNewDTRControl != DTRCONTROL(TTYInfo)) {
	RTSCONTROL(TTYInfo) = dwNewRTSControl;
	DTRCONTROL(TTYInfo) = dwNewDTRControl;
	fUpdateDCB = TRUE;
    }

    //
    // update XON/XOFF limits if needed
    //
    wNewXONLimit = GetDlgItemInt(hdlg, IDC_XONLIMITEDIT, &fSuccess, FALSE);
    wNewXOFFLimit = GetDlgItemInt(hdlg, IDC_XOFFLIMITEDIT, &fSuccess, FALSE);
    if (wNewXOFFLimit != XOFFLIMIT(TTYInfo) ||
	    wNewXONLimit != XONLIMIT(TTYInfo)) {
	XOFFLIMIT(TTYInfo) = wNewXOFFLimit;
	XONLIMIT(TTYInfo)  = wNewXONLimit;
	fUpdateDCB = TRUE;
    }

    //
    // update XON/XOFF chars if needed
    //
    chNewXON = GetHexControl(hdlg, IDC_XONCHAREDIT, IDC_XONCHARDISP);
    chNewXOFF = GetHexControl(hdlg, IDC_XOFFCHAREDIT, IDC_XOFFCHARDISP);
    if (chNewXOFF != XOFFCHAR(TTYInfo) ||
	    chNewXON != XONCHAR(TTYInfo)) {
	XOFFCHAR(TTYInfo) = chNewXOFF;
	XONCHAR(TTYInfo)  = chNewXON;
	fUpdateDCB = TRUE;
    }

    //
    // update booleans from check boxes
    //
    fNewTXafterXoffSent = IsDlgButtonChecked(hdlg, IDC_TXAFTERXOFFSENTCHK);
    fNewCTSOut = IsDlgButtonChecked(hdlg, IDC_CTSOUTCHK);
    fNewDSROut = IsDlgButtonChecked(hdlg, IDC_DSROUTCHK);
    fNewDSRIn  = IsDlgButtonChecked(hdlg, IDC_DSRINCHK);
    fNewXOut   = IsDlgButtonChecked(hdlg, IDC_XONXOFFOUTCHK);
    fNewXIn    = IsDlgButtonChecked(hdlg, IDC_XONXOFFINCHK);

    if (fNewTXafterXoffSent != TXAFTERXOFFSENT(TTYInfo) ||
	    fNewCTSOut != CTSOUTFLOW(TTYInfo) ||
	    fNewDSROut != DSROUTFLOW(TTYInfo) ||
	    fNewDSRIn  != DSRINFLOW(TTYInfo)  ||
	    fNewXOut   != XONXOFFOUTFLOW(TTYInfo) ||
	    fNewXIn    != XONXOFFINFLOW(TTYInfo) ) {
	CTSOUTFLOW(TTYInfo) = fNewCTSOut;
	DSROUTFLOW(TTYInfo) = fNewDSROut;
	DSRINFLOW(TTYInfo)  = fNewDSRIn;
	XONXOFFOUTFLOW(TTYInfo) = fNewXOut;
	XONXOFFINFLOW(TTYInfo)  = fNewXIn;
	TXAFTERXOFFSENT(TTYInfo) = fNewTXafterXoffSent;
	fUpdateDCB = TRUE;
    }

    //
    // update current settings if they have actually changed
    //
    if (fUpdateDCB)
	UpdateTTYInfo();

    return;
}


/*-----------------------------------------------------------------------------

FUNCTION: InitFlowControlDlg(HWND)

PURPOSE: Sets controls based on current tty flow control settings

PARAMETERS:
    hdlg - Dialog window handle

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void InitFlowControlDlg(HWND hdlg)
{
    //
    // fill and init DTR control combo
    //
    FillComboBox( GetDlgItem( hdlg, IDC_DTRCONTROLCOMBO ),
		 szDTRControlStrings, DTRControlTable,
		 sizeof( DTRControlTable) / sizeof( DTRControlTable[0] ),
		 DTRCONTROL( TTYInfo ) );

    //
    // fill and init RTS control combo
    //
    FillComboBox( GetDlgItem( hdlg, IDC_RTSCONTROLCOMBO ),
		 szRTSControlStrings, RTSControlTable,
		 sizeof( RTSControlTable) / sizeof( RTSControlTable[0] ),
		 RTSCONTROL( TTYInfo ) );

    //
    // XON/XOFF characters
    //
    InitHexControl(hdlg, IDC_XONCHAREDIT, IDC_XONCHARDISP, XONCHAR(TTYInfo));
    InitHexControl(hdlg, IDC_XOFFCHAREDIT, IDC_XOFFCHARDISP, XOFFCHAR(TTYInfo));

    //
    // XON/XOFF limits
    //
    SetDlgItemInt(hdlg, IDC_XONLIMITEDIT, XONLIMIT(TTYInfo), FALSE);
    SetDlgItemInt(hdlg, IDC_XOFFLIMITEDIT, XOFFLIMIT(TTYInfo), FALSE);

    //
    // check boxes
    //
    CheckDlgButton(hdlg, IDC_CTSOUTCHK, CTSOUTFLOW(TTYInfo));
    CheckDlgButton(hdlg, IDC_DSROUTCHK, DSROUTFLOW(TTYInfo));
    CheckDlgButton(hdlg, IDC_DSRINCHK, DSRINFLOW(TTYInfo));
    CheckDlgButton(hdlg, IDC_XONXOFFOUTCHK, XONXOFFOUTFLOW(TTYInfo));
    CheckDlgButton(hdlg, IDC_XONXOFFINCHK, XONXOFFINFLOW(TTYInfo));
    CheckDlgButton(hdlg, IDC_TXAFTERXOFFSENTCHK, TXAFTERXOFFSENT(TTYInfo));

    return;
}


/*-----------------------------------------------------------------------------

FUNCTION: FlowDefault(HWND, WORD)

PURPOSE: Sets controls based on hardware or software flow control

PARAMETERS:
    hdlg - Dialog window handle
    wId  - ID of button used to set the default:
	IDC_DTRDSRBTN     - DTR/DSR hardware flow-control
	IDC_RTSCTSBTN     - RTS/CTS hardware flow-control
	IDC_XOFFXONBTNBTN - XOFF/XON software flow control
	IDC_NONEBTN       - no flow control

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void FlowDefault(HWND hdlg, WORD wId)
{
    //
    // set dtr control to handshake if using DTR/DSR flow-control
    //
    SetComboBox( GetDlgItem( hdlg, IDC_DTRCONTROLCOMBO ),
		  sizeof( DTRControlTable) / sizeof( DTRControlTable[0] ),
		  wId == IDC_DTRDSRBTN ? DTR_CONTROL_HANDSHAKE : DTR_CONTROL_ENABLE);

    //
    // set rts control to handshake if using RTS/CTS flow-control
    //
    SetComboBox( GetDlgItem( hdlg, IDC_RTSCONTROLCOMBO ),
		  sizeof( RTSControlTable) / sizeof( RTSControlTable[0] ),
		  wId == IDC_RTSCTSBTN ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE);

    //
    // set check boxes according to wId
    //
    switch(wId)
    {
	case IDC_RTSCTSBTN:
	case IDC_DTRDSRBTN:
	    CheckDlgButton(hdlg, IDC_CTSOUTCHK, wId == IDC_RTSCTSBTN);
	    CheckDlgButton(hdlg, IDC_DSROUTCHK, wId == IDC_DTRDSRBTN);
	    CheckDlgButton(hdlg, IDC_XONXOFFOUTCHK, FALSE);
	    CheckDlgButton(hdlg, IDC_XONXOFFINCHK, FALSE);
	    break;

	case IDC_XOFFXONBTN:
	case IDC_NONEBTN:
	    CheckDlgButton(hdlg, IDC_CTSOUTCHK, FALSE);
	    CheckDlgButton(hdlg, IDC_DSROUTCHK, FALSE);
	    CheckDlgButton(hdlg, IDC_XONXOFFOUTCHK, wId == IDC_XOFFXONBTN);
	    CheckDlgButton(hdlg, IDC_XONXOFFINCHK, wId == IDC_XOFFXONBTN);
	    break;
    }

    //
    // settings that are disabled when any 'default' flow-control is used
    //
    CheckDlgButton(hdlg, IDC_DSRINCHK, FALSE);
    CheckDlgButton(hdlg, IDC_TXAFTERXOFFSENTCHK, FALSE);


    return;
}


/*-----------------------------------------------------------------------------

FUNCTION: FlowControlProc(HWND, UINT, WPARAM, LPARAM)

PURPOSE: Dialog Procedure for Flow Control Settings Dialog

PARAMETERS:
    hdlg     - Dialog window handle
    uMessage - window message
    wparam   - message parameter (depends on message)
    lparam   - message parameter (depends on message)

RETURN:
    TRUE if message is handled
    FALSE if message is not handled
    Exception is WM_INITDIALOG: returns FALSE since focus is not set

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
BOOL CALLBACK FlowControlProc(HWND hdlg, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
    switch(uMessage)
    {
	case WM_INITDIALOG:     // init controls
	    InitFlowControlDlg(hdlg);
	    break;

	case WM_COMMAND:
	    switch(LOWORD(wparam))
	    {
		case IDOK:
			SaveFlowControlDlg(hdlg);
			//
			// FALL THROUGH
			//

		case IDCANCEL:
			EndDialog(hdlg, LOWORD(wparam));
			return TRUE;

		case IDC_RTSCTSBTN:
		case IDC_DTRDSRBTN:
		case IDC_XOFFXONBTN:
		case IDC_NONEBTN:
			FlowDefault(hdlg, LOWORD(wparam));
			return TRUE;

		case IDC_XONCHAREDIT:
			GetHexControl(hdlg, IDC_XONCHAREDIT, IDC_XONCHARDISP);
			return TRUE;

		case IDC_XOFFCHAREDIT:
			GetHexControl(hdlg, IDC_XOFFCHAREDIT, IDC_XOFFCHARDISP);
			return TRUE;
	    }
	    break;
    }

    return FALSE;
}


/*-----------------------------------------------------------------------------

FUNCTION: InitTimeoutsDlg(HWND, COMMTIMEOUTS)

PURPOSE: Initializes timeouts dialog controls based on parameter

PARAMETERS:
    hdlg - Dialog window handle
    ct   - COMMTIMEOUTS used to set the dialog controls

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void InitTimeoutsDlg(HWND hdlg, COMMTIMEOUTS ct)
{
    SetDlgItemInt(hdlg, IDC_READINTERVALEDIT, ct.ReadIntervalTimeout, FALSE);
    SetDlgItemInt(hdlg, IDC_READMULTIPLIEREDIT, ct.ReadTotalTimeoutMultiplier, FALSE);
    SetDlgItemInt(hdlg, IDC_READCONSTANTEDIT, ct.ReadTotalTimeoutConstant, FALSE);
    SetDlgItemInt(hdlg, IDC_WRITEMULTIPLIEREDIT, ct.WriteTotalTimeoutMultiplier, FALSE);
    SetDlgItemInt(hdlg, IDC_WRITECONSTANTEDIT, ct.WriteTotalTimeoutConstant, FALSE);
    CheckDlgButton(hdlg, IDC_DISPLAYTIMEOUTS, SHOWTIMEOUTS(TTYInfo));
    return;
}

/*-----------------------------------------------------------------------------

FUNCTION: SaveTimeoutsDlg(HWND)

PURPOSE: Saves values from controls into tty timeout settings

PARAMETERS:
    hdlg - Dialog window handle

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
void SaveTimeoutsDlg(HWND hdlg)
{
    COMMTIMEOUTS ctNew;

    //
    // get new timeouts from dialog controls
    //
    ctNew.ReadIntervalTimeout         = GetDlgItemInt(hdlg, IDC_READINTERVALEDIT,    NULL, FALSE);
    ctNew.ReadTotalTimeoutMultiplier  = GetDlgItemInt(hdlg, IDC_READMULTIPLIEREDIT,  NULL, FALSE);
    ctNew.ReadTotalTimeoutConstant    = GetDlgItemInt(hdlg, IDC_READCONSTANTEDIT,    NULL, FALSE);
    ctNew.WriteTotalTimeoutMultiplier = GetDlgItemInt(hdlg, IDC_WRITEMULTIPLIEREDIT, NULL, FALSE);
    ctNew.WriteTotalTimeoutConstant   = GetDlgItemInt(hdlg, IDC_WRITECONSTANTEDIT,   NULL, FALSE);

    SHOWTIMEOUTS(TTYInfo) = IsDlgButtonChecked(hdlg, IDC_DISPLAYTIMEOUTS);

    //
    // set new timeouts if they are different
    //
    if (memcmp(&ctNew, &(TIMEOUTSNEW(TTYInfo)), sizeof(COMMTIMEOUTS))) {
        //
        // if connected, set new time outs and purge pending operations
        //
        if (CONNECTED(TTYInfo)) {
            if (!SetCommTimeouts(COMDEV(TTYInfo), &ctNew)) {
            ErrorReporter("SetCommTimeouts");
            return;
            }

            if (!PurgeComm(COMDEV(TTYInfo), PURGE_TXABORT | PURGE_RXABORT))
            ErrorReporter("PurgeComm");
        }

        //
        // save timeouts in the tty info structure
        //
        TIMEOUTSNEW(TTYInfo) = ctNew;
    }

    return;
}

/*-----------------------------------------------------------------------------

FUNCTION: TimeoutsProc(HWND, UINT, WPARAM, LPARAM)

PURPOSE: Dialog Procedure for comm timeouts

PARAMETERS:
    hdlg     - Dialog window handle
    uMessage - window message
    wparam   - message parameter (depends on message)
    lparam   - message parameter (depends on message)

RETURN:
    TRUE if message is handled
    FALSE if message is not handled
    Exception is WM_INITDIALOG: returns FALSE since focus is not set

HISTORY:   Date:      Author:     Comment:
	   10/27/95   AllenD      Wrote it

-----------------------------------------------------------------------------*/
BOOL CALLBACK TimeoutsProc(HWND hdlg, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
    switch(uMessage)
    {
	case WM_INITDIALOG:     // init controls
	    InitTimeoutsDlg(hdlg, TIMEOUTSNEW(TTYInfo));
	    break;

	case WM_COMMAND:
	    switch(LOWORD(wparam))
	    {
		case IDOK:
			SaveTimeoutsDlg(hdlg);

			//
			// FALL THROUGH
			//

		case IDCANCEL:
			EndDialog(hdlg, LOWORD(wparam));
			return TRUE;

		case IDC_DEFAULTSBTN:
			InitTimeoutsDlg(hdlg, gTimeoutsDefault);
			return TRUE;
	    }
	    break;
    }

    return FALSE;
}

BOOL CALLBACK GetADWORDProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    int iRet = 0;

    if (uMessage == WM_COMMAND) {
	switch(LOWORD(wParam)) {
	    case IDOK:
		iRet = GetDlgItemInt(hDlg, IDC_DWORDEDIT, NULL, FALSE);
		//
		// FALL THROUGH
		//

	    case IDCANCEL:
		EndDialog(hDlg, iRet);
		return TRUE;
	}
    }

    return FALSE;
}

DWORD GetAFrequency()
{
    return ((DWORD) DialogBox(ghInst, MAKEINTRESOURCE(IDD_GETADWORD), ghwndMain, GetADWORDProc));
}
