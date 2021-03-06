/***************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  OpenCPN Platform specific support utilities
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2015 by David S. Register                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 **************************************************************************/


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
#include "wx/wx.h"
#endif //precompiled headers

#include <wx/app.h>
#include <wx/apptrait.h>
#include "wx/stdpaths.h"
#include <wx/filename.h>

#include "OCPNPlatform.h"

#include "chart1.h"
#include "cutil.h"

#ifdef __OCPN__ANDROID__
#include "androidUTIL.h"
#include "qdebug.h"
#endif

// Include CrashRpt Header
#ifdef OCPN_USE_CRASHRPT
#include "CrashRpt.h"
#include <new.h>
#endif

#ifdef LINUX_CRASHRPT
#include "crashprint.h"
#endif

#ifndef __WXMSW__
#include <signal.h>
#include <setjmp.h>
#endif

DECLARE_APP(MyApp)

void appendOSDirSlash( wxString* pString );


#ifndef __WXMSW__
struct sigaction          sa_all;
struct sigaction          sa_all_old;
extern sigjmp_buf env;                    // the context saved by sigsetjmp();
#endif


extern int                       quitflag;
extern MyFrame                   *gFrame;
extern bool                      g_bportable;


wxLog       *g_logger;





//  OCPN Platform implementation

OCPNPlatform::OCPNPlatform()
{
}

OCPNPlatform::~OCPNPlatform()
{
}

//--------------------------------------------------------------------------
//      Per-Platform Initialization support
//--------------------------------------------------------------------------
#ifdef __WXMSW__
int MyNewHandler( size_t size )
{
    //  Pass to wxWidgets Main Loop handler
    throw std::bad_alloc();
    
    return 0;
}
#endif

//-----------------------------------------------------------------------
//      Signal Handlers
//-----------------------------------------------------------------------
#ifndef __WXMSW__

//These are the signals possibly expected
//      SIGUSR1
//      Raised externally to cause orderly termination of application
//      Intended to act just like pushing the "EXIT" button

//      SIGSEGV
//      Some undefined segfault......

void
catch_signals(int signo)
{
    switch(signo)
    {
        case SIGUSR1:
            quitflag++;                             // signal to the timer loop
            break;
            
        case SIGSEGV:
            siglongjmp(env, 1);// jump back to the setjmp() point
            break;
            
        case SIGTERM:
            gFrame->Close();
            break;
            
        default:
            break;
    }
    
}
#endif

#ifdef OCPN_USE_CRASHRPT
// Define the crash callback
int CALLBACK CrashCallback(CR_CRASH_CALLBACK_INFO* pInfo)
{
    //  Flush log file
    if( g_logger)
        g_logger->Flush();
    
    return CR_CB_DODEFAULT;
}
#endif



