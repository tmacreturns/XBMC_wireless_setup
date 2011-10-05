/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "system.h"
#include "GUIWindowFullScreen.h"
#include "Application.h"
#include "Util.h"
#ifdef HAS_VIDEO_PLAYBACK
#include "cores/VideoRenderers/RenderManager.h"
#endif
#include "utils/GUIInfoManager.h"
#include "GUIProgressControl.h"
#include "GUIAudioManager.h"
#include "GUILabelControl.h"
#include "GUIWindowOSD.h"
#include "GUIFontManager.h"
#include "GUITextLayout.h"
#include "GUIWindowManager.h"
#include "GUIDialogFullScreenInfo.h"
#include "GUIDialogAudioSubtitleSettings.h"
#include "GUIDialogNumeric.h"
#include "GUISliderControl.h"
#include "Settings.h"
#include "FileItem.h"
#include "VideoReferenceClock.h"
#include "AdvancedSettings.h"
#include "CPUInfo.h"
#include "GUISettings.h"
#include "LocalizeStrings.h"
#include "utils/SingleLock.h"
#include "utils/log.h"
#include "utils/TimeUtils.h"
#include "DateTime.h"
#include "ButtonTranslator.h"

#include <stdio.h>


#define BLUE_BAR                          0
#define LABEL_ROW1                       10
#define LABEL_ROW2                       11
#define LABEL_ROW3                       12

#define BTN_OSD_VIDEO                    13
#define BTN_OSD_AUDIO                    14
#define BTN_OSD_SUBTITLE                 15

#define MENU_ACTION_AVDELAY               1
#define MENU_ACTION_SEEK                  2
#define MENU_ACTION_SUBTITLEDELAY         3
#define MENU_ACTION_SUBTITLEONOFF         4
#define MENU_ACTION_SUBTITLELANGUAGE      5
#define MENU_ACTION_INTERLEAVED           6
#define MENU_ACTION_FRAMERATECONVERSIONS  7
#define MENU_ACTION_AUDIO_STREAM          8

#define MENU_ACTION_NEW_BOOKMARK          9
#define MENU_ACTION_NEXT_BOOKMARK        10
#define MENU_ACTION_CLEAR_BOOKMARK       11

#define MENU_ACTION_NOCACHE              12

#define IMG_PAUSE                        16
#define IMG_2X                           17
#define IMG_4X                           18
#define IMG_8X                           19
#define IMG_16X                          20
#define IMG_32X                          21

#define IMG_2Xr                         117
#define IMG_4Xr                         118
#define IMG_8Xr                         119
#define IMG_16Xr                        120
#define IMG_32Xr                        121

//Displays current position, visible after seek or when forced
//Alt, use conditional visibility Player.DisplayAfterSeek
#define LABEL_CURRENT_TIME               22

//Displays when video is rebuffering
//Alt, use conditional visibility Player.IsCaching
#define LABEL_BUFFERING                  24

//Progressbar used for buffering status and after seeking
#define CONTROL_PROGRESS                 23

#ifdef __APPLE__
static CLinuxResourceCounter m_resourceCounter;
#endif

static color_t color[8] = { 0xFFFFFF00, 0xFFFFFFFF, 0xFF0099FF, 0xFF00FF00, 0xFFCCFF00, 0xFF00FFFF, 0xFFE5E5E5, 0xFFC0C0C0 };

CGUIWindowFullScreen::CGUIWindowFullScreen(void)
    : CGUIWindow(WINDOW_FULLSCREEN_VIDEO, "VideoFullScreen.xml")
{
  m_timeCodeStamp[0] = 0;
  m_timeCodePosition = 0;
  m_timeCodeShow = false;
  m_timeCodeTimeout = 0;
  m_bShowViewModeInfo = false;
  m_dwShowViewModeTimeout = 0;
  m_bShowCurrentTime = false;
  m_subsLayout = NULL;
  m_sliderAction = 0;
  // audio
  //  - language
  //  - volume
  //  - stream

  // video
  //  - Create Bookmark (294)
  //  - Cycle bookmarks (295)
  //  - Clear bookmarks (296)
  //  - jump to specific time
  //  - slider
  //  - av delay

  // subtitles
  //  - delay
  //  - language

}

CGUIWindowFullScreen::~CGUIWindowFullScreen(void)
{}

void CGUIWindowFullScreen::AllocResources(bool forceLoad)
{
  CGUIWindow::AllocResources(forceLoad);
  DynamicResourceAlloc(false);
}

void CGUIWindowFullScreen::FreeResources(bool forceUnload)
{
  g_settings.Save();
  DynamicResourceAlloc(true);
  CGUIWindow::FreeResources(forceUnload);
}

