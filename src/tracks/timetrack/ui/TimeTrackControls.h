/**********************************************************************

Audacity: A Digital Audio Editor

TimeTrackControls.h

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#ifndef __AUDACITY_TIME_TRACK_CONTROLS__
#define __AUDACITY_TIME_TRACK_CONTROLS__

#include "../../ui/TrackControls.h"

class TimeTrackControls final : public TrackControls
{
   TimeTrackControls(const TimeTrackControls&) = delete;
   TimeTrackControls &operator=(const TimeTrackControls&) = delete;

public:
   explicit
   TimeTrackControls( std::shared_ptr<Track> pTrack )
      : TrackControls( pTrack ) {}
   ~TimeTrackControls();

   HitTestResult HitTest
      (const TrackPanelMouseEvent &event,
       const AudacityProject *pProject) override;

   PopupMenuTable *GetMenuExtension(Track *pTrack) override;
};

#endif
