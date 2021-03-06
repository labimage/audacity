/**********************************************************************

  Audacity: A Digital Audio Editor

  Screenshot.cpp

  Dominic Mazzoni

*******************************************************************/

#include "Screenshot.h"
#include "MemoryX.h"
#include "commands/ScreenshotCommand.h"
#include "commands/CommandTargets.h"
#include "commands/CommandDirectory.h"
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/frame.h>

#include "ShuttleGui.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dirdlg.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include <wx/statusbr.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/tglbtn.h>
#include <wx/window.h>

#include "AudacityApp.h"
#include "Project.h"
#include "Prefs.h"
#include "toolbars/ToolManager.h"

#include "Track.h"

class CommandType;

////////////////////////////////////////////////////////////////////////////////

class ScreenFrame final : public wxFrame
{
 public:
   // constructors and destructors
   ScreenFrame(wxWindow *parent, wxWindowID id);
   virtual ~ScreenFrame();

   bool ProcessEvent(wxEvent & event) override;

 private:
   void Populate();
   void PopulateOrExchange(ShuttleGui &S);

   void OnCloseWindow(wxCloseEvent & event);
   void OnUIUpdate(wxUpdateUIEvent & event);
   void OnDirChoose(wxCommandEvent & event);

   void SizeMainWindow(int w, int h);
   void OnMainWindowSmall(wxCommandEvent & event);
   void OnMainWindowLarge(wxCommandEvent & event);
   void OnToggleBackgroundBlue(wxCommandEvent & event);
   void OnToggleBackgroundWhite(wxCommandEvent & event);

   void DoCapture(wxString captureMode);
   void OnCaptureSomething(wxCommandEvent & event);

   void TimeZoom(double seconds);
   void OnOneSec(wxCommandEvent & event);
   void OnTenSec(wxCommandEvent & event);
   void OnOneMin(wxCommandEvent & event);
   void OnFiveMin(wxCommandEvent & event);
   void OnOneHour(wxCommandEvent & event);

   void SizeTracks(int h);
   void OnShortTracks(wxCommandEvent & event);
   void OnMedTracks(wxCommandEvent & event);
   void OnTallTracks(wxCommandEvent & event);

   std::unique_ptr<ScreenshotCommand> CreateCommand();

   wxCheckBox *mDelayCheckBox;
   wxTextCtrl *mDirectoryTextBox;
   wxToggleButton *mBlue;
   wxToggleButton *mWhite;
   wxStatusBar *mStatus;

   std::unique_ptr<ScreenshotCommand> mCommand;
   CommandExecutionContext mContext;

   DECLARE_EVENT_TABLE()
};

using ScreenFramePtr = Destroy_ptr<ScreenFrame>;
ScreenFramePtr mFrame;

////////////////////////////////////////////////////////////////////////////////

void OpenScreenshotTools()
{
   if (!mFrame) {
      mFrame = ScreenFramePtr{ safenew ScreenFrame(wxGetApp().GetTopWindow(), -1) };
   }
   mFrame->Show();
   mFrame->Raise();
}

void CloseScreenshotTools()
{
   // The code below looks like a memory leak,
   // but actually wxWidgets will take care of deleting the
   // screenshot window, becuase the parent window is
   // being deleted.  So we only need to free up our pointer
   // to it, not actually delete the underlying object.
   if( mFrame )
      mFrame.release();
}

////////////////////////////////////////////////////////////////////////////////

class ScreenFrameTimer final : public wxTimer
{
 public:
   ScreenFrameTimer(ScreenFrame *frame,
                    wxEvent & event)
   {
      screenFrame = frame;
      evt.reset(event.Clone());
   }

   void Notify() override
   {
      // Process timer notification just once, then destroy self
      evt->SetEventObject(NULL);
      screenFrame->ProcessEvent(*evt);
      delete this;
   }

 private:
   ScreenFrame *screenFrame;
   std::unique_ptr<wxEvent> evt;
};

////////////////////////////////////////////////////////////////////////////////

enum
{
   IdMainWindowSmall = 19200,
   IdMainWindowLarge,

   IdDirectory,
   IdDirChoose,

   IdOneSec,
   IdTenSec,
   IdOneMin,
   IdFiveMin,
   IdOneHour,

   IdShortTracks,
   IdMedTracks,
   IdTallTracks,

   IdDelayCheckBox,