bool CGUIWindowFullScreen::OnAction(const CAction &action)
{
  if (g_application.m_pPlayer != NULL && g_application.m_pPlayer->OnAction(action))
    return true;

  if (m_timeCodePosition > 0 && action.GetButtonCode())
  { // check whether we have a mapping in our virtual videotimeseek "window" and have a select action
    CKey key(action.GetButtonCode());
    CAction timeSeek = CButtonTranslator::GetInstance().GetAction(WINDOW_VIDEO_TIME_SEEK, key, false);
    if (timeSeek.GetID() == ACTION_SELECT_ITEM)
    {
      SeekToTimeCodeStamp(SEEK_ABSOLUTE);
      return true;
    }
  }

  const unsigned int MsgTime = 300;
  const unsigned int DisplTime = 2000;

  switch (action.GetID())
  {
  case ACTION_SHOW_OSD:
    ToggleOSD();
    return true;

  case ACTION_SHOW_GUI:
    {
      // switch back to the menu
      OutputDebugString("Switching to GUI\n");
      g_windowManager.PreviousWindow();
      OutputDebugString("Now in GUI\n");
      return true;
    }
    break;

  case ACTION_PLAYER_PLAY:
  case ACTION_PAUSE:
    if (m_timeCodePosition > 0)
    {
      SeekToTimeCodeStamp(SEEK_ABSOLUTE);
      return true;
    }
    break;

  case ACTION_STEP_BACK:
    if (m_timeCodePosition > 0)
      SeekToTimeCodeStamp(SEEK_RELATIVE, SEEK_BACKWARD);
    else
      g_application.m_pPlayer->Seek(false, false);
    return true;
    break;

  case ACTION_STEP_FORWARD:
    if (m_timeCodePosition > 0)
      SeekToTimeCodeStamp(SEEK_RELATIVE, SEEK_FORWARD);
    else
      g_application.m_pPlayer->Seek(true, false);
    return true;
    break;

  case ACTION_BIG_STEP_BACK:
    if (m_timeCodePosition > 0)
      SeekToTimeCodeStamp(SEEK_RELATIVE, SEEK_BACKWARD);
    else
      g_application.m_pPlayer->Seek(false, true);
    return true;
    break;

  case ACTION_BIG_STEP_FORWARD:
    if (m_timeCodePosition > 0)
      SeekToTimeCodeStamp(SEEK_RELATIVE, SEEK_FORWARD);
    else
      g_application.m_pPlayer->Seek(true, true);
    return true;
    break;

  case ACTION_NEXT_SCENE:
    if (g_application.m_pPlayer->SeekScene(true))
      g_infoManager.SetDisplayAfterSeek();
    return true;
    break;

  case ACTION_PREV_SCENE:
    if (g_application.m_pPlayer->SeekScene(false))
      g_infoManager.SetDisplayAfterSeek();
    return true;
    break;

  case ACTION_SHOW_OSD_TIME:
    m_bShowCurrentTime = !m_bShowCurrentTime;
    if(!m_bShowCurrentTime)
      g_infoManager.SetDisplayAfterSeek(0); //Force display off
    g_infoManager.SetShowTime(m_bShowCurrentTime);
    return true;
    break;

  case ACTION_SHOW_SUBTITLES:
    {
      g_settings.m_currentVideoSettings.m_SubtitleOn = !g_settings.m_currentVideoSettings.m_SubtitleOn;
      g_application.m_pPlayer->SetSubtitleVisible(g_settings.m_currentVideoSettings.m_SubtitleOn);
      CStdString sub;
      if (g_settings.m_currentVideoSettings.m_SubtitleOn)
        g_application.m_pPlayer->GetSubtitleName(g_application.m_pPlayer->GetSubtitle(),sub);
      else
        sub = g_localizeStrings.Get(1223);
      g_application.m_guiDialogKaiToast.QueueNotification(CGUIDialogKaiToast::Info,
                                                          g_localizeStrings.Get(287), sub, DisplTime, false, MsgTime);
    }
    return true;
    break;

  case ACTION_SHOW_INFO:
    {
      CGUIDialogFullScreenInfo* pDialog = (CGUIDialogFullScreenInfo*)g_windowManager.GetWindow(WINDOW_DIALOG_FULLSCREEN_INFO);
      if (pDialog)
      {
        pDialog->DoModal();
        return true;
      }
      break;
    }

  case ACTION_NEXT_SUBTITLE:
    {
      if (g_application.m_pPlayer->GetSubtitleCount() == 0)
        return true;

      if(g_settings.m_currentVideoSettings.m_SubtitleStream < 0)
        g_settings.m_currentVideoSettings.m_SubtitleStream = g_application.m_pPlayer->GetSubtitle();

      if (g_settings.m_currentVideoSettings.m_SubtitleOn)
      {
        g_settings.m_currentVideoSettings.m_SubtitleStream++;
        if (g_settings.m_currentVideoSettings.m_SubtitleStream >= g_application.m_pPlayer->GetSubtitleCount())
        {
          g_settings.m_currentVideoSettings.m_SubtitleStream = 0;
          g_settings.m_currentVideoSettings.m_SubtitleOn = false;
          g_application.m_pPlayer->SetSubtitleVisible(false);
        }
        g_application.m_pPlayer->SetSubtitle(g_settings.m_currentVideoSettings.m_SubtitleStream);
      }
      else
      {
        g_settings.m_currentVideoSettings.m_SubtitleOn = true;
        g_application.m_pPlayer->SetSubtitleVisible(true);
      }

      CStdString sub;
      if (g_settings.m_currentVideoSettings.m_SubtitleOn)
        g_application.m_pPlayer->GetSubtitleName(g_settings.m_currentVideoSettings.m_SubtitleStream,sub);
      else
        sub = g_localizeStrings.Get(1223);
      g_application.m_guiDialogKaiToast.QueueNotification(CGUIDialogKaiToast::Info, g_localizeStrings.Get(287), sub, DisplTime, false, MsgTime);
    }
    return true;
    break;

  case ACTION_SUBTITLE_DELAY_MIN:
    g_settings.m_currentVideoSettings.m_SubtitleDelay -= 0.1f;
    if (g_settings.m_currentVideoSettings.m_SubtitleDelay < -g_advancedSettings.m_videoSubsDelayRange)
      g_settings.m_currentVideoSettings.m_SubtitleDelay = -g_advancedSettings.m_videoSubsDelayRange;
    if (g_application.m_pPlayer)
      g_application.m_pPlayer->SetSubTitleDelay(g_settings.m_currentVideoSettings.m_SubtitleDelay);

    ShowSlider(action.GetID(), 22006, g_settings.m_currentVideoSettings.m_SubtitleDelay,
                                      -g_advancedSettings.m_videoSubsDelayRange, 0.1f,
                                       g_advancedSettings.m_videoSubsDelayRange);
    return true;
    break;
  case ACTION_SUBTITLE_DELAY_PLUS:
    g_settings.m_currentVideoSettings.m_SubtitleDelay += 0.1f;
    if (g_settings.m_currentVideoSettings.m_SubtitleDelay > g_advancedSettings.m_videoSubsDelayRange)
      g_settings.m_currentVideoSettings.m_SubtitleDelay = g_advancedSettings.m_videoSubsDelayRange;
    if (g_application.m_pPlayer)
      g_application.m_pPlayer->SetSubTitleDelay(g_settings.m_currentVideoSettings.m_SubtitleDelay);

    ShowSlider(action.GetID(), 22006, g_settings.m_currentVideoSettings.m_SubtitleDelay,
                                      -g_advancedSettings.m_videoSubsDelayRange, 0.1f,
                                       g_advancedSettings.m_videoSubsDelayRange);
    return true;
    break;
  case ACTION_SUBTITLE_DELAY:
    ShowSlider(action.GetID(), 22006, g_settings.m_currentVideoSettings.m_SubtitleDelay,
                                      -g_advancedSettings.m_videoSubsDelayRange, 0.1f,
                                       g_advancedSettings.m_videoSubsDelayRange, true);
    return true;
    break;
  case ACTION_AUDIO_DELAY:
    ShowSlider(action.GetID(), 297, g_settings.m_currentVideoSettings.m_AudioDelay,
                                    -g_advancedSettings.m_videoAudioDelayRange, 0.025f,
                                     g_advancedSettings.m_videoAudioDelayRange, true);
    return true;
    break;
  case ACTION_AUDIO_DELAY_MIN:
    g_settings.m_currentVideoSettings.m_AudioDelay -= 0.025f;
    if (g_settings.m_currentVideoSettings.m_AudioDelay < -g_advancedSettings.m_videoAudioDelayRange)
      g_settings.m_currentVideoSettings.m_AudioDelay = -g_advancedSettings.m_videoAudioDelayRange;
    if (g_application.m_pPlayer)
      g_application.m_pPlayer->SetAVDelay(g_settings.m_currentVideoSettings.m_AudioDelay);

    ShowSlider(action.GetID(), 297, g_settings.m_currentVideoSettings.m_AudioDelay,
                                    -g_advancedSettings.m_videoAudioDelayRange, 0.025f,
                                     g_advancedSettings.m_videoAudioDelayRange);
    return true;
    break;
  case ACTION_AUDIO_DELAY_PLUS:
    g_settings.m_currentVideoSettings.m_AudioDelay += 0.025f;
    if (g_settings.m_currentVideoSettings.m_AudioDelay > g_advancedSettings.m_videoAudioDelayRange)
      g_settings.m_currentVideoSettings.m_AudioDelay = g_advancedSettings.m_videoAudioDelayRange;
    if (g_application.m_pPlayer)
      g_application.m_pPlayer->SetAVDelay(g_settings.m_currentVideoSettings.m_AudioDelay);

    ShowSlider(action.GetID(), 297, g_settings.m_currentVideoSettings.m_AudioDelay,
                                    -g_advancedSettings.m_videoAudioDelayRange, 0.025f,
                                     g_advancedSettings.m_videoAudioDelayRange);
    return true;
    break;
  case ACTION_AUDIO_NEXT_LANGUAGE:
    {
      if (g_application.m_pPlayer->GetAudioStreamCount() == 1)
        return true;

      g_settings.m_currentVideoSettings.m_AudioStream++;
      if (g_settings.m_currentVideoSettings.m_AudioStream >= g_application.m_pPlayer->GetAudioStreamCount())
        g_settings.m_currentVideoSettings.m_AudioStream = 0;
      g_application.m_pPlayer->SetAudioStream(g_settings.m_currentVideoSettings.m_AudioStream);    // Set the audio stream to the one selected
      CStdString aud;
      g_application.m_pPlayer->GetAudioStreamName(g_settings.m_currentVideoSettings.m_AudioStream,aud);
      g_application.m_guiDialogKaiToast.QueueNotification(CGUIDialogKaiToast::Info, g_localizeStrings.Get(460), aud, DisplTime, false, MsgTime);
      return true;
    }
    break;
  case REMOTE_0:
  case REMOTE_1:
  case REMOTE_2:
  case REMOTE_3:
  case REMOTE_4:
  case REMOTE_5:
  case REMOTE_6:
  case REMOTE_7:
  case REMOTE_8:
  case REMOTE_9:
    {
      if (g_application.CurrentFileItem().IsLiveTV())
      {
        int channelNr = -1;

        CStdString strChannel;
        strChannel.Format("%i", action.GetID() - REMOTE_0);
        if (CGUIDialogNumeric::ShowAndGetNumber(strChannel, g_localizeStrings.Get(19000)))
          channelNr = atoi(strChannel.c_str());

        if (channelNr > 0)
          OnAction(CAction(ACTION_CHANNEL_SWITCH, (float)channelNr));
      }
      else
      {
        ChangetheTimeCode(action.GetID());
      }
      return true;
    }
    break;

  case ACTION_ASPECT_RATIO:
    { // toggle the aspect ratio mode (only if the info is onscreen)
      if (m_bShowViewModeInfo)
      {
#ifdef HAS_VIDEO_PLAYBACK
        g_renderManager.SetViewMode(++g_settings.m_currentVideoSettings.m_ViewMode);
#endif
      }
      m_bShowViewModeInfo = true;
      m_dwShowViewModeTimeout = CTimeUtils::GetTimeMS();
    }
    return true;
    break;
  case ACTION_SMALL_STEP_BACK:
    if (m_timeCodePosition > 0)
      SeekToTimeCodeStamp(SEEK_RELATIVE, SEEK_BACKWARD);
    else
    {
      int orgpos = (int)g_application.GetTime();
      int jumpsize = g_advancedSettings.m_videoSmallStepBackSeconds; // secs
      int setpos = (orgpos > jumpsize) ? orgpos - jumpsize : 0;
      g_application.SeekTime((double)setpos);
    }
    return true;
    break;
  case ACTION_ZOOM_IN:
    {
      g_settings.m_currentVideoSettings.m_CustomZoomAmount += 0.01f;
      if (g_settings.m_currentVideoSettings.m_CustomZoomAmount > 2.f)
        g_settings.m_currentVideoSettings.m_CustomZoomAmount = 2.f;
      g_settings.m_currentVideoSettings.m_ViewMode = VIEW_MODE_CUSTOM;
      g_renderManager.SetViewMode(VIEW_MODE_CUSTOM);
      ShowSlider(action.GetID(), 216, g_settings.m_currentVideoSettings.m_CustomZoomAmount, 0.5f, 0.1f, 2.0f);
    }
    return true;
    break;
  case ACTION_ZOOM_OUT:
    {
      g_settings.m_currentVideoSettings.m_CustomZoomAmount -= 0.01f;
      if (g_settings.m_currentVideoSettings.m_CustomZoomAmount < 0.5f)
        g_settings.m_currentVideoSettings.m_CustomZoomAmount = 0.5f;
      g_settings.m_currentVideoSettings.m_ViewMode = VIEW_MODE_CUSTOM;
      g_renderManager.SetViewMode(VIEW_MODE_CUSTOM);
      ShowSlider(action.GetID(), 216, g_settings.m_currentVideoSettings.m_CustomZoomAmount, 0.5f, 0.1f, 2.0f);
    }
    return true;
    break;
  case ACTION_INCREASE_PAR:
    {
      g_settings.m_currentVideoSettings.m_CustomPixelRatio += 0.01f;
      if (g_settings.m_currentVideoSettings.m_CustomPixelRatio > 2.f)
        g_settings.m_currentVideoSettings.m_CustomZoomAmount = 2.f;
      g_settings.m_currentVideoSettings.m_ViewMode = VIEW_MODE_CUSTOM;
      g_renderManager.SetViewMode(VIEW_MODE_CUSTOM);
      ShowSlider(action.GetID(), 217, g_settings.m_currentVideoSettings.m_CustomPixelRatio, 0.5f, 0.1f, 2.0f);
    }
    return true;
    break;
  case ACTION_DECREASE_PAR:
    {
      g_settings.m_currentVideoSettings.m_CustomPixelRatio -= 0.01f;
      if (g_settings.m_currentVideoSettings.m_CustomZoomAmount < 0.5f)
        g_settings.m_currentVideoSettings.m_CustomPixelRatio = 0.5f;
      g_settings.m_currentVideoSettings.m_ViewMode = VIEW_MODE_CUSTOM;
      g_renderManager.SetViewMode(VIEW_MODE_CUSTOM);
      ShowSlider(action.GetID(), 217, g_settings.m_currentVideoSettings.m_CustomPixelRatio, 0.5f, 0.1f, 2.0f);
    }
    return true;
    break;
  default:
      break;
  }
  return CGUIWindow::OnAction(action);
}

