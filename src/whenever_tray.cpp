/// whenever_tray
///
/// Tray application to launch the Whenever scheduler in the background in
/// graphical environments. The purpose of this application is to launch,
/// pause and resume the scheduler, and to hide the associated console on
/// Windows.
///
/// This application is provided as a bare minimum cross-platform interface
/// for Whenever in graphical environments, in order to avoid running the
/// scheduler in a console/terminal window, especially on Windows desktops.
///
/// Based on the *wxTaskBarIcon demo* by Julian Smart for wxWidgets.
///
/// - for the TOML interface: https://github.com/ToruNiina/toml11
///
/// Build system: using the wxWidgets Template
///
/// - https://github.com/lszl84/wx_cmake_template
///

// some helpers from the STL for use with TOML and to remain cross-platform
#include <string>
#include <map>
#include <chrono>
#include <thread>

// the TOML library: see above
#include "toml11/toml.hpp"

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/artprov.h"
#include "wx/taskbar.h"
#include "wx/process.h"
#include "wx/txtstrm.h"
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/aboutdlg.h>
#include <wx/arrstr.h>
#include <wx/bmpbndl.h>
#include <wx/gdicmn.h>

#include "whenever_tray.h"

#include "images/icon_svg.h"

// shortcut to show the main frame in debug sessions: must be false
#define DEBUG_SHOW_FRAME false

// definitions and constants
#define APP_NAME "WheneverTray"
#define APP_NAME_LONG "Minimalistic launcher for Whenever"
#define APP_DESCRIPTION "A minimalistic launcher to start/stop the Whenever scheduler\n"    \
                        "in a desktop environment, and to provide basic access to the\n"    \
                        "scheduler interface through an icon in the tray notification\n"    \
                        "area and its associated menu.\n\n"                                 \
                        "(running: %s)\n"
#define APP_VERSION "0.1.5"
#define APP_COPYRIGHT "(c) 2023"
#define APP_AUTHOR "Francesco Garosi"
#define APP_WEBSITE "https://github.com/almostearthling/"

#define APP_KILL_SLEEP 1500     // milliseconds
#define APP_START_SLEEP 500     // milliseconds

// default values for the scheduler executable filename on Windows and UNIX
#ifdef __WINDOWS__
const char* WHENEVER_COMMAND = "whenever.exe";
const char* LOGVIEW_DEFAULT_COMMAND = "notepad.exe";
#else
const char* WHENEVER_COMMAND = "whenever";
const char* LOGVIEW_DEFAULT_COMMAND = "gnome-text-editor";
#endif

// commands that can be passed to the scheduler
const char* WHENEVER_CMD_EXIT = "exit\n";
const char* WHENEVER_CMD_PAUSE = "pause\n";
const char* WHENEVER_CMD_RESUME = "resume\n";
const char* WHENEVER_CMD_RESETCONDS = "reset_conditions\n";

// configuration file name (to be found in the hidden user data directory)
const char* CONFIG_FILE = "whenever_tray.toml";

// default configuration for the underlying scheduler
const char* WHENEVER_CONFIG = "whenever.toml";
const char* WHENEVER_LOG = "whenever.log";
const char* WHENEVER_LOGLEVEL = "info";

// priorities
const unsigned int PRIORITY_NORMAL = wxPRIORITY_DEFAULT; // = 50
const unsigned int PRIORITY_MINIMUM = wxPRIORITY_MIN;    // = 0
const unsigned int PRIORITY_LOW = (wxPRIORITY_DEFAULT - wxPRIORITY_MIN) / 2;

// this is left as a definition so to spare some memory when not used
#define DEBUG_BOX(msg) wxMessageBox(msg, "DEBUG", wxOK | wxICON_INFORMATION)

// the SLEEP function is a macro to keep it simpler
#define SLEEP(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))

// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------

static WTHiddenFrame* hidden_frame = NULL;


// ============================================================================
// WTPipedProcess: implementation
// ============================================================================

