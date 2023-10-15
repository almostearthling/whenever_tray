/// whenever_tray
///
/// Tray application to launch the *whenever* scheduler in the background on
/// graphical environments. The purpose of this application is to launch and/or
/// restart the scheduler (hiding the associated console on Windows) and to
/// provide information about the scheduler status.
///
/// Based on the *wxTaskBarIcon demo* by Julian Smart for wxWidgets.

// Define a new application
class WTApp : public wxApp {
public:
    virtual bool OnInit() wxOVERRIDE;
};

// Define the taskbar icon interface
class WheneverTrayIcon : public wxTaskBarIcon {
public:
#if defined(__WXOSX__) && wxOSX_USE_COCOA
    MyTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE)
        : wxTaskBarIcon(iconType)
#else
    WheneverTrayIcon()
#endif
    { }

    void OnMenuExit(wxCommandEvent&);
    void OnMenuPause(wxCommandEvent&);
    void OnMenuResume(wxCommandEvent&);
    void OnMenuResetConditions(wxCommandEvent&);
    void OnMenuShowLog(wxCommandEvent&);
    void OnMenuAbout(wxCommandEvent&);
    virtual wxMenu* CreatePopupMenu() wxOVERRIDE;

    wxDECLARE_EVENT_TABLE();
};

// forward declaration
class WTPipedProcess;

// Define a new frame type: this is going to be our main frame
class WTHiddenFrame : public wxFrame {
public:
    // ctor(s)
    WTHiddenFrame(const wxString& title);
    ~WTHiddenFrame();

    // interface to the underlying *whenever* process
    bool StartWheneverCommand(unsigned int priority);
    bool StopWheneverCommand();
    bool PauseWhenever();
    bool ResumeWhenever();
    bool ResetConditions();
    bool ShowWheneverLog();
    wxString GetWheneverVersion() {
        return m_cmdVersion.Clone();
    }

protected:
    // event handlers (these functions should _not_ be virtual)
    void OnExit(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    WheneverTrayIcon* m_taskBarIcon;

private:
    WTPipedProcess* m_process;

    long m_pid;
    wxString m_cmdLine;
    wxString m_cmdLineLogView;
    wxString m_cmdVersion;

    // any class wishing to process wxWidgets events must use this macro
    wxDECLARE_EVENT_TABLE();
};

// This is the handler for process termination events, specialized for
// output redirection and capture
class WTPipedProcess : public wxProcess {
public:
    WTPipedProcess(WTHiddenFrame* parent) : wxProcess(parent) {
        m_parent = parent;
        Redirect();
        m_bAlive = true;
    }
    bool Alive() {
        return m_bAlive;
    }

    virtual void OnTerminate(int pid, int status) wxOVERRIDE;

protected:
    WTHiddenFrame* m_parent;
    wxString m_cmd;
    bool m_bAlive;
};


// end.