void CGUIWindowFullScreen::OnWindowLoaded()
{
  CGUIWindow::OnWindowLoaded();
  // override the clear colour - we must never clear fullscreen
  m_clearBackground = 0;

  CGUIProgressControl* pProgress = (CGUIProgressControl*)GetControl(CONTROL_PROGRESS);
  if(pProgress)
  {
    if( pProgress->GetInfo() == 0 || pProgress->GetVisibleCondition() == 0)
    {
      pProgress->SetInfo(PLAYER_PROGRESS);
      pProgress->SetVisibleCondition(PLAYER_DISPLAY_AFTER_SEEK, false);
      pProgress->SetVisible(true);
    }
  }

  CGUILabelControl* pLabel = (CGUILabelControl*)GetControl(LABEL_BUFFERING);
  if(pLabel && pLabel->GetVisibleCondition() == 0)
  {
    pLabel->SetVisibleCondition(PLAYER_CACHING, false);
    pLabel->SetVisible(true);
  }

  pLabel = (CGUILabelControl*)GetControl(LABEL_CURRENT_TIME);
  if(pLabel && pLabel->GetVisibleCondition() == 0)
  {
    pLabel->SetVisibleCondition(PLAYER_DISPLAY_AFTER_SEEK, false);
    pLabel->SetVisible(true);
    pLabel->SetLabel("$INFO(VIDEOPLAYER.TIME) / $INFO(VIDEOPLAYER.DURATION)");
  }
}