//  Called from MyApp() immediately upon entry to MyApp::OnInit()
void OCPNPlatform::Initialize_1( void )
{
    
#ifdef OCPN_USE_CRASHRPT
#ifndef _DEBUG
    // Install Windows crash reporting
    
    CR_INSTALL_INFO info;
    memset(&info, 0, sizeof(CR_INSTALL_INFO));
    info.cb = sizeof(CR_INSTALL_INFO);
    info.pszAppName = _T("OpenCPN");
    
    wxString version_crash = str_version_major + _T(".") + str_version_minor + _T(".") + str_version_patch;
    info.pszAppVersion = version_crash.c_str();
    
    int type = MiniDumpWithDataSegs;  // Include the data sections from all loaded modules.
    // This results in the inclusion of global variables
    
    type |=  MiniDumpNormal;// | MiniDumpWithPrivateReadWriteMemory | MiniDumpWithIndirectlyReferencedMemory;
    info.uMiniDumpType = (MINIDUMP_TYPE)type;
    
    // Install all available exception handlers....
    info.dwFlags = CR_INST_ALL_POSSIBLE_HANDLERS;
    
    //  Except memory allocation failures
    info.dwFlags &= ~CR_INST_NEW_OPERATOR_ERROR_HANDLER;
    
    //  Allow user to attach files
    info.dwFlags |= CR_INST_ALLOW_ATTACH_MORE_FILES;
    
    //  Allow user to add more info
    info.dwFlags |= CR_INST_SHOW_ADDITIONAL_INFO_FIELDS;
    
    
    // URL for sending error reports over HTTP.
    if(g_bEmailCrashReport){
        info.pszEmailTo = _T("opencpn@bigdumboat.com");
        info.pszSmtpProxy = _T("mail.bigdumboat.com:587");
        info.pszUrl = _T("http://bigdumboat.com/crashrpt/ocpn_crashrpt.php");
        info.uPriorities[CR_HTTP] = 1;  // First try send report over HTTP
    }
    else{
        info.dwFlags |= CR_INST_DONT_SEND_REPORT;
        info.uPriorities[CR_HTTP] = CR_NEGATIVE_PRIORITY;       // don't send at all
    }
    
    info.uPriorities[CR_SMTP] = CR_NEGATIVE_PRIORITY;  // Second try send report over SMTP
    info.uPriorities[CR_SMAPI] = CR_NEGATIVE_PRIORITY; //1; // Third try send report over Simple MAPI
    
    wxStandardPaths& crash_std_path = *dynamic_cast<wxStandardPaths*>(&wxApp::GetTraits()->GetStandardPaths());
    wxString crash_rpt_save_locn = crash_std_path.GetConfigDir();
    if( g_bportable ) {
        wxFileName exec_path_crash( crash_std_path.GetExecutablePath() );
        crash_rpt_save_locn = exec_path_crash.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR );
    }
    
    wxString locn = crash_rpt_save_locn + _T("\\CrashReports");
    
    if(!wxDirExists( locn ) )
        wxMkdir( locn );
    
    if(wxDirExists( locn ) ){
        wxCharBuffer buf = locn.ToUTF8();
        wchar_t wlocn[256];
        if(buf && (locn.Length() < sizeof(wlocn)) ){
            MultiByteToWideChar( 0, 0, buf.data(), -1, wlocn, sizeof(wlocn)-1);
            info.pszErrorReportSaveDir = (LPCWSTR)wlocn;
        }
    }
    
    // Provide privacy policy URL
    wxStandardPathsBase& std_path_crash = wxApp::GetTraits()->GetStandardPaths();
    std_path_crash.Get();
    wxFileName exec_path_crash( std_path_crash.GetExecutablePath() );
    wxString policy_file =  exec_path_crash.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR );
    policy_file += _T("PrivacyPolicy.txt");
    policy_file.Prepend(_T("file:"));
    
    info.pszPrivacyPolicyURL = policy_file.c_str();;
    
    int nResult = crInstall(&info);
    if(nResult!=0) {
        TCHAR buff[256];
        crGetLastErrorMsg(buff, 256);
        MessageBox(NULL, buff, _T("crInstall error, Crash Reporting disabled."), MB_OK);
    }
    
    // Establish the crash callback function
    crSetCrashCallback( CrashCallback, NULL );
    
    // Take screenshot of the app window at the moment of crash
    crAddScreenshot2(CR_AS_PROCESS_WINDOWS|CR_AS_USE_JPEG_FORMAT, 95);
    
    //  Mark some files to add to the crash report
    wxString home_data_crash = std_path_crash.GetConfigDir();
    if( g_bportable ) {
        wxFileName f( std_path_crash.GetExecutablePath() );
        home_data_crash = f.GetPath();
    }
    appendOSDirSlash( &home_data_crash );
    
    wxString config_crash = _T("opencpn.ini");
    config_crash.Prepend( home_data_crash );
    crAddFile2( config_crash.c_str(), NULL, NULL, CR_AF_MISSING_FILE_OK | CR_AF_ALLOW_DELETE );
    
    wxString log_crash = _T("opencpn.log");
    log_crash.Prepend( home_data_crash );
    crAddFile2( log_crash.c_str(), NULL, NULL, CR_AF_MISSING_FILE_OK | CR_AF_ALLOW_DELETE );
    
#endif
#endif

    
#ifdef LINUX_CRASHRPT
#if wxUSE_ON_FATAL_EXCEPTION
    // fatal exceptions handling
    wxHandleFatalExceptions (true);
#endif
#endif

#ifdef __WXMSW__
    //  Invoke my own handler for failures of malloc/new
    _set_new_handler( MyNewHandler );
    //  configure malloc to call the New failure handler on failure
    _set_new_mode(1);
#endif    
    
    //    On MSW, force the entire process to run on one CPU core only
    //    This resolves some difficulty with wxThread syncronization