   IdCaptureFirst,
   IdCaptureWindowContents = IdCaptureFirst,
   IdCaptureFullWindow,
   IdCaptureWindowPlus,
   IdCaptureFullScreen,
  
   IdCaptureToolbars,
   IdCaptureMenus,
   IdCaptureEffects,
   IdCapturePreferences,
   IdCaptureSelectionBar,
   IdCaptureSpectralSelection,
   IdCaptureTools,
   IdCaptureTransport,
   IdCaptureMixer,
   IdCaptureMeter,
   IdCapturePlayMeter,
   IdCaptureRecordMeter,
   IdCaptureEdit,
   IdCaptureDevice,
   IdCaptureTranscription,
   IdCaptureScrub,

   IdCaptureTrackPanel,
   IdCaptureRuler,
   IdCaptureTracks,
   IdCaptureFirstTrack,
   IdCaptureSecondTrack,
   IdCaptureLast = IdCaptureSecondTrack,

   IdToggleBackgroundBlue,
   IdToggleBackgroundWhite,

   // Put all events that might need delay below:
   IdAllDelayedEvents,


   IdLastDelayedEvent,
};

BEGIN_EVENT_TABLE(ScreenFrame, wxFrame)
   EVT_CLOSE(ScreenFrame::OnCloseWindow)

   EVT_UPDATE_UI(IdCaptureFullScreen,   ScreenFrame::OnUIUpdate)

   EVT_BUTTON(IdMainWindowSmall,        ScreenFrame::OnMainWindowSmall)
   EVT_BUTTON(IdMainWindowLarge,        ScreenFrame::OnMainWindowLarge)
   EVT_TOGGLEBUTTON(IdToggleBackgroundBlue,   ScreenFrame::OnToggleBackgroundBlue)
   EVT_TOGGLEBUTTON(IdToggleBackgroundWhite,  ScreenFrame::OnToggleBackgroundWhite)
   EVT_COMMAND_RANGE(IdCaptureFirst, IdCaptureLast, wxEVT_COMMAND_BUTTON_CLICKED, ScreenFrame::OnCaptureSomething)

   EVT_BUTTON(IdOneSec,                 ScreenFrame::OnOneSec)
   EVT_BUTTON(IdTenSec,                 ScreenFrame::OnTenSec)
   EVT_BUTTON(IdOneMin,                 ScreenFrame::OnOneMin)
   EVT_BUTTON(IdFiveMin,                ScreenFrame::OnFiveMin)
   EVT_BUTTON(IdOneHour,                ScreenFrame::OnOneHour)

   EVT_BUTTON(IdShortTracks,            ScreenFrame::OnShortTracks)
   EVT_BUTTON(IdMedTracks,              ScreenFrame::OnMedTracks)
   EVT_BUTTON(IdTallTracks,             ScreenFrame::OnTallTracks)

   EVT_BUTTON(IdDirChoose,              ScreenFrame::OnDirChoose)
END_EVENT_TABLE();

// Must not be called before CreateStatusBar!
std::unique_ptr<ScreenshotCommand> ScreenFrame::CreateCommand()
{
   wxASSERT(mStatus != NULL);
   auto output =
      std::make_unique<CommandOutputTarget>(std::make_unique<NullProgressTarget>(),
                              std::make_shared<StatusBarTarget>(*mStatus),
                              std::make_shared<MessageBoxTarget>());
   CommandType *type = CommandDirectory::Get()->LookUp(wxT("Screenshot"));
   wxASSERT_MSG(type != NULL, wxT("Screenshot command doesn't exist!"));
   return std::make_unique<ScreenshotCommand>(*type, std::move(output), this);
}

ScreenFrame::ScreenFrame(wxWindow * parent, wxWindowID id)
:  wxFrame(parent, id, _("Screen Capture Frame"),
           wxDefaultPosition, wxDefaultSize,

#if !defined(__WXMSW__)

   #if !defined(__WXMAC__) // bug1358
           wxFRAME_TOOL_WINDOW |
   #endif

#else

           wxSTAY_ON_TOP|

#endif

           wxSYSTEM_MENU|wxCAPTION|wxCLOSE_BOX),
   mContext(&wxGetApp(), GetActiveProject())
{
   mDelayCheckBox = NULL;
   mDirectoryTextBox = NULL;

   mStatus = CreateStatusBar();
   mCommand = CreateCommand();

   Populate();

   // Reset the toolbars to a known state.
   // Note that the audio could be playing.
   // The monitoring will switch off temporarily
   // because we've switched monitor mid play.
   // Bug 383 - Resetting the toolbars is not wanted.
   // Any that are invisible will be amde visible as/when needed.
   //mContext.GetProject()->GetToolManager()->Reset();
   Center();
}