bool CGUIWindowFullScreen::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
  case GUI_MSG_WINDOW_INIT:
    {
      // check whether we've come back here from a window during which time we've actually
      // stopped playing videos
      if (message.GetParam1() == WINDOW_INVALID && !g_application.IsPlayingVideo())
      { // why are we here if nothing is playing???
        g_windowManager.PreviousWindow();
        return true;
      }
      g_infoManager.SetShowInfo(false);
      g_infoManager.SetShowCodec(false);
      m_bShowCurrentTime = false;
      g_infoManager.SetDisplayAfterSeek(0); // Make sure display after seek is off.

      // switch resolution
      g_graphicsContext.SetFullScreenVideo(true);

#ifdef HAS_VIDEO_PLAYBACK
      // make sure renderer is uptospeed
      g_renderManager.Update(false);
#endif
      // now call the base class to load our windows
      CGUIWindow::OnMessage(message);

      m_bShowViewModeInfo = false;

      if (CUtil::IsUsingTTFSubtitles())
      {
        CSingleLock lock (m_fontLock);

        CStdString fontPath = "special://xbmc/media/Fonts/";
        fontPath += g_guiSettings.GetString("subtitles.font");

        // We scale based on PAL4x3 - this at least ensures all sizing is constant across resolutions.
        CGUIFont *subFont = g_fontManager.LoadTTF("__subtitle__", fontPath, color[g_guiSettings.GetInt("subtitles.color")], 0, g_guiSettings.GetInt("subtitles.height"), g_guiSettings.GetInt("subtitles.style"), false, 1.0f, 1.0f, RES_PAL_4x3, true);
        CGUIFont *borderFont = g_fontManager.LoadTTF("__subtitleborder__", fontPath, 0xFF000000, 0, g_guiSettings.GetInt("subtitles.height"), g_guiSettings.GetInt("subtitles.style"), true, 1.0f, 1.0f, RES_PAL_4x3, true);
        if (!subFont || !borderFont)
          CLog::Log(LOGERROR, "CGUIWindowFullScreen::OnMessage(WINDOW_INIT) - Unable to load subtitle font");
        else
          m_subsLayout = new CGUITextLayout(subFont, true, 0, borderFont);
      }
      else
        m_subsLayout = NULL;

      return true;
    }
  case GUI_MSG_WINDOW_DEINIT:
    {
      CGUIWindow::OnMessage(message);

      CGUIDialog *pDialog = (CGUIDialog *)g_windowManager.GetWindow(WINDOW_DIALOG_OSD_TELETEXT);
      if (pDialog) pDialog->Close(true);
      CGUIDialogSlider *slider = (CGUIDialogSlider *)g_windowManager.GetWindow(WINDOW_DIALOG_SLIDER);
      if (slider) slider->Close(true);
      pDialog = (CGUIDialog *)g_windowManager.GetWindow(WINDOW_OSD);
      if (pDialog) pDialog->Close(true);
      pDialog = (CGUIDialog *)g_windowManager.GetWindow(WINDOW_DIALOG_FULLSCREEN_INFO);
      if (pDialog) pDialog->Close(true);

      FreeResources(true);

      CSingleLock lock (g_graphicsContext);
      g_graphicsContext.SetFullScreenVideo(false);
      lock.Leave();

#ifdef HAS_VIDEO_PLAYBACK
      // make sure renderer is uptospeed
      g_renderManager.Update(false);
#endif

      CSingleLock lockFont(m_fontLock);
      if (m_subsLayout)
      {
        g_fontManager.Unload("__subtitle__");
        g_fontManager.Unload("__subtitleborder__");
        delete m_subsLayout;
        m_subsLayout = NULL;
      }

      return true;
    }
  case GUI_MSG_SETFOCUS:
  case GUI_MSG_LOSTFOCUS:
    if (message.GetSenderId() != WINDOW_FULLSCREEN_VIDEO) return true;
    break;
  }

  return CGUIWindow::OnMessage(message);
}