#if 0
#ifdef __WXMSW__
    //Gets the current process handle
    HANDLE hProc = GetCurrentProcess();
    DWORD procMask;
    DWORD sysMask;
    HANDLE hDup;
    DuplicateHandle( hProc, hProc, hProc, &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS );
    
    //Gets the current process affinity mask
    GetProcessAffinityMask( hDup, &procMask, &sysMask );
    
    // Take a simple approach, and assume up to 4 processors
    DWORD newMask;
    if( ( procMask & 1 ) == 1 ) newMask = 1;
    else
        if( ( procMask & 2 ) == 2 ) newMask = 2;
        else
            if( ( procMask & 4 ) == 4 ) newMask = 4;
            else
                if( ( procMask & 8 ) == 8 ) newMask = 8;
                
                //Set te affinity mask for the process
                BOOL res = SetProcessAffinityMask( hDup, (DWORD_PTR) newMask );
            
            if( res == 0 ) {
                //Error setting affinity mask!!
            }
#endif
#endif
            
#ifdef __WXMSW__
            
            //    Handle any Floating Point Exceptions which may leak thru from other
            //    processes.  The exception filter is in cutil.c
            //    Seems to only happen for W98
            
            wxPlatformInfo Platform;
            if( Platform.GetOperatingSystemId() == wxOS_WINDOWS_9X ) SetUnhandledExceptionFilter (&MyUnhandledExceptionFilter);
#endif
            
#ifdef __WXMSW__
            //     _CrtSetBreakAlloc(25503);
#endif
            

#ifndef __WXMSW__
            //      Setup Linux SIGNAL handling, for external program control
            
            //      Build the sigaction structure
            sa_all.sa_handler = catch_signals;// point to my handler
            sigemptyset(&sa_all.sa_mask);// make the blocking set
            // empty, so that all
            // other signals will be
            // unblocked during my handler
            sa_all.sa_flags = 0;
            
            sigaction(SIGUSR1, NULL, &sa_all_old);// save existing action for this signal
            
            //      Register my request for some signals
            sigaction(SIGUSR1, &sa_all, NULL);
            
            sigaction(SIGUSR1, NULL, &sa_all_old);// inspect existing action for this signal
            
            sigaction(SIGTERM, &sa_all, NULL);
            sigaction(SIGTERM, NULL, &sa_all_old);
#endif
            
}

//  Called from MyApp() immediately before creation of MyFrame()
void Initialize_2( void ){
}

//  Called from MyApp() just before end of MyApp::OnInit()
void Initialize_3( void ){
}


//--------------------------------------------------------------------------
//      Per-Platform file/directory support
//--------------------------------------------------------------------------

wxStandardPaths& OCPNPlatform::GetStdPaths()
{
#ifndef __OCPN__ANDROID__    
    return *dynamic_cast<wxStandardPaths*>(&(wxGetApp().GetTraits()->GetStandardPaths()));
#else 
//    return *dynamic_cast<wxStandardPaths*>(&wxApp::GetTraits()->GetStandardPaths());
    return *dynamic_cast<wxStandardPaths*>(&(wxTheApp->GetTraits())->GetStandardPaths());
#endif    
    
}


wxString &OCPNPlatform::GetHomeDir()
{
    if(m_homeDir.IsEmpty()){

        //      Establish a "home" location
 //       wxStandardPaths& std_path = *dynamic_cast<wxStandardPaths*>(&wxApp::GetTraits()->GetStandardPaths());
        wxStandardPaths& std_path = GetStdPaths();
        //        wxStandardPaths &std_path = ( wxStandardPaths) wxGetApp().GetTraits()->GetStandardPaths();
        
        //TODO  Why is the following preferred?  Will not compile with gcc...
//    wxStandardPaths& std_path = wxApp::GetTraits()->GetStandardPaths();

#ifdef __unix__
        std_path.SetInstallPrefix(wxString(PREFIX, wxConvUTF8));
#endif

#ifdef __WXMSW__
        m_homeDir =  std_path.GetConfigDir();   // on w98, produces "/windows/Application Data"
#else
        m_homeDir = std_path.GetUserConfigDir();
#endif

//  On android, make the private data dir on the sdcard, if it exists.
//  This make debugging easier, as it is not deleted whenever the APK is re-deployed.
//  This behaviour should go away at Release.
#ifdef __OCPN__ANDROID__
        if( wxDirExists(_T("/mnt/sdcard")) ){
            m_homeDir =  _T("/mnt/sdcard/.opencpn");
        }
#endif

        if( g_bportable ) 
            m_homeDir = GetExePath();
        
#ifdef  __WXOSX__
        appendOSDirSlash(&m_homeDir);
        m_homeDir.Append(_T("opencpn"));
#endif            

        appendOSDirSlash( &m_homeDir );
    }
    
    return m_homeDir;
}

