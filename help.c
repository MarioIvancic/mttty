
#include <windows.h>
#include "mttty.h"

/*
    Prototypes for functions called only in this file
*/
BOOL CALLBACK HelpDlgProc( HWND, UINT, WPARAM, LPARAM );
void InitHelpDlg( HWND );


BOOL CmdHelp(HWND hwnd)
{
    DialogBox(ghInst, MAKEINTRESOURCE(IDD_HELP), hwnd, HelpDlgProc);
    return 0;
}



void InitHelpDlg(HWND hDlg)
{
    static const char *helpText = "In HEX macros any character other than 0-9, a-f, A-F are ignored so you can use any other character as separator or no separator at all.\r\n\r\n"
    "In ASCII macros you can use escape sequences \\n \\r \\t \\0 \\\\ \\xhh where hh are two hexadecimal digits.";
    SetDlgItemText(hDlg, IDC_HELPTEXT, helpText);
}


BOOL CALLBACK HelpDlgProc(HWND hdlg, UINT uMessage, WPARAM wparam, LPARAM lparam)
{
    switch(uMessage)
    {
        case WM_INITDIALOG:
            InitHelpDlg(hdlg);
        break;

        case WM_COMMAND:
            if (LOWORD(wparam) == IDOK)
            {
                EndDialog(hdlg, TRUE);
                return TRUE;
            }
        break;
    }

    return FALSE;
}