EVENT_RESULT CGUIWindowFullScreen::OnMouseEvent(const CPoint &point, const CMouseEvent &event)
{
  if (event.m_id == ACTION_MOUSE_RIGHT_CLICK)
  { // no control found to absorb this click - go back to GUI
    OnAction(CAction(ACTION_SHOW_GUI));
    return EVENT_RESULT_HANDLED;
  }
  if (event.m_id == ACTION_MOUSE_WHEEL_UP)
  {
    return g_application.OnAction(CAction(ACTION_ANALOG_SEEK_FORWARD, 0.5f)) ? EVENT_RESULT_HANDLED : EVENT_RESULT_UNHANDLED;
  }
  if (event.m_id == ACTION_MOUSE_WHEEL_DOWN)
  {
    return g_application.OnAction(CAction(ACTION_ANALOG_SEEK_BACK, 0.5f)) ? EVENT_RESULT_HANDLED : EVENT_RESULT_UNHANDLED;
  }
  if (event.m_id != ACTION_MOUSE_MOVE || event.m_offsetX || event.m_offsetY)
  { // some other mouse action has occurred - bring up the OSD
    CGUIWindowOSD *pOSD = (CGUIWindowOSD *)g_windowManager.GetWindow(WINDOW_OSD);
    if (pOSD)
    {
      pOSD->SetAutoClose(3000);
      pOSD->DoModal();
    }
    return EVENT_RESULT_HANDLED;
  }
  return EVENT_RESULT_UNHANDLED;
}