ScreenFrame::~ScreenFrame()
{
   if( mFrame )
      mFrame.release();
}

void ScreenFrame::Populate()
{
   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);
}

void ScreenFrame::PopulateOrExchange(ShuttleGui & S)
{
   wxPanel *p = S.StartPanel();
   {
      S.SetBorder(3);

      S.StartStatic(_("Choose location to save files"));
      {
         S.StartMultiColumn(3, wxEXPAND);
         {
            S.SetStretchyCol(1);

            wxString dir =
               gPrefs->Read(wxT("/ScreenshotPath"),
                            wxFileName::GetHomeDir());
            mDirectoryTextBox =
               S.Id(IdDirectory).AddTextBox(_("Save images to:"),
                                            dir, 30);
            S.Id(IdDirChoose).AddButton(_("Choose..."));
         }
         S.EndMultiColumn();
      }
      S.EndStatic();

      S.StartStatic(_("Capture entire window or screen"));
      {
         S.StartHorizontalLay();
         {
            S.Id(IdMainWindowSmall).AddButton(_("Resize Small"));
            S.Id(IdMainWindowLarge).AddButton(_("Resize Large"));
            /* i18n-hint: Bkgnd is short for background and appears on a small button
             * It is OK to just translate this item as if it said 'Blue' */
            wxASSERT(p); // To justify safenew
            mBlue = safenew wxToggleButton(p,
                                       IdToggleBackgroundBlue,
                                       _("Blue Bkgnd"));
            S.AddWindow(mBlue);
            /* i18n-hint: Bkgnd is short for background and appears on a small button
             * It is OK to just translate this item as if it said 'White' */
            mWhite = safenew wxToggleButton(p,
                                        IdToggleBackgroundWhite,
                                        _("White Bkgnd"));
            S.AddWindow(mWhite);
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureWindowContents).AddButton(_("Capture Window Only"));
            S.Id(IdCaptureFullWindow).AddButton(_("Capture Full Window"));
            S.Id(IdCaptureWindowPlus).AddButton(_("Capture Window Plus"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureFullScreen).AddButton(_("Capture Full Screen"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            mDelayCheckBox = S.Id(IdDelayCheckBox).AddCheckBox
               (_("Wait 5 seconds and capture frontmost window/dialog"),
                _("false"));
         }
         S.EndHorizontalLay();
      }
      S.EndStatic();

      S.StartStatic(_("Capture part of a project window"));
      {
         S.StartHorizontalLay();
         {
            S.Id(IdCaptureToolbars).AddButton(_("All Toolbars"));
#ifdef EXPERIMENTAL_DOCS_AUTOMATION
            S.Id(IdCaptureMenus).AddButton(_("All Menus"));
            S.Id(IdCaptureEffects).AddButton(_("All Effects"));
            S.Id(IdCapturePreferences).AddButton(_("All Preferences"));
#endif
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureSelectionBar).AddButton(_("SelectionBar"));
            S.Id(IdCaptureSpectralSelection).AddButton(_("Spectral Selection"));
            S.Id(IdCaptureTools).AddButton(_("Tools"));
            S.Id(IdCaptureTransport).AddButton(_("Transport"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureMixer).AddButton(_("Mixer"));
            S.Id(IdCaptureMeter).AddButton(_("Meter"));
            S.Id(IdCapturePlayMeter).AddButton(_("Play Meter"));
            S.Id(IdCaptureRecordMeter).AddButton(_("Record Meter"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureEdit).AddButton(_("Edit"));
            S.Id(IdCaptureDevice).AddButton(_("Device"));
            S.Id(IdCaptureTranscription).AddButton(_("Transcription"));
            S.Id(IdCaptureScrub).AddButton(_("Scrub"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureTrackPanel).AddButton(_("Track Panel"));
            S.Id(IdCaptureRuler).AddButton(_("Ruler"));
            S.Id(IdCaptureTracks).AddButton(_("Tracks"));
            S.Id(IdCaptureFirstTrack).AddButton(_("First Track"));
            S.Id(IdCaptureSecondTrack).AddButton(_("Second Track"));
         }
         S.EndHorizontalLay();
      }
      S.EndStatic();

      S.StartStatic(_("Scale"));
      {
         S.StartHorizontalLay();
         {
            S.Id(IdOneSec).AddButton(_("One Sec"));
            S.Id(IdTenSec).AddButton(_("Ten Sec"));
            S.Id(IdOneMin).AddButton(_("One Min"));
            S.Id(IdFiveMin).AddButton(_("Five Min"));
            S.Id(IdOneHour).AddButton(_("One Hour"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdShortTracks).AddButton(_("Short Tracks"));
            S.Id(IdMedTracks).AddButton(_("Medium Tracks"));
            S.Id(IdTallTracks).AddButton(_("Tall Tracks"));
         }
         S.EndHorizontalLay();
      }
      S.EndStatic();
   }
   S.EndPanel();

   Layout();
   GetSizer()->Fit(this);
   SetMinSize(GetSize());

   int top = 0;
#ifdef __WXMAC__
   // Allow for Mac menu bar
   top += 20;
#endif

   int width, height;
   GetSize(&width, &height);
   int displayWidth, displayHeight;
   wxDisplaySize(&displayWidth, &displayHeight);

   if (width > 100) {
      Move(displayWidth - width - 16, top + 16);
   }
   else {
      CentreOnParent();
   }

   SetIcon(mContext.GetProject()->GetIcon());
}

bool ScreenFrame::ProcessEvent(wxEvent & e)
{
   int id = e.GetId();

   if (mDelayCheckBox &&
       mDelayCheckBox->GetValue() &&
       e.IsCommandEvent() &&
       e.GetEventType() == wxEVT_COMMAND_BUTTON_CLICKED &&
       id >= IdAllDelayedEvents && id <= IdLastDelayedEvent &&
       e.GetEventObject() != NULL) {
      // safenew because it's a one-shot that deletes itself
      ScreenFrameTimer *timer = safenew ScreenFrameTimer(this, e);
      timer->Start(5000, true);
      return true;
   }

   if (e.IsCommandEvent() && e.GetEventObject() == NULL) {
      e.SetEventObject(this);
   }
   return wxFrame::ProcessEvent(e);
}

void ScreenFrame::OnCloseWindow(wxCloseEvent &  WXUNUSED(event))
{
   if (this == mFrame.get())
      mFrame.release();
   Destroy();
}

void ScreenFrame::OnUIUpdate(wxUpdateUIEvent &  WXUNUSED(event))
{
#ifdef __WXMAC__
   wxTopLevelWindow *top = mCommand->GetFrontWindow(GetActiveProject());
   bool needupdate = false;
   bool enable = false;

   if ((!top || top->IsIconized()) && mDirectoryTextBox->IsEnabled()) {
      needupdate = true;
      enable = false;
   }
   else if ((top && !top->IsIconized()) && !mDirectoryTextBox->IsEnabled()) {
      needupdate = true;
      enable = true;
   }

   if (needupdate) {
      for (int i = IdMainWindowSmall; i < IdLastDelayedEvent; i++) {
         wxWindow *w = wxWindow::FindWindowById(i, this);
         if (w) {
            w->Enable(enable);
         }
      }
   }
#endif
}

void ScreenFrame::OnDirChoose(wxCommandEvent & WXUNUSED(event))
{
   wxString current = mDirectoryTextBox->GetValue();

   wxDirDialog dlog(this,
                    _("Choose a location to save screenshot images"),
                    current);

   dlog.ShowModal();
   if (dlog.GetPath() != wxT("")) {
      wxFileName tmpDirPath;
      tmpDirPath.AssignDir(dlog.GetPath());
      wxString path = tmpDirPath.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR);
      mDirectoryTextBox->SetValue(path);
      gPrefs->Write(wxT("/ScreenshotPath"), path);
      gPrefs->Flush();
   }
}

void ScreenFrame::OnToggleBackgroundBlue(wxCommandEvent & WXUNUSED(event))
{
   mWhite->SetValue(false);
   mCommand->SetParameter(wxT("Background"),
         mBlue->GetValue() ? wxT("Blue") : wxT("None"));
}

void ScreenFrame::OnToggleBackgroundWhite(wxCommandEvent & WXUNUSED(event))
{
   mBlue->SetValue(false);
   mCommand->SetParameter(wxT("Background"),
         mWhite->GetValue() ? wxT("White") : wxT("None"));
}

void ScreenFrame::SizeMainWindow(int w, int h)
{
   int top = 20;

   mContext.GetProject()->Maximize(false);
   mContext.GetProject()->SetSize(16, 16 + top, w, h);
   //Bug383 - Toolbar Resets not wanted.
   //mContext.GetProject()->GetToolManager()->Reset();
}

void ScreenFrame::OnMainWindowSmall(wxCommandEvent & WXUNUSED(event))
{
   SizeMainWindow(680, 450);
}

void ScreenFrame::OnMainWindowLarge(wxCommandEvent & WXUNUSED(event))
{
   SizeMainWindow(900, 600);
}

void ScreenFrame::DoCapture(wxString captureMode)
{
   Hide();
   mCommand->SetParameter(wxT("FilePath"), mDirectoryTextBox->GetValue());

   mCommand->SetParameter(wxT("CaptureMode"), captureMode);
   if (!mCommand->Apply(mContext))
      mStatus->SetStatusText(wxT("Capture failed!"), mainStatusBarField);
   Show();
}

void ScreenFrame::OnCaptureSomething(wxCommandEvent &  event)
{
   int i = event.GetId() - IdCaptureFirst;

   wxArrayString Names;

   Names.Add(wxT("window"));
   Names.Add(wxT("fullwindow"));
   Names.Add(wxT("windowplus"));
   Names.Add(wxT("fullscreen"));
   Names.Add(wxT("toolbars"));
   Names.Add(wxT("menus"));
   Names.Add(wxT("effects"));
   Names.Add(wxT("preferences"));
   Names.Add(wxT("selectionbar"));
   Names.Add(wxT("spectralselection"));
   Names.Add(wxT("tools"));
   Names.Add(wxT("transport"));
   Names.Add(wxT("mixer"));
   Names.Add(wxT("meter"));
   Names.Add(wxT("playmeter"));
   Names.Add(wxT("recordmeter"));
   Names.Add(wxT("edit"));
   Names.Add(wxT("device"));
   Names.Add(wxT("transcription"));
   Names.Add(wxT("scrub"));
   Names.Add(wxT("trackpanel"));
   Names.Add(wxT("ruler"));
   Names.Add(wxT("tracks"));
   Names.Add(wxT("firsttrack"));
   Names.Add(wxT("secondtrack"));

   DoCapture(Names[i]);
}

void ScreenFrame::TimeZoom(double seconds)
{
   int width, height;
   mContext.GetProject()->GetClientSize(&width, &height);
   mContext.GetProject()->mViewInfo.SetZoom((0.75 * width) / seconds);
   mContext.GetProject()->RedrawProject();
}

void ScreenFrame::OnOneSec(wxCommandEvent & WXUNUSED(event))
{
   TimeZoom(1.0);
}

void ScreenFrame::OnTenSec(wxCommandEvent & WXUNUSED(event))
{
   TimeZoom(10.0);
}

void ScreenFrame::OnOneMin(wxCommandEvent & WXUNUSED(event))
{
   TimeZoom(60.0);
}

void ScreenFrame::OnFiveMin(wxCommandEvent & WXUNUSED(event))
{
   TimeZoom(300.0);
}

void ScreenFrame::OnOneHour(wxCommandEvent & WXUNUSED(event))
{
   TimeZoom(3600.0);
}

void ScreenFrame::SizeTracks(int h)
{
   TrackListIterator iter(mContext.GetProject()->GetTracks());
   for (Track * t = iter.First(); t; t = iter.Next()) {
      if (t->GetKind() == Track::Wave) {
         if (t->GetLink()) {
            t->SetHeight(h);
         }
         else {
            t->SetHeight(h*2);
         }
      }
   }
   mContext.GetProject()->RedrawProject();
}

void ScreenFrame::OnShortTracks(wxCommandEvent & WXUNUSED(event))
{
   TrackListIterator iter(mContext.GetProject()->GetTracks());
   for (Track * t = iter.First(); t; t = iter.Next()) {
      if (t->GetKind() == Track::Wave) {
         t->SetHeight(t->GetMinimizedHeight());
      }
   }
   mContext.GetProject()->RedrawProject();
}

void ScreenFrame::OnMedTracks(wxCommandEvent & WXUNUSED(event))
{
   SizeTracks(60);
}

void ScreenFrame::OnTallTracks(wxCommandEvent & WXUNUSED(event))
{
   SizeTracks(85);
}
