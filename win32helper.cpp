#undef NOMINMAX
#undef UNICODE
#include <Windows.h>
#include <VersionHelpers.h>
#include <gdiplus.h>
#include <gdiplusheaders.h>
#include <TlHelp32.h>

#pragma comment(lib, "Gdi32")
#pragma comment(lib, "User32")

bool supportTranslucentBackground()
{
    return IsWindows8OrGreater();
}

int getProcessId()
{
    return static_cast<int>(::GetCurrentProcessId());
}

int getParentProcessId(int pid)
{
    HANDLE h = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    int ppid = 0;
    PROCESSENTRY32 pe = { sizeof(pe), 0, 0, 0, 0, 0, 0, 0, 0, {0} };
    if (::Process32First(h, &pe)) {
        do {
            if (static_cast<int>(pe.th32ProcessID) == pid) {
                ppid = static_cast<int>(pe.th32ParentProcessID);
                break;
            }
        } while (::Process32Next(h, &pe));
    }
    ::CloseHandle(h);
    return ppid;
}

int findProcessId(char const * name, bool latest)
{
    int pid = 0;
    HANDLE hProcSnapshot = ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if( INVALID_HANDLE_VALUE != hProcSnapshot )
    {
        FILETIME lpCreationTime;
        FILETIME lpExitTime;
        FILETIME lpKernelTime;
        FILETIME lpUserTime;
        FILETIME lpCreationTime2 = {0, 0};
        if (!latest) {
            SYSTEMTIME systemTime;
            ::GetSystemTime(&systemTime);
            ::SystemTimeToFileTime(&systemTime, &lpCreationTime2);
        }
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(PROCESSENTRY32);
        if( ::Process32First( hProcSnapshot, &procEntry ) )
        {
            do
            {
                HANDLE hModSnapshot = ::CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procEntry.th32ProcessID );
                if( INVALID_HANDLE_VALUE != hModSnapshot )
                {
                    MODULEENTRY32 modEntry;
                    modEntry.dwSize = sizeof( MODULEENTRY32 );
                    if( Module32First( hModSnapshot, &modEntry ) )
                    {
                        if( strcmp(name, modEntry.szModule) == 0)
                        {
                            ::CloseHandle( hModSnapshot );

                            HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS,
                                                       FALSE, procEntry.th32ProcessID);
                            ::GetProcessTimes(hProc,
                                              &lpCreationTime,
                                              &lpExitTime,
                                              &lpKernelTime,
                                              &lpUserTime);
                            ::CloseHandle(hProc);
                            if ((CompareFileTime(&lpCreationTime, &lpCreationTime2) > 0) == latest)
                            {
                                pid = procEntry.th32ProcessID;
                                lpCreationTime2 = lpCreationTime;
                            }
                        }
                    }
                    ::CloseHandle( hModSnapshot );
                }
                else
                {
                    //auto msg = FormatErrorMsg(GetLastError());
                    //wprintf(TEXT("Process %d %x\n"), procEntry.th32ProcessID, GetLastError());
                }
            }
            while( ::Process32Next( hProcSnapshot, &procEntry ) );
        }
        ::CloseHandle( hProcSnapshot );
    }
    return pid;
}

struct EnumWindowParam
{
    unsigned long pid;
    char const ** titleParts;
    HWND hwnd;
};

BOOL CALLBACK EnumWindowsProc(_In_ HWND hwnd, _In_ LPARAM lParam)
{
    char temp[256] = {0};
    unsigned long pid = 0;
    EnumWindowParam * param = reinterpret_cast<EnumWindowParam *>(lParam);
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ::GetCurrentProcessId() // avoid deadlock
            || (param->pid && pid != param->pid))
        return TRUE;
    ::GetWindowTextA(hwnd, temp, 255);
    char * p = temp;
    char const ** titleParts = param->titleParts;
    while (*titleParts && (p = strstr(p, *titleParts))) {
        ++titleParts;
    }
    if (*titleParts)
        return TRUE;
    param->hwnd = hwnd;
    return FALSE;
}