void CGUIWindowFullScreen::FrameMove()
{
  if (g_application.GetPlaySpeed() != 1)
    g_infoManager.SetDisplayAfterSeek();
  if (m_bShowCurrentTime)
    g_infoManager.SetDisplayAfterSeek();

  if (!g_application.m_pPlayer) return;

  if( g_application.m_pPlayer->IsCaching() )
  {
    g_infoManager.SetDisplayAfterSeek(0); //Make sure these stuff aren't visible now
  }

  //------------------------
  if (g_infoManager.GetBool(PLAYER_SHOWCODEC))
  {
    // show audio codec info
    CStdString strAudio, strVideo, strGeneral;
    g_application.m_pPlayer->GetAudioInfo(strAudio);
    {
      CGUIMessage msg(GUI_MSG_LABEL_SET, GetID(), LABEL_ROW1);
      msg.SetLabel(strAudio);
      OnMessage(msg);
    }
    // show video codec info
    g_application.m_pPlayer->GetVideoInfo(strVideo);
    {
      CGUIMessage msg(GUI_MSG_LABEL_SET, GetID(), LABEL_ROW2);
      msg.SetLabel(strVideo);
      OnMessage(msg);
    }
    // show general info
    g_application.m_pPlayer->GetGeneralInfo(strGeneral);
    {
      CStdString strGeneralFPS;
#ifdef __APPLE__
      // We show CPU usage for the entire process, as it's arguably more useful.
      double dCPU = m_resourceCounter.GetCPUUsage();
      CStdString strCores;
      strCores.Format("cpu:%.0f%%", dCPU);
#else
      CStdString strCores = g_cpuInfo.GetCoresUsageString();
#endif
      int    missedvblanks;
      int    refreshrate;
      double clockspeed;
      CStdString strClock;

      if (g_VideoReferenceClock.GetClockInfo(missedvblanks, clockspeed, refreshrate))
        strClock.Format("S( refresh:%i missed:%i speed:%+.3f%% %s )"
                       , refreshrate
                       , missedvblanks
                       , clockspeed - 100.0
                       , g_renderManager.GetVSyncState().c_str());

      strGeneralFPS.Format("%s\nW( fps:%02.2f %s ) %s"
                         , strGeneral.c_str()
                         , g_infoManager.GetFPS()
                         , strCores.c_str(), strClock.c_str() );

      CGUIMessage msg(GUI_MSG_LABEL_SET, GetID(), LABEL_ROW3);
      msg.SetLabel(strGeneralFPS);
      OnMessage(msg);
    }
  }
  //----------------------
  // ViewMode Information
  //----------------------
  if (m_bShowViewModeInfo && CTimeUtils::GetTimeMS() - m_dwShowViewModeTimeout > 2500)
  {
    m_bShowViewModeInfo = false;
  }
  if (m_bShowViewModeInfo)
  {
    {
      // get the "View Mode" string
      CStdString strTitle = g_localizeStrings.Get(629);
      CStdString strMode = g_localizeStrings.Get(630 + g_settings.m_currentVideoSettings.m_ViewMode);
      CStdString strInfo;
      strInfo.Format("%s : %s", strTitle.c_str(), strMode.c_str());
      CGUIMessage msg(GUI_MSG_LABEL_SET, GetID(), LABEL_ROW1);
      msg.SetLabel(strInfo);
      OnMessage(msg);
    }
    // show sizing information
    CRect SrcRect, DestRect;
    float fAR;
    g_application.m_pPlayer->GetVideoRect(SrcRect, DestRect);
    g_application.m_pPlayer->GetVideoAspectRatio(fAR);
    {
      CStdString strSizing;
      strSizing.Format(g_localizeStrings.Get(245),
                       (int)SrcRect.Width(), (int)SrcRect.Height(),
                       (int)DestRect.Width(), (int)DestRect.Height(), g_settings.m_fZoomAmount, fAR*g_settings.m_fPixelRatio, g_settings.m_fPixelRatio);
      CGUIMessage msg(GUI_MSG_LABEL_SET, GetID(), LABEL_ROW2);
      msg.SetLabel(strSizing);
      OnMessage(msg);
    }
    // show resolution information
    int iResolution = g_graphicsContext.GetVideoResolution();
    {
      CStdString strStatus;
      if (g_settings.m_ResInfo[iResolution].bFullScreen)
        strStatus.Format("%s %ix%i@%.2fHz - %s",
          g_localizeStrings.Get(13287), g_settings.m_ResInfo[iResolution].iWidth,
          g_settings.m_ResInfo[iResolution].iHeight, g_settings.m_ResInfo[iResolution].fRefreshRate,
          g_localizeStrings.Get(244));
      else
        strStatus.Format("%s %ix%i - %s",
          g_localizeStrings.Get(13287), g_settings.m_ResInfo[iResolution].iWidth,
          g_settings.m_ResInfo[iResolution].iHeight, g_localizeStrings.Get(242));

      CGUIMessage msg(GUI_MSG_LABEL_SET, GetID(), LABEL_ROW3);
      msg.SetLabel(strStatus);
      OnMessage(msg);
    }
  }

  if (m_timeCodeShow && m_timeCodePosition != 0)
  {
    if ( (CTimeUtils::GetTimeMS() - m_timeCodeTimeout) >= 2500)
    {
      m_timeCodeShow = false;
      m_timeCodePosition = 0;
    }
    CStdString strDispTime = "00:00:00";

    CGUIMessage msg(GUI_MSG_LABEL_SET, GetID(), LABEL_ROW1);

    for (int pos = 7, i = m_timeCodePosition; pos >= 0 && i > 0; pos--)
    {
      if (strDispTime[pos] != ':')
      {
        i -= 1;
        strDispTime[pos] = (char)m_timeCodeStamp[i] + '0';
      }
    }

    strDispTime += "/" + g_infoManager.GetDuration(TIME_FORMAT_HH_MM_SS) + " [" + g_infoManager.GetCurrentPlayTime(TIME_FORMAT_HH_MM_SS) + "]"; // duration [ time ]
    msg.SetLabel(strDispTime);
    OnMessage(msg);
  }

  if (g_infoManager.GetBool(PLAYER_SHOWCODEC) || m_bShowViewModeInfo)
  {
    SET_CONTROL_VISIBLE(LABEL_ROW1);
    SET_CONTROL_VISIBLE(LABEL_ROW2);
    SET_CONTROL_VISIBLE(LABEL_ROW3);
    SET_CONTROL_VISIBLE(BLUE_BAR);
  }
  else if (m_timeCodeShow)
  {
    SET_CONTROL_VISIBLE(LABEL_ROW1);
    SET_CONTROL_HIDDEN(LABEL_ROW2);
    SET_CONTROL_HIDDEN(LABEL_ROW3);
    SET_CONTROL_VISIBLE(BLUE_BAR);
  }
  else
  {
    SET_CONTROL_HIDDEN(LABEL_ROW1);
    SET_CONTROL_HIDDEN(LABEL_ROW2);
    SET_CONTROL_HIDDEN(LABEL_ROW3);
    SET_CONTROL_HIDDEN(BLUE_BAR);
  }
}