void WTPipedProcess::OnTerminate(int pid, int status) {
    m_bAlive = false;
    wxProcess::OnTerminate(pid, status);
}


// ----------------------------------------------------------------------------
// WTApp: the application class
// ----------------------------------------------------------------------------

wxIMPLEMENT_APP(WTApp);

bool WTApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    if (!wxTaskBarIcon::IsAvailable()) {
        wxMessageBox(
            "No tray area support on OS: leaving.",
            "Error",
            wxOK | wxICON_EXCLAMATION);
        return false;
    }

    // create the main window
    hidden_frame = new WTHiddenFrame(APP_NAME);
    hidden_frame->Show(DEBUG_SHOW_FRAME);

    return true;
}


// ----------------------------------------------------------------------------
// WTHiddenFrame: the hidden application frame
// ----------------------------------------------------------------------------

// event table
wxBEGIN_EVENT_TABLE(WTHiddenFrame, wxFrame)
    EVT_BUTTON(wxID_EXIT, WTHiddenFrame::OnExit)
    EVT_CLOSE(WTHiddenFrame::OnCloseWindow)
wxEND_EVENT_TABLE()

WTHiddenFrame::WTHiddenFrame(const wxString& title) : wxFrame(NULL, wxID_ANY, title) {
    // build icons from the embedded SVG data
    wxBitmapBundle bmp_bundle =
        wxBitmapBundle::FromSVG(ICON_SVG, wxSize(32, 32));
    wxIcon tbicon = bmp_bundle.GetIcon(wxSize(16, 16));
    wxIcon frameicon = bmp_bundle.GetIcon(wxSize(32, 32));

    // initialize process reference members
    m_process = NULL;
    m_pid = 0;

    // set the frame icon
    SetIcon(frameicon);

    m_taskBarIcon = new WheneverTrayIcon();
    if (!m_taskBarIcon->SetIcon(tbicon, APP_NAME_LONG)) {
        wxMessageBox(
            "Could not set icon: exiting.",
            "Error",
            wxOK | wxICON_EXCLAMATION);
        Close(true);
    }

#if defined(__WXOSX__) && wxOSX_USE_COCOA
    m_dockIcon = new MyTaskBarIcon(wxTBI_DOCK);
    if (!m_dockIcon->SetIcon(tbicon, APP_NAME_LONG)) {
        wxMessageBox(
            "Could not set dock icon: exiting.",
            "Error",
            wxOK | wxICON_EXCLAMATION);
        Close(true);
    }
#endif

    // get the configuration
    wxStandardPaths paths = wxStandardPaths::Get();
    wxString data_dir = paths.GetUserDataDir();
    wxString cfgfile(data_dir + wxFileName::GetPathSeparator() + wxString(CONFIG_FILE));
    wxString command_path, config_path, log_path, log_level, logview_command_path;
    unsigned int priority = PRIORITY_MINIMUM;
    wxString t_cmdver;
    wxArrayString t_cmdout;

    try {
        auto res_conf = toml::parse(cfgfile.ToStdString());
        if (!res_conf.is_table() && !res_conf.contains("whenever_tray")) {
            command_path = wxString(WHENEVER_COMMAND);
            logview_command_path = wxString(LOGVIEW_DEFAULT_COMMAND);
            config_path = data_dir + wxFileName::GetPathSeparator() + wxString(WHENEVER_CONFIG);
            log_path = data_dir + wxFileName::GetPathSeparator() + wxString(WHENEVER_LOG);
            log_level = wxString(WHENEVER_LOGLEVEL);
        } else {
            // there might be better ways to extract the configuration, but
            // this is quick and does the job: anyway all the local variables
            // used during the process are thrown away after initialization
            auto conf = toml::find<std::map<std::string, std::string>>(res_conf, "whenever_tray");
            auto e1 = conf.find("whenever_command");
            if (e1 != conf.end()) {
                command_path = wxString(conf["whenever_command"]);
            } else {
                command_path = wxString(WHENEVER_COMMAND);
            }
            auto e2 = conf.find("whenever_config");
            if (e2 != conf.end()) {
                config_path = wxString(conf["whenever_config"]);
            } else {
                config_path = data_dir + wxFileName::GetPathSeparator() + wxString(WHENEVER_CONFIG);
            }
            auto e3 = conf.find("whenever_logfile");
            if (e3 != conf.end()) {
                log_path = wxString(conf["whenever_logfile"]);
            } else {
                log_path = data_dir + wxFileName::GetPathSeparator() + wxString(WHENEVER_LOG);
            }
            auto e4 = conf.find("whenever_loglevel");
            if (e4 != conf.end()) {
                log_level = wxString(conf["whenever_loglevel"]);
                wxString allowed = wxString("/error/warn/info/debug/trace/");
                if (allowed.find(wxString::Format("/%s/", log_level)) == wxNOT_FOUND)  {
                    log_level = wxString(WHENEVER_LOGLEVEL);
                }
            } else {
                log_level = wxString(WHENEVER_LOGLEVEL);
            }
            auto e5 = conf.find("whenever_priority");
            if (e5 != conf.end()) {
                auto s = wxString(conf["whenever_priority"]);
                wxString allowed = wxString("/normal/low/minimum/");
                if (allowed.find(wxString::Format("/%s/", s)) == wxNOT_FOUND) {
                    priority = PRIORITY_MINIMUM;
                } else {
                    if (s == "normal")
                        priority = PRIORITY_NORMAL;
                    else if (s == "low")
                        priority = PRIORITY_LOW;
                    else
                        priority = PRIORITY_MINIMUM;
                }
            }
            auto e6 = conf.find("logview_command");
            if (e6 != conf.end()) {
                logview_command_path = wxString(conf["logview_command"]);
            } else {
                logview_command_path = wxString(LOGVIEW_DEFAULT_COMMAND);
            }
        }
    }
    catch (...) {
        wxMessageBox(
            "Could not read/parse configuration file:\n"
            "please check for presence or errors.\n"
            "Default values will be used.",
            "Warning",
            wxOK | wxICON_EXCLAMATION);
        command_path = wxString(WHENEVER_COMMAND);
        logview_command_path = wxString(LOGVIEW_DEFAULT_COMMAND);
        config_path = data_dir + wxFileName::GetPathSeparator() + wxString(WHENEVER_CONFIG);
        log_path = data_dir + wxFileName::GetPathSeparator() + wxString(WHENEVER_LOG);
        log_level = wxString(WHENEVER_LOGLEVEL);
    }

    // retrieve the version of Whenever directly from the command line
    t_cmdver << "\"" << command_path << "\"" << wxString(" --version");
    wxExecute(t_cmdver, t_cmdout, wxEXEC_SYNC | wxEXEC_HIDE_CONSOLE);
    if (t_cmdout.IsEmpty()) {
        m_cmdVersion = "unknown version";
    } else {
        m_cmdVersion = t_cmdout[0];
    }

    // build a minimal command that logs where requested and start it
    m_cmdLine
        << "\"" << command_path << "\""
        << wxString(" -L ") << log_level << (" -l ")
        << "\"" << log_path << "\""
        << " "
        << "\"" << config_path << "\"";

    // save the command line for the log viewer command
    m_cmdLineLogView
        << "\"" << logview_command_path << "\"" << " "
        << "\"" << log_path << "\"";

    if (!StartWheneverCommand(priority)) {
        wxMessageBox(
            "Could not start scheduler process:\n"
            "please check configuration file.",
            "Error",
            wxOK | wxICON_EXCLAMATION);
        Close(true);
    }
}