intptr_t findWindow(int pid, char const * titleParts[])
{
    EnumWindowParam param = {static_cast<unsigned long>(pid), titleParts, nullptr};
    ::EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&param));
    return reinterpret_cast<intptr_t>(param.hwnd);
}

bool isWindowValid(intptr_t hwnd)
{
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    return ::IsWindow(hWnd);
}

bool isWindowShown(intptr_t hwnd)
{
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    return ::IsWindow(hWnd) && (::GetWindowLongA(hWnd, GWL_STYLE) & WS_VISIBLE);
}

void showWindow(intptr_t hwnd)
{
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

void hideWindow(intptr_t hwnd)
{
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
}

void setWindowAtTop(intptr_t hwnd)
{
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void closeWindow(intptr_t hwnd)
{
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    ::CloseWindow(hWnd);
}

void attachWindow(intptr_t hwnd, intptr_t hwndParent, int left, int top)
{
    HWND hWndParent = reinterpret_cast<HWND>(hwndParent);
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    LONG style = ::GetWindowLongA(hWnd, GWL_STYLE);
    style |= WS_CHILDWINDOW;
    style &= ~WS_POPUP;
    style &= ~WS_SYSMENU;
    style &= ~WS_MAXIMIZEBOX;
    style &= ~WS_MINIMIZEBOX;
    ::SetWindowLongA(hWnd, GWL_STYLE, style);
    ::SetParent(hWnd, hWndParent);
    if (left < 0 || top < 0) {
        RECT rect;
        ::GetWindowRect(hWndParent, &rect);
        if (left < 0)
            left += rect.right - rect.left;
        if (top < 0)
            top += rect.bottom - rect.top;
    }
    ::SetWindowPos(hWnd, HWND_TOP, left, top, 0, 0, SWP_NOSIZE);
    //::ShowWindow(hWnd, SW_SHOWNORMAL);
    //::SetActiveWindow(hWndParent);
    ::SetFocus(hWndParent);
}

void moveChildWindow(intptr_t hwndParent, intptr_t hwnd, int dx, int dy)
{
    HWND hWndParent = reinterpret_cast<HWND>(hwndParent);
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    RECT rectParent, rect;
    ::GetWindowRect(hWndParent, &rectParent);
    ::GetWindowRect(hWnd, &rect);
    ::SetWindowPos(hWnd, HWND_TOP,
                   rect.left - rectParent.left + dx,
                   rect.top - rectParent.top + dy,
                   0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void setArrowCursor()
{
    HCURSOR hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    ::SetCursor(hCursor);
}

void showCursor()
{
    ::ShowCursor(true);
}

/*
 * desk: capture desktop window, then split by hwnd rect
 */
int captureImage(intptr_t hwnd, char ** out, int * nout, bool desk)
{
    HWND hWnd = reinterpret_cast<HWND>(hwnd);
    HDC hdcWindow;
    HDC hdcMemDC = nullptr;
    HBITMAP hbMem = nullptr;
    BITMAP bmpMem;

    // Retrieve the handle to a display device context for the client
    // area of the window.
    if (hwnd == -1)
        hWnd = ::GetDesktopWindow();
    HWND hWnd2 = desk ? ::GetDesktopWindow() : hWnd;
    hdcWindow = GetDC(hWnd2);

    // Create a compatible DC which is used in a BitBlt from the window DC
    hdcMemDC = CreateCompatibleDC(hdcWindow);

    if(!hdcMemDC)
    {
        MessageBox(hWnd, "CreateCompatibleDC has failed","Failed", MB_OK);
        goto done;
    }

    // Get the client area for size calculation
    RECT rcClient;
    ::GetClientRect(hWnd, &rcClient);
    POINT ptClient = {rcClient.left, rcClient.top};
    SIZE szClient = {rcClient.right-rcClient.left, rcClient.bottom-rcClient.top};
    if (desk) {
        ::ClientToScreen(hWnd, &ptClient);
    }


    // Create a compatible bitmap from the Window DC
    hbMem = ::CreateCompatibleBitmap(hdcWindow, szClient.cx, szClient.cy);

    if(!hbMem)
    {
        MessageBox(hWnd, "CreateCompatibleBitmap Failed","Failed", MB_OK);
        goto done;
    }

    // Select the compatible bitmap into the compatible memory DC.
    SelectObject(hdcMemDC,hbMem);

    // Bit block transfer into our compatible memory DC.
    if(!BitBlt(hdcMemDC,
               0,
               0,
               szClient.cx,
               szClient.cy,
               hdcWindow,
               ptClient.x,
               ptClient.y,
               SRCCOPY))
    {
        MessageBox(hWnd, "BitBlt has failed", "Failed", MB_OK);
        goto done;
    }

    // Get the BITMAP from the HBITMAP
    GetObject(hbMem,sizeof(BITMAP),&bmpMem);

    BITMAPFILEHEADER   bmfHeader;
    BITMAPINFOHEADER   bi;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmpMem.bmWidth;
    bi.biHeight = bmpMem.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    DWORD dwBmpSize = ((bmpMem.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmpMem.bmHeight;

    // Add the size of the headers to the size of the bitmap to get the total file size
    DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    //Offset to where the actual bitmap bits start.
    bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);

    //Size of the file
    bmfHeader.bfSize = dwSizeofDIB;

    //bfType must always be BM for Bitmaps
    bmfHeader.bfType = 0x4D42; //BM

    char *lpbitmap = new char[dwSizeofDIB];

    memcpy(lpbitmap, &bmfHeader, sizeof(BITMAPFILEHEADER));

    memcpy(lpbitmap + sizeof(BITMAPFILEHEADER), &bi, sizeof(BITMAPINFOHEADER));

    // Gets the "bits" from the bitmap and copies them into a buffer
    // which is pointed to by lpbitmap.
    GetDIBits(hdcWindow, hbMem, 0,
        (UINT)bmpMem.bmHeight,
        lpbitmap + bmfHeader.bfOffBits,
        (BITMAPINFO *)&bi, DIB_RGB_COLORS);

    *out = lpbitmap;
    *nout = dwSizeofDIB;

    //Clean up
done:
    DeleteObject(hbMem);
    DeleteObject(hdcMemDC);
    ReleaseDC(hWnd,hdcWindow);

    return 0;
}
/*
bool saveGdiImage(char* data, int size, char** out, int * nout)
{
#ifdef _DEBUG
    static ULONG_PTR token = 0;
    if (token == 0) {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartupOutput output;
        Gdiplus::GdiplusStartup(&token, &input, &output);
    }
    IStream *from = nullptr;
    ::CreateStreamOnHGlobal(nullptr, true, &from);
    from->Write(data, static_cast<ULONG>(size), nullptr);
    from->Seek({{0, 0}}, STREAM_SEEK_SET, nullptr);
    Gdiplus::Image *image = Gdiplus::Image::FromStream(from);
    if (!image)
        return false;
    from->Release();
    IStream *to = nullptr;
    ::CreateStreamOnHGlobal(nullptr, true, &to);
//    image/bmp  : {557cf400-1a04-11d3-9a73-0000f81ef32e}
//    image/jpeg : {557cf401-1a04-11d3-9a73-0000f81ef32e}
//    image/gif  : {557cf402-1a04-11d3-9a73-0000f81ef32e}
//    image/tiff : {557cf405-1a04-11d3-9a73-0000f81ef32e}
//    image/png  : {557cf406-1a04-11d3-9a73-0000f81ef32e}
    CLSID pngClsid = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    //Gdiplus::GetEncoderClsid(L"image/png", &pngClsid);
    image->Save(to, &pngClsid, nullptr);
    delete image;
    ULARGE_INTEGER end;
    to->Seek({{0, 0}}, STREAM_SEEK_END, &end);
    to->Seek({{0, 0}}, STREAM_SEEK_SET, nullptr);
    *nout = static_cast<int>(end.LowPart);
    *out = new char[static_cast<unsigned int>(*nout)];
    to->Read(*out, static_cast<ULONG>(*nout), nullptr);
    to->Release();
#endif
    return true;
}
*/