wxString &OCPNPlatform::GetExePath()
{
    if(m_exePath.IsEmpty()){
        
        wxStandardPaths& std_path = GetStdPaths();
        m_exePath = std_path.GetExecutablePath();
    }
    
    return m_exePath;
}

wxString &OCPNPlatform::GetSharedDataDir()
{
    if(m_SData_Dir.IsEmpty()){
        //      Establish a "shared data" location
        /*  From the wxWidgets documentation...
         * 
         *     wxStandardPaths::GetDataDir
         *     wxString GetDataDir() const
         *     Return the location of the applications global, i.e. not user-specific, data files.
         * Unix: prefix/share/appname
         * Windows: the directory where the executable file is located
         * Mac: appname.app/Contents/SharedSupport bundle subdirectory
         */
        wxStandardPaths& std_path = GetStdPaths();
        m_SData_Dir = std_path.GetDataDir();
        appendOSDirSlash( &m_SData_Dir );
        
#ifdef __OCPN__ANDROID__
        wxFileName fdir = wxFileName::DirName(std_path.GetUserConfigDir());
        
        fdir.RemoveLastDir();
        m_SData_Dir = fdir.GetPath();
        m_SData_Dir += _T("/cache/");
#endif
        
        if( g_bportable )
            m_SData_Dir = GetHomeDir();
    }
    
    return m_SData_Dir;
    
}

wxString &OCPNPlatform::GetPrivateDataDir()
{
    if(m_PrivateDataDir.IsEmpty()){
        //      Establish the prefix of the location of user specific data files
        wxStandardPaths& std_path = GetStdPaths();
        
#ifdef __WXMSW__
        m_PrivateDataDir = GetHomeDir();                     // should be {Documents and Settings}\......
#elif defined __WXOSX__
        m_PrivateDataDir = std_path.GetUserConfigDir();     // should be ~/Library/Preferences
#else
        m_PrivateDataDir = std_path.GetUserDataDir();       // should be ~/.opencpn
#endif
        
        if( g_bportable )
            m_PrivateDataDir = GetHomeDir();
        
#ifdef __OCPN__ANDROID__
        m_PrivateDataDir = GetHomeDir();
#endif
    }
    
    return m_PrivateDataDir;
}


wxString &OCPNPlatform::GetPluginDir()
{
    if(m_PluginsDir.IsEmpty()){

        wxStandardPaths& std_path = GetStdPaths();
        
        //  Get the PlugIns directory location
        m_PluginsDir = std_path.GetPluginsDir();   // linux:   {prefix}/lib/opencpn
        // Mac:     appname.app/Contents/PlugIns
#ifdef __WXMSW__
        m_PluginsDir += _T("\\plugins");             // Windows: {exe dir}/plugins
#endif
        
        if( g_bportable ) {
            m_PluginsDir = GetHomeDir();
            m_PluginsDir += _T("plugins");
        }
        
    }
    
    return m_PluginsDir;
}


wxString &OCPNPlatform::GetConfigFileName()
{
    if(m_config_file_name.IsEmpty()){
        //      Establish the location of the config file
        wxStandardPaths& std_path = GetStdPaths();
        
#ifdef __WXMSW__
        m_config_file_name = _T("opencpn.ini");
        m_config_file_name.Prepend( GetHomeDir() );
        
#elif defined __WXOSX__
        m_config_file_name = std_path.GetUserConfigDir(); // should be ~/Library/Preferences
        appendOSDirSlash(&m_config_file_name);
        m_config_file_name.Append(_T("opencpn.ini"));
#else
        m_config_file_name = std_path.GetUserDataDir(); // should be ~/.opencpn
        appendOSDirSlash(&m_config_file_name);
        m_config_file_name.Append(_T("opencpn.conf"));
#endif
        
        if( g_bportable ) {
            m_config_file_name = GetHomeDir();
#ifdef __WXMSW__
            m_config_file_name += _T("opencpn.ini");
#elif defined __WXOSX__
            m_config_file_name +=_T("opencpn.ini");
#else
            m_config_file_name += _T("opencpn.conf");
#endif
            
        }
        
#ifdef __OCPN__ANDROID__
        m_config_file_name = GetHomeDir();
        m_config_file_name += _T("opencpn.conf");
#endif
        
    }
    
    return m_config_file_name;
}