/// Destructor: stop process, if any, then delete dynamic data. The exit
/// handlers defined below leave gracefully
WTHiddenFrame::~WTHiddenFrame() {
    StopWheneverCommand();
    delete m_taskBarIcon;
}

// See above
void WTHiddenFrame::OnExit(wxCommandEvent& WXUNUSED(event)) {
    Close(true);
}

// See above
void WTHiddenFrame::OnCloseWindow(wxCloseEvent& WXUNUSED(event)) {
    Destroy();
}

/// Interface to start the underlying command (same on Windows and UNIX)
bool WTHiddenFrame::StartWheneverCommand(unsigned int priority) {
    if (!m_process) {
        m_process = new WTPipedProcess(this);
    }
    // run the scheduler at selected priority
    m_process->SetPriority(priority);
    m_pid = wxExecute(
        m_cmdLine,
        wxEXEC_ASYNC | wxEXEC_HIDE_CONSOLE | wxEXEC_MAKE_GROUP_LEADER,
        m_process);
    SLEEP(APP_START_SLEEP);
    if (!m_process->Alive()) {
        m_pid = 0;
    }
    if (!m_pid) {
        return false;
    }
    return true;
}

/// Interface to stop the scheduler: uses the communication channel (stdin)
bool WTHiddenFrame::StopWheneverCommand() {
    if (m_pid && m_process->Exists(m_pid)) {
        // in most cases the command is expected to work and the scheduler
        // will exit cleanly in a short while (normally is a fraction of a
        // second, because *whenever* will try to react to commands at most
        // after 0.5 seconds), but in case something goes wrong this wrapper
        // will try to explicitly kill the scheduler: in this case `false`
        // is returned to remind that something has failed - although the
        // current implementation just ignores this return value
        wxOutputStream* appstdin = m_process->GetOutputStream();
        if (appstdin) {
            // shorten the command in order to remove the trailing zero
            appstdin->WriteAll(WHENEVER_CMD_EXIT, strlen(WHENEVER_CMD_EXIT));
            SLEEP(APP_KILL_SLEEP);
        }
        if (!m_process->Exists(m_pid)) {
            m_pid = 0;
            return true;
        } else {
            m_process->Kill(m_pid, wxSIGKILL, wxKILL_CHILDREN);
            SLEEP(APP_KILL_SLEEP);
            return false;
        }
    } else {
        return false;
    }
}