void CGUIWindowFullScreen::Render()
{
  if (g_application.m_pPlayer)
    RenderTTFSubtitles();
  CGUIWindow::Render();
}

void CGUIWindowFullScreen::RenderTTFSubtitles()
{
  if ((g_application.GetCurrentPlayer() == EPC_MPLAYER || g_application.GetCurrentPlayer() == EPC_DVDPLAYER) &&
      CUtil::IsUsingTTFSubtitles() && (g_application.m_pPlayer->GetSubtitleVisible()))
  {
    CSingleLock lock (m_fontLock);

    if(!m_subsLayout)
      return;

    CStdString subtitleText = "How now brown cow";
    if (g_application.m_pPlayer->GetCurrentSubtitle(subtitleText))
    {
      // Remove HTML-like tags from the subtitles until
      subtitleText.Replace("\\r", "");
      subtitleText.Replace("\r", "");
      subtitleText.Replace("\\n", "[CR]");
      subtitleText.Replace("\n", "[CR]");
      subtitleText.Replace("<br>", "[CR]");
      subtitleText.Replace("\\N", "[CR]");
      subtitleText.Replace("<i>", "[I]");
      subtitleText.Replace("</i>", "[/I]");
      subtitleText.Replace("<b>", "[B]");
      subtitleText.Replace("</b>", "[/B]");
      subtitleText.Replace("<u>", "");
      subtitleText.Replace("<p>", "");
      subtitleText.Replace("<P>", "");
      subtitleText.Replace("&nbsp;", "");
      subtitleText.Replace("</u>", "");
      subtitleText.Replace("</i", "[/I]"); // handle tags which aren't closed properly (happens).
      subtitleText.Replace("</b", "[/B]");
      subtitleText.Replace("</u", "");

      RESOLUTION res = g_graphicsContext.GetVideoResolution();
      g_graphicsContext.SetRenderingResolution(res, false);

      float maxWidth = (float) g_settings.m_ResInfo[res].Overscan.right - g_settings.m_ResInfo[res].Overscan.left;
      m_subsLayout->Update(subtitleText, maxWidth * 0.9f, false, true); // true to force LTR reading order (most Hebrew subs are this format)

      float textWidth, textHeight;
      m_subsLayout->GetTextExtent(textWidth, textHeight);
      float x = maxWidth * 0.5f + g_settings.m_ResInfo[res].Overscan.left;
      float y = g_settings.m_ResInfo[res].iSubtitles - textHeight;

      m_subsLayout->RenderOutline(x, y, 0, 0xFF000000, XBFONT_CENTER_X, maxWidth);
    }
  }
}