wxString *OCPNPlatform::GetPluginDirPtr()
{
    return &m_PluginsDir;
}
wxString *OCPNPlatform::GetSharedDataDirPtr()
{
    return &m_SData_Dir;
}
wxString *OCPNPlatform::GetPrivateDataDirPtr()
{
    return &m_PrivateDataDir;
}


bool OCPNPlatform::InitializeLogFile( void )
{
    //      Establish Log File location
    mlog_file = GetHomeDir();
    
#ifdef  __WXOSX__
    
    wxFileName LibPref(mlog_file);          // starts like "~/Library/Preferences"
    LibPref.RemoveLastDir();// takes off "Preferences"
    
    mlog_file = LibPref.GetFullPath();
    appendOSDirSlash(&mlog_file);
    
    mlog_file.Append(_T("Logs/"));// so, on OS X, opencpn.log ends up in ~/Library/Logs
                                   // which makes it accessible to Applications/Utilities/Console....
#endif
    
    // create the opencpn "home" directory if we need to
    wxFileName wxHomeFiledir( GetHomeDir() );
    if( true != wxHomeFiledir.DirExists( wxHomeFiledir.GetPath() ) )
        if( !wxHomeFiledir.Mkdir( wxHomeFiledir.GetPath() ) ) {
            wxASSERT_MSG(false,_T("Cannot create opencpn home directory"));
            return false;
    }
        
        // create the opencpn "log" directory if we need to
    wxFileName wxLogFiledir( mlog_file );
    if( true != wxLogFiledir.DirExists( wxLogFiledir.GetPath() ) ) {
        if( !wxLogFiledir.Mkdir( wxLogFiledir.GetPath() ) ) {
            wxASSERT_MSG(false,_T("Cannot create opencpn log directory"));
            return false;
        }
    }
    
    mlog_file.Append( _T("opencpn.log") );
    wxString logit = mlog_file;
        
        //  Constrain the size of the log file
    if( ::wxFileExists( mlog_file ) ) {
            if( wxFileName::GetSize( mlog_file ) > 1000000 ) {
                wxString oldlog = mlog_file;
                oldlog.Append( _T(".log") );
                //  Defer the showing of this messagebox until the system locale is established.
                large_log_message = ( _("Old log will be moved to opencpn.log.log") );
                ::wxRenameFile( mlog_file, oldlog );
            }
    }
        
#ifdef __OCPN__ANDROID__
        //  Force new logfile for each instance
        // TODO Remove this behaviour on Release
    if( ::wxFileExists( mlog_file ) ){
        ::wxRemoveFile( mlog_file );
    }
#endif
        
    flog = fopen( mlog_file.mb_str(), "a" );
    g_logger = new wxLogStderr( flog );
        
#ifdef __OCPN__ANDROID__
        //  Trouble printing timestamp
    logger->SetTimestamp((const char *)NULL);
#endif
        
#if defined(__WXGTK__) || defined(__WXOSX__)
    g_logger->SetTimestamp(_T("%H:%M:%S %Z"));
#endif
        
    m_Oldlogger = wxLog::SetActiveTarget( g_logger );

    return true;
    
}

void OCPNPlatform::CloseLogFile( void)
{
    if( g_logger ) {
        wxLog::SetActiveTarget( m_Oldlogger );
        delete g_logger;
    }
}










//--------------------------------------------------------------------------
//      Internal GPS Support
//--------------------------------------------------------------------------

bool OCPNPlatform::hasInternalGPS(wxString profile)
{
    
#ifdef __OCPN__ANDROID__
    bool t = androidDeviceHasGPS();
    qDebug() << "androidDeviceHasGPS" << t;
    return t;
#else

    return false;
    
#endif    
}


//--------------------------------------------------------------------------
//      Internal Bluetooth Support
//--------------------------------------------------------------------------

bool OCPNPlatform::hasInternalBT(wxString profile)
{
    
    return false;
}