/// Interface to pause the scheduler: uses the communication channel (stdin)
bool WTHiddenFrame::PauseWhenever() {
    if (m_pid && m_process->Exists(m_pid)) {
        wxOutputStream* appstdin = m_process->GetOutputStream();
        if (appstdin) {
            // shorten the command in order to remove the trailing zero
            appstdin->WriteAll(WHENEVER_CMD_PAUSE, strlen(WHENEVER_CMD_PAUSE));
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

/// Interface to resume the scheduler: uses the communication channel (stdin)
bool WTHiddenFrame::ResumeWhenever() {
    if (m_pid && m_process->Exists(m_pid)) {
        wxOutputStream* appstdin = m_process->GetOutputStream();
        if (appstdin) {
            // shorten the command in order to remove the trailing zero
            appstdin->WriteAll(WHENEVER_CMD_RESUME, strlen(WHENEVER_CMD_RESUME));
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

/// Interface to reset conditions: uses the communication channel (stdin)
bool WTHiddenFrame::ResetConditions() {
    if (m_pid && m_process->Exists(m_pid)) {
        wxOutputStream* appstdin = m_process->GetOutputStream();
        if (appstdin) {
            // shorten the command in order to remove the trailing zero
            appstdin->WriteAll(WHENEVER_CMD_RESETCONDS, strlen(WHENEVER_CMD_RESETCONDS));
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

/// Interface to resume the scheduler: uses the communication channel (stdin)
bool WTHiddenFrame::ShowWheneverLog() {
    if (m_pid && m_process->Exists(m_pid)) {
        if (wxExecute(m_cmdLineLogView, wxEXEC_ASYNC) < 0) {
            return false;
        } else {
            return true;
        }
    } else {
        return false;
    }
}


// ----------------------------------------------------------------------------
// WheneverTrayIcon implementation
// ----------------------------------------------------------------------------

enum {
    PU_PAUSE = 10001,
    PU_RESUME,
    PU_RESET_CONDITIONS,
    PU_SHOW_LOG,
    PU_ABOUT,
    PU_EXIT,
};

wxBEGIN_EVENT_TABLE(WheneverTrayIcon, wxTaskBarIcon)
    EVT_MENU(PU_PAUSE, WheneverTrayIcon::OnMenuPause)
    EVT_MENU(PU_RESUME, WheneverTrayIcon::OnMenuResume)
    EVT_MENU(PU_RESET_CONDITIONS, WheneverTrayIcon::OnMenuResetConditions)
    EVT_MENU(PU_SHOW_LOG, WheneverTrayIcon::OnMenuShowLog)
    EVT_MENU(PU_EXIT, WheneverTrayIcon::OnMenuExit)
    EVT_MENU(PU_ABOUT, WheneverTrayIcon::OnMenuAbout)
wxEND_EVENT_TABLE()

/// Handle Menu: (Tray) -> &Pause
void WheneverTrayIcon::OnMenuPause(wxCommandEvent&) {
    hidden_frame->PauseWhenever();
}

/// Handle Menu: (Tray) -> Res&ume
void WheneverTrayIcon::OnMenuResume(wxCommandEvent&) {
    hidden_frame->ResumeWhenever();
}

/// Handle Menu: (Tray) -> Reset &Conditions
void WheneverTrayIcon::OnMenuResetConditions(wxCommandEvent&) {
    hidden_frame->ResetConditions();
}

/// Handle Menu: (Tray) -> Show &Log
void WheneverTrayIcon::OnMenuShowLog(wxCommandEvent&) {
    hidden_frame->ShowWheneverLog();
}

/// Handle Menu: (Tray) -> E&xit
void WheneverTrayIcon::OnMenuExit(wxCommandEvent&) {
    hidden_frame->StopWheneverCommand();
    hidden_frame->Close(true);
}

/// Handle Menu: (Tray) -> &About (build and show about box)
void WheneverTrayIcon::OnMenuAbout(wxCommandEvent&) {
    wxBitmapBundle bmp_bundle =
        wxBitmapBundle::FromSVG(ICON_SVG, wxSize(128, 128));

    wxIcon icon = bmp_bundle.GetIcon(wxSize(128, 128));
    wxString desc;

    desc.Printf(APP_DESCRIPTION, hidden_frame->GetWheneverVersion());

    wxAboutDialogInfo aboutInfo;
    aboutInfo.SetName(APP_NAME_LONG);
    aboutInfo.SetVersion(APP_VERSION);
    aboutInfo.SetDescription(desc);
    aboutInfo.SetCopyright(APP_COPYRIGHT);
    aboutInfo.SetWebSite(APP_WEBSITE);
    aboutInfo.AddDeveloper(APP_AUTHOR);
    aboutInfo.SetIcon(icon);

    wxAboutBox(aboutInfo);
}

/// Create the main popup menu that activates by right-clicking tray icon
wxMenu* WheneverTrayIcon::CreatePopupMenu() {
    wxMenu* menu = new wxMenu;
    menu->Append(PU_PAUSE, "&Pause Scheduler");
    menu->Append(PU_RESUME, "Res&ume Scheduler");
    menu->Append(PU_RESET_CONDITIONS, "Reset &Conditions");
    menu->Append(PU_SHOW_LOG, "Show &Log...");
    menu->AppendSeparator();
    menu->Append(PU_ABOUT, "&About...");
    /* OSX has built-in quit menu for the dock menu, but not for the status item */
#ifdef __WXOSX__
    if (OSXIsStatusItem())
#endif
    {
        menu->AppendSeparator();
        menu->Append(PU_EXIT, "E&xit");
    }
    return menu;
}


// end.