void CGUIWindowFullScreen::ChangetheTimeCode(int remote)
{
  if (remote >= REMOTE_0 && remote <= REMOTE_9)
  {
    m_timeCodeShow = true;
    m_timeCodeTimeout = CTimeUtils::GetTimeMS();

    if (m_timeCodePosition < 6)
      m_timeCodeStamp[m_timeCodePosition++] = remote - REMOTE_0;
    else
    {
      // rotate around
      for (int i = 0; i < 5; i++)
        m_timeCodeStamp[i] = m_timeCodeStamp[i+1];
      m_timeCodeStamp[5] = remote - REMOTE_0;
    }
  }
}

void CGUIWindowFullScreen::SeekToTimeCodeStamp(SEEK_TYPE type, SEEK_DIRECTION direction)
{
  double total = GetTimeCodeStamp();
  if (type == SEEK_RELATIVE)
    total = g_application.GetTime() + (((direction == SEEK_FORWARD) ? 1 : -1) * total);

  if (total < g_application.GetTotalTime())
    g_application.SeekTime(total);

  m_timeCodePosition = 0;
  m_timeCodeShow = false;
}

double CGUIWindowFullScreen::GetTimeCodeStamp()
{
  // Convert the timestamp into an integer
  int tot = 0;
  for (int i = 0; i < m_timeCodePosition; i++)
    tot = tot * 10 + m_timeCodeStamp[i];

  // Interpret result as HHMMSS
  int s = tot % 100; tot /= 100;
  int m = tot % 100; tot /= 100;
  int h = tot % 100;
  return h * 3600 + m * 60 + s;
}

void CGUIWindowFullScreen::SeekChapter(int iChapter)
{
  g_application.m_pPlayer->SeekChapter(iChapter);

  // Make sure gui items are visible.
  g_infoManager.SetDisplayAfterSeek();
}

void CGUIWindowFullScreen::ShowSlider(int action, int label, float value, float min, float delta, float max, bool modal)
{
  m_sliderAction = action;
  if (modal)
    CGUIDialogSlider::ShowAndGetInput(g_localizeStrings.Get(label), value, min, delta, max, this);
  else
    CGUIDialogSlider::Display(label, value, min, delta, max, this);
}

void CGUIWindowFullScreen::OnSliderChange(void *data, CGUISliderControl *slider)
{
  if (!slider)
    return;

  if (m_sliderAction == ACTION_ZOOM_OUT || m_sliderAction == ACTION_ZOOM_IN ||
      m_sliderAction == ACTION_INCREASE_PAR || m_sliderAction == ACTION_DECREASE_PAR)
  {
    CStdString strValue;
    strValue.Format("%1.2f",slider->GetFloatValue());
    slider->SetTextValue(strValue);
  }
  else
    slider->SetTextValue(CGUIDialogAudioSubtitleSettings::FormatDelay(slider->GetFloatValue(), 0.025f));

  if (g_application.m_pPlayer)
  {
    if (m_sliderAction == ACTION_AUDIO_DELAY)
    {
      g_settings.m_currentVideoSettings.m_AudioDelay = slider->GetFloatValue();
      g_application.m_pPlayer->SetAVDelay(g_settings.m_currentVideoSettings.m_AudioDelay);
    }
    else if (m_sliderAction == ACTION_SUBTITLE_DELAY)
    {
      g_settings.m_currentVideoSettings.m_SubtitleDelay = slider->GetFloatValue();
      g_application.m_pPlayer->SetSubTitleDelay(g_settings.m_currentVideoSettings.m_SubtitleDelay);
    }
  }
}

void CGUIWindowFullScreen::ToggleOSD()
{
  CGUIWindowOSD *pOSD = (CGUIWindowOSD *)g_windowManager.GetWindow(WINDOW_OSD);
  if (pOSD)
  {
    if (pOSD->IsDialogRunning())
      pOSD->Close();
    else
      pOSD->DoModal();
  }
}
