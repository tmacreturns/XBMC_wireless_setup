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
#include "GUIWindowSlideShow.h"
#include "Application.h"
#include "Picture.h"
#include "Util.h"
#include "URL.h"
#include "TextureManager.h"
#include "GUILabelControl.h"
#include "utils/GUIInfoManager.h"
#include "FileSystem/Directory.h"
#include "GUIDialogPictureInfo.h"
#include "GUIUserMessages.h"
#include "GUIWindowManager.h"
#include "Settings.h"
#include "GUISettings.h"
#include "FileItem.h"
#include "Texture.h"
#include "WindowingFactory.h"
#include "Texture.h"
#include "LocalizeStrings.h"
#include "utils/SingleLock.h"
#include "utils/log.h"
#include "utils/TimeUtils.h"

using namespace XFILE;

#define MAX_ZOOM_FACTOR                     10
#define MAX_PICTURE_SIZE             2048*2048

#define IMMEDIATE_TRANSISTION_TIME          20

#define PICTURE_MOVE_AMOUNT              0.02f
#define PICTURE_MOVE_AMOUNT_ANALOG       0.01f
#define PICTURE_VIEW_BOX_COLOR      0xffffff00 // YELLOW
#define PICTURE_VIEW_BOX_BACKGROUND 0xff000000 // BLACK

#define FPS                                 25

#define BAR_IMAGE                            1
#define LABEL_ROW1                          10
#define LABEL_ROW2                          11
#define LABEL_ROW2_EXTRA                    12
#define CONTROL_PAUSE                       13

static float zoomamount[10] = { 1.0f, 1.2f, 1.5f, 2.0f, 2.8f, 4.0f, 6.0f, 9.0f, 13.5f, 20.0f };

CBackgroundPicLoader::CBackgroundPicLoader()
{
  m_pCallback = NULL;
  m_loadPic = CreateEvent(NULL,false,false,NULL);
  m_isLoading = false;
}

CBackgroundPicLoader::~CBackgroundPicLoader()
{
  StopThread();
  CloseHandle(m_loadPic);
}

void CBackgroundPicLoader::Create(CGUIWindowSlideShow *pCallback)
{
  m_pCallback = pCallback;
  m_isLoading = false;
  CThread::Create(false);
}

void CBackgroundPicLoader::Process()
{
  unsigned int totalTime = 0;
  unsigned int count = 0;
  while (!m_bStop)
  { // loop around forever, waiting for the app to call LoadPic
    if (WaitForSingleObject(m_loadPic, 10) == WAIT_OBJECT_0)
    {
      if (m_pCallback)
      {
        unsigned int start = CTimeUtils::GetTimeMS();
        CBaseTexture* texture = new CTexture();
        unsigned int originalWidth = 0;
        unsigned int originalHeight = 0;
        texture->LoadFromFile(m_strFileName, m_maxWidth, m_maxHeight, g_guiSettings.GetBool("pictures.useexifrotation"), &originalWidth, &originalHeight);
        totalTime += CTimeUtils::GetTimeMS() - start;
        count++;
        // tell our parent
        bool bFullSize = ((int)texture->GetWidth() < m_maxWidth) && ((int)texture->GetHeight() < m_maxHeight);
        if (!bFullSize)
        {
          int iSize = texture->GetWidth() * texture->GetHeight() - MAX_PICTURE_SIZE;
          if ((iSize + (int)texture->GetWidth() > 0) || (iSize + (int)texture->GetHeight() > 0))
            bFullSize = true;
          if (!bFullSize && texture->GetWidth() == g_Windowing.GetMaxTextureSize())
            bFullSize = true;
          if (!bFullSize && texture->GetHeight() == g_Windowing.GetMaxTextureSize())
            bFullSize = true;
        }
        m_pCallback->OnLoadPic(m_iPic, m_iSlideNumber, texture, originalWidth, originalHeight, bFullSize);
        m_isLoading = false;
      }
    }
  }
  CLog::Log(LOGDEBUG, "Time for loading %u images: %u ms, average %u ms",
            count, totalTime, totalTime / count);
}

void CBackgroundPicLoader::LoadPic(int iPic, int iSlideNumber, const CStdString &strFileName, const int maxWidth, const int maxHeight)
{
  m_iPic = iPic;
  m_iSlideNumber = iSlideNumber;
  m_strFileName = strFileName;
  m_maxWidth = maxWidth;
  m_maxHeight = maxHeight;
  m_isLoading = true;
  SetEvent(m_loadPic);
}

CGUIWindowSlideShow::CGUIWindowSlideShow(void)
    : CGUIWindow(WINDOW_SLIDESHOW, "SlideShow.xml")
{
  m_pBackgroundLoader = NULL;
  m_slides = new CFileItemList;
  m_Resolution = RES_INVALID;
  Reset();
}

CGUIWindowSlideShow::~CGUIWindowSlideShow(void)
{
  Reset();
  delete m_slides;
}


bool CGUIWindowSlideShow::IsPlaying() const
{
  return m_Image[m_iCurrentPic].IsLoaded();
}

void CGUIWindowSlideShow::Reset()
{
  g_infoManager.SetShowCodec(false);
  m_bSlideShow = false;
  m_bPause = false;
  m_bErrorMessage = false;
  m_bReloadImage = false;
  m_bScreensaver = false;
  m_Image[0].UnLoad();

  m_iRotate = 0;
  m_iZoomFactor = 1;
  m_iCurrentSlide = 0;
  m_iNextSlide = 1;
  m_iCurrentPic = 0;
  CSingleLock lock(m_slideSection);
  m_slides->Clear();
  m_Resolution = g_graphicsContext.GetVideoResolution();
}

void CGUIWindowSlideShow::FreeResources()
{ // wait for any outstanding picture loads
  if (m_pBackgroundLoader)
  {
    // sleep until the loader finishes loading the current pic
    CLog::Log(LOGDEBUG,"Waiting for BackgroundLoader thread to close");
    while (m_pBackgroundLoader->IsLoading())
      Sleep(10);
    // stop the thread
    CLog::Log(LOGDEBUG,"Stopping BackgroundLoader thread");
    m_pBackgroundLoader->StopThread();
    delete m_pBackgroundLoader;
    m_pBackgroundLoader = NULL;
  }
  // and close the images.
  m_Image[0].Close();
  m_Image[1].Close();
  g_infoManager.ResetCurrentSlide();
}

void CGUIWindowSlideShow::Add(const CFileItem *picture)
{
  CFileItemPtr item(new CFileItem(*picture));
  m_slides->Add(item);
}

void CGUIWindowSlideShow::ShowNext()
{
  if (m_slides->Size() == 1)
    return;

  m_iNextSlide = m_iCurrentSlide + 1;
  if (m_iNextSlide >= m_slides->Size())
    m_iNextSlide = 0;

  m_bLoadNextPic = true;
}

void CGUIWindowSlideShow::ShowPrevious()
{
  if (m_slides->Size() == 1)
    return;

  m_iNextSlide = m_iCurrentSlide - 1;
  if (m_iNextSlide < 0)
    m_iNextSlide = m_slides->Size() - 1;
  m_bLoadNextPic = true;
}


void CGUIWindowSlideShow::Select(const CStdString& strPicture)
{
  for (int i = 0; i < m_slides->Size(); ++i)
  {
    const CFileItemPtr item = m_slides->Get(i);
    if (item->m_strPath == strPicture)
    {
      m_iCurrentSlide = i;
      m_iNextSlide = m_iCurrentSlide + 1;
      if (m_iNextSlide >= m_slides->Size())
        m_iNextSlide = 0;
      return ;
    }
  }
}

const CFileItemList &CGUIWindowSlideShow::GetSlideShowContents()
{
  return *m_slides;
}

const CFileItemPtr CGUIWindowSlideShow::GetCurrentSlide()
{
  if (m_iCurrentSlide >= 0 && m_iCurrentSlide < m_slides->Size())
    return m_slides->Get(m_iCurrentSlide);
  return CFileItemPtr();
}

bool CGUIWindowSlideShow::InSlideShow() const
{
  return m_bSlideShow;
}

void CGUIWindowSlideShow::StartSlideShow(bool screensaver)
{
  m_bSlideShow = true;
  m_bScreensaver = screensaver;
}

void CGUIWindowSlideShow::Render()
{
  // reset the screensaver if we're in a slideshow
  // (unless we are the screensaver!)
  if (m_bSlideShow && !g_application.IsInScreenSaver())
    g_application.ResetScreenSaver();
  int iSlides = m_slides->Size();
  if (!iSlides) return ;

  if (m_iNextSlide < 0 || m_iNextSlide >= m_slides->Size())
    m_iNextSlide = 0;
  if (m_iCurrentSlide < 0 || m_iCurrentSlide >= m_slides->Size())
    m_iCurrentSlide = 0;

  // Create our background loader if necessary
  if (!m_pBackgroundLoader)
  {
    m_pBackgroundLoader = new CBackgroundPicLoader();

    if (!m_pBackgroundLoader)
    {
      throw 1;
    }
    m_pBackgroundLoader->Create(this);
  }

  bool bSlideShow = m_bSlideShow && !m_bPause;

  if (m_bErrorMessage)
  { // we have an error when loading either the current or next picture
    // check to see if we have a picture loaded
    CLog::Log(LOGDEBUG, "We have an error loading a picture!");
    if (m_Image[m_iCurrentPic].IsLoaded())
    { // Yes.  Let's let it transistion out, wait for it to be released, then try loading again.
      CLog::Log(LOGERROR, "Error loading the next image %s", m_slides->Get(m_iNextSlide)->m_strPath.c_str());
      if (!bSlideShow)
      { // tell the pic to start transistioning out now
        m_Image[m_iCurrentPic].StartTransistion();
        m_Image[m_iCurrentPic].SetTransistionTime(1, IMMEDIATE_TRANSISTION_TIME); // only 20 frames for the transistion
      }
      m_bWaitForNextPic = true;
      m_bErrorMessage = false;
    }
    else
    { // No.  Not much we can do here.  If we're in a slideshow, we mayaswell move on to the next picture
      // change to next image
      if (bSlideShow)
      {
        CLog::Log(LOGERROR, "Error loading the current image %s", m_slides->Get(m_iCurrentSlide)->m_strPath.c_str());
        m_iCurrentSlide = m_iNextSlide;
        ShowNext();
        m_bErrorMessage = false;
      }
      else if (m_bLoadNextPic)
      {
        m_iCurrentSlide = m_iNextSlide;
        m_bErrorMessage = false;
      }
      // else just drop through - there's nothing we can do (error message will be displayed)
    }
  }
  if (m_bErrorMessage)
  {
    RenderErrorMessage();
    return ;
  }

  if (!m_Image[m_iCurrentPic].IsLoaded() && !m_pBackgroundLoader->IsLoading())
  { // load first image
    CLog::Log(LOGDEBUG, "Loading the current image %s", m_slides->Get(m_iCurrentSlide)->m_strPath.c_str());
    m_bWaitForNextPic = false;
    m_bLoadNextPic = false;
    // load using the background loader
    int maxWidth, maxHeight;
    GetCheckedSize((float)g_settings.m_ResInfo[m_Resolution].iWidth * zoomamount[m_iZoomFactor - 1],
                    (float)g_settings.m_ResInfo[m_Resolution].iHeight * zoomamount[m_iZoomFactor - 1],
                    maxWidth, maxHeight);
    m_pBackgroundLoader->LoadPic(m_iCurrentPic, m_iCurrentSlide, m_slides->Get(m_iCurrentSlide)->m_strPath, maxWidth, maxHeight);
  }

  // check if we should discard an already loaded next slide
  if (m_bLoadNextPic && m_Image[1 - m_iCurrentPic].IsLoaded() && m_Image[1 - m_iCurrentPic].SlideNumber() != m_iNextSlide)
  {
    m_Image[1 - m_iCurrentPic].Close();
  }
  // if we're reloading an image (for better res on zooming we need to close any open ones as well)
  if (m_bReloadImage && m_Image[1 - m_iCurrentPic].IsLoaded() && m_Image[1 - m_iCurrentPic].SlideNumber() != m_iCurrentSlide)
  {
    m_Image[1 - m_iCurrentPic].Close();
  }

  if (m_bReloadImage)
  {
    if (m_Image[m_iCurrentPic].IsLoaded() && !m_Image[1 - m_iCurrentPic].IsLoaded() && !m_pBackgroundLoader->IsLoading() && !m_bWaitForNextPic)
    { // reload the image if we need to
      CLog::Log(LOGDEBUG, "Reloading the current image %s at zoom level %i", m_slides->Get(m_iCurrentSlide)->m_strPath.c_str(), m_iZoomFactor);
      // first, our maximal size for this zoom level
      int maxWidth = (int)((float)g_settings.m_ResInfo[m_Resolution].iWidth * zoomamount[m_iZoomFactor - 1]);
      int maxHeight = (int)((float)g_settings.m_ResInfo[m_Resolution].iWidth * zoomamount[m_iZoomFactor - 1]);

      // the actual maximal size of the image to optimize the sizing based on the known sizing (aspect ratio)
      int width, height;
      GetCheckedSize((float)m_Image[m_iCurrentPic].GetOriginalWidth(), (float)m_Image[m_iCurrentPic].GetOriginalHeight(), width, height);

      // use the smaller of the two (no point zooming in more than we have to)
      if (maxWidth < width) width = maxWidth;
      if (maxHeight < height) height = maxHeight;

      m_pBackgroundLoader->LoadPic(m_iCurrentPic, m_iCurrentSlide, m_slides->Get(m_iCurrentSlide)->m_strPath, width, height);
    }
  }
  else
  {
    if ((bSlideShow || m_bLoadNextPic) && m_Image[m_iCurrentPic].IsLoaded() && !m_Image[1 - m_iCurrentPic].IsLoaded() && !m_pBackgroundLoader->IsLoading() && !m_bWaitForNextPic)
    { // load the next image
      CLog::Log(LOGDEBUG, "Loading the next image %s", m_slides->Get(m_iNextSlide)->m_strPath.c_str());
      int maxWidth, maxHeight;
      GetCheckedSize((float)g_settings.m_ResInfo[m_Resolution].iWidth * zoomamount[m_iZoomFactor - 1],
                     (float)g_settings.m_ResInfo[m_Resolution].iHeight * zoomamount[m_iZoomFactor - 1],
                     maxWidth, maxHeight);
      m_pBackgroundLoader->LoadPic(1 - m_iCurrentPic, m_iNextSlide, m_slides->Get(m_iNextSlide)->m_strPath, maxWidth, maxHeight);
    }
  }

  // render the current image
  if (m_Image[m_iCurrentPic].IsLoaded())
  {
    m_Image[m_iCurrentPic].SetInSlideshow(m_bSlideShow);
    m_Image[m_iCurrentPic].Pause(m_bPause);
    m_Image[m_iCurrentPic].Render();
  }

  // Check if we should be transistioning immediately
  if (m_bLoadNextPic)
  {
    CLog::Log(LOGDEBUG, "Starting immediate transistion due to user wanting slide %s", m_slides->Get(m_iNextSlide)->m_strPath.c_str());
    if (m_Image[m_iCurrentPic].StartTransistion())
    {
      m_Image[m_iCurrentPic].SetTransistionTime(1, IMMEDIATE_TRANSISTION_TIME); // only 20 frames for the transistion
      m_bLoadNextPic = false;
    }
  }

  // render the next image
  if (m_Image[m_iCurrentPic].DrawNextImage())
  {
    if (m_Image[1 - m_iCurrentPic].IsLoaded())
    {
      // set the appropriate transistion time
      m_Image[1 - m_iCurrentPic].SetTransistionTime(0, m_Image[m_iCurrentPic].GetTransistionTime(1));
      m_Image[1 - m_iCurrentPic].Pause(m_bPause);
      m_Image[1 - m_iCurrentPic].Render();
    }
    else // next pic isn't loaded.  We should hang around if it is in progress
    {
      if (m_pBackgroundLoader->IsLoading())
      {
//        CLog::Log(LOGDEBUG, "Having to hold the current image (%s) while we load %s", m_vecSlides[m_iCurrentSlide].c_str(), m_vecSlides[m_iNextSlide].c_str());
        m_Image[m_iCurrentPic].Keep();
      }
    }
  }

  // check if we should swap images now
  if (m_Image[m_iCurrentPic].IsFinished())
  {
    CLog::Log(LOGDEBUG, "Image %s is finished rendering, switching to %s", m_slides->Get(m_iCurrentSlide)->m_strPath.c_str(), m_slides->Get(m_iNextSlide)->m_strPath.c_str());
    m_Image[m_iCurrentPic].Close();
    if (m_Image[1 - m_iCurrentPic].IsLoaded())
      m_iCurrentPic = 1 - m_iCurrentPic;
    m_iCurrentSlide = m_iNextSlide;
    if (bSlideShow)
    {
      m_iNextSlide++;
      if (m_iNextSlide >= m_slides->Size())
        m_iNextSlide = 0;
    }
//    m_iZoomFactor = 1;
    m_iRotate = 0;
  }

  RenderPause();

  if (m_Image[m_iCurrentPic].IsLoaded())
    g_infoManager.SetCurrentSlide(*m_slides->Get(m_iCurrentSlide));

  RenderErrorMessage();

  CGUIWindow::Render();
}

bool CGUIWindowSlideShow::OnAction(const CAction &action)
{
  if (m_bScreensaver)
  {
    g_windowManager.PreviousWindow();
    return true;
  }

  switch (action.GetID())
  {
  case ACTION_SHOW_CODEC:
    {
      CGUIDialogPictureInfo *pictureInfo = (CGUIDialogPictureInfo *)g_windowManager.GetWindow(WINDOW_DIALOG_PICTURE_INFO);
      if (pictureInfo)
      {
        // no need to set the picture here, it's done in Render()
        pictureInfo->DoModal();
      }
    }
    break;
  case ACTION_PREVIOUS_MENU:
  case ACTION_STOP:
    g_windowManager.PreviousWindow();
    break;
  case ACTION_NEXT_PICTURE:
//    if (m_iZoomFactor == 1)
      ShowNext();
    break;
  case ACTION_PREV_PICTURE:
//    if (m_iZoomFactor == 1)
      ShowPrevious();
    break;
  case ACTION_MOVE_RIGHT:
    if (m_iZoomFactor == 1)
      ShowNext();
    else
      Move(PICTURE_MOVE_AMOUNT, 0);
    break;

  case ACTION_MOVE_LEFT:
    if (m_iZoomFactor == 1)
      ShowPrevious();
    else
      Move( -PICTURE_MOVE_AMOUNT, 0);
    break;

  case ACTION_MOVE_DOWN:
    Move(0, PICTURE_MOVE_AMOUNT);
    break;

  case ACTION_MOVE_UP:
    Move(0, -PICTURE_MOVE_AMOUNT);
    break;

  case ACTION_PAUSE:
    if (m_bSlideShow)
      m_bPause = !m_bPause;
    break;

  case ACTION_PLAYER_PLAY:
    if (!m_bSlideShow)
    {
      m_bSlideShow = true;
      m_bPause = false;
    }
    else if (m_bPause)
      m_bPause = false;
    break;

  case ACTION_ZOOM_OUT:
    Zoom(m_iZoomFactor - 1);
    break;

  case ACTION_ZOOM_IN:
    Zoom(m_iZoomFactor + 1);
    break;

  case ACTION_ROTATE_PICTURE:
    Rotate();
    break;

  case ACTION_ZOOM_LEVEL_NORMAL:
  case ACTION_ZOOM_LEVEL_1:
  case ACTION_ZOOM_LEVEL_2:
  case ACTION_ZOOM_LEVEL_3:
  case ACTION_ZOOM_LEVEL_4:
  case ACTION_ZOOM_LEVEL_5:
  case ACTION_ZOOM_LEVEL_6:
  case ACTION_ZOOM_LEVEL_7:
  case ACTION_ZOOM_LEVEL_8:
  case ACTION_ZOOM_LEVEL_9:
    Zoom((action.GetID() - ACTION_ZOOM_LEVEL_NORMAL) + 1);
    break;
  case ACTION_ANALOG_MOVE:
    Move(action.GetAmount()*PICTURE_MOVE_AMOUNT_ANALOG, -action.GetAmount(1)*PICTURE_MOVE_AMOUNT_ANALOG);
    break;
  default:
    return CGUIWindow::OnAction(action);
  }
  return true;
}

void CGUIWindowSlideShow::RenderErrorMessage()
{
  if (!m_bErrorMessage)
    return ;

  const CGUIControl *control = GetControl(LABEL_ROW1);
  if (NULL == control || control->GetControlType() != CGUIControl::GUICONTROL_LABEL)
  {
     CLog::Log(LOGERROR,"CGUIWindowSlideShow::RenderErrorMessage - cant get label control!");
     return;
  }

  CGUIFont *pFont = ((CGUILabelControl *)control)->GetLabelInfo().font;
  CGUITextLayout::DrawText(pFont, 0.5f*g_graphicsContext.GetWidth(), 0.5f*g_graphicsContext.GetHeight(), 0xffffffff, 0, g_localizeStrings.Get(747), XBFONT_CENTER_X | XBFONT_CENTER_Y);
}

bool CGUIWindowSlideShow::OnMessage(CGUIMessage& message)
{
  switch ( message.GetMessage() )
  {
  case GUI_MSG_WINDOW_DEINIT:
    {
      if (m_Resolution != g_guiSettings.m_LookAndFeelResolution)
      {
        //FIXME: Use GUI resolution for now
        //g_graphicsContext.SetVideoResolution(g_guiSettings.m_LookAndFeelResolution, TRUE);
      }

      //   Reset();
      if (message.GetParam1() != WINDOW_PICTURES)
      {
        m_ImageLib.Unload();
      }
      g_windowManager.ShowOverlay(OVERLAY_STATE_SHOWN);
      FreeResources();
    }
    break;

  case GUI_MSG_WINDOW_INIT:
    {
      m_Resolution = (RESOLUTION) g_guiSettings.GetInt("pictures.displayresolution");

      //FIXME: Use GUI resolution for now
      if (0 /*m_Resolution != g_guiSettings.m_LookAndFeelResolution && m_Resolution != INVALID && m_Resolution!=AUTORES*/)
      {
        g_graphicsContext.SetVideoResolution(m_Resolution);
      }
      else
      {
        m_Resolution = g_graphicsContext.GetVideoResolution();
      }

      CGUIWindow::OnMessage(message);
      if (message.GetParam1() != WINDOW_PICTURES)
      {
        m_ImageLib.Load();
      }
      g_windowManager.ShowOverlay(OVERLAY_STATE_HIDDEN);

      // turn off slideshow if we only have 1 image
      if (m_slides->Size() <= 1)
        m_bSlideShow = false;

      return true;
    }
    break;
  case GUI_MSG_START_SLIDESHOW:
    {
      CStdString strFolder = message.GetStringParam();
      unsigned int iParams = message.GetParam1();
      //decode params
      bool bRecursive = false;
      bool bRandom = false;
      bool bNotRandom = false;
      if (iParams > 0)
      {
        if ((iParams & 1) == 1)
          bRecursive = true;
        if ((iParams & 2) == 2)
          bRandom = true;
        if ((iParams & 4) == 4)
          bNotRandom = true;
      }
      RunSlideShow(strFolder, bRecursive, bRandom, bNotRandom);
    }
    break;
  }
  return CGUIWindow::OnMessage(message);
}

void CGUIWindowSlideShow::RenderPause()
{ // display the pause icon
  if (m_bPause)
  {
    SET_CONTROL_VISIBLE(CONTROL_PAUSE);
  }
  else
  {
    SET_CONTROL_HIDDEN(CONTROL_PAUSE);
  }
  /*
   static DWORD dwCounter=0;
   dwCounter++;
   if (dwCounter > 25)
   {
    dwCounter=0;
   }
   if (!m_bPause) return;
   if (dwCounter <13) return;*/

}

void CGUIWindowSlideShow::Rotate()
{
  if (!m_Image[m_iCurrentPic].DrawNextImage() && m_iZoomFactor == 1)
  {
    m_Image[m_iCurrentPic].Rotate(++m_iRotate);
  }
}

void CGUIWindowSlideShow::Zoom(int iZoom)
{
  if (iZoom > MAX_ZOOM_FACTOR || iZoom < 1)
    return ;
  // set the zoom amount and then set so that the image is reloaded at the higher (or lower)
  // resolution as necessary
  if (!m_Image[m_iCurrentPic].DrawNextImage())
  {
    m_Image[m_iCurrentPic].Zoom(iZoom);
    // check if we need to reload the image for better resolution
#ifdef RELOAD_ON_ZOOM
    if (iZoom > m_iZoomFactor && !m_Image[m_iCurrentPic].FullSize())
      m_bReloadImage = true;
    if (iZoom == 1)
      m_bReloadImage = true;
#endif
    m_iZoomFactor = iZoom;
  }
}

void CGUIWindowSlideShow::Move(float fX, float fY)
{
  if (m_Image[m_iCurrentPic].IsLoaded() && m_Image[m_iCurrentPic].GetZoom() > 1)
  { // we move in the opposite direction, due to the fact we are moving
    // the viewing window, not the picture.
    m_Image[m_iCurrentPic].Move( -fX, -fY);
  }
}

void CGUIWindowSlideShow::OnLoadPic(int iPic, int iSlideNumber, CBaseTexture* pTexture, int iOriginalWidth, int iOriginalHeight, bool bFullSize)
{
  if (pTexture)
  {
    // set the pic's texture + size etc.
    CSingleLock lock(m_slideSection);
    if (iSlideNumber >= m_slides->Size())
    { // throw this away - we must have cleared the slideshow while we were still loading
      delete pTexture;
      return;
    }
    CLog::Log(LOGDEBUG, "Finished background loading %s", m_slides->Get(iSlideNumber)->m_strPath.c_str());
    if (m_bReloadImage)
    {
      if (m_Image[m_iCurrentPic].IsLoaded() && m_Image[m_iCurrentPic].SlideNumber() != iSlideNumber)
      { // wrong image (ie we finished loading the next image, not the current image)
        delete pTexture;
        return;
      }
      m_Image[m_iCurrentPic].UpdateTexture(pTexture);
      m_Image[m_iCurrentPic].SetOriginalSize(iOriginalWidth, iOriginalHeight, bFullSize);
      m_bReloadImage = false;
    }
    else
    {
      if (m_bSlideShow)
        m_Image[iPic].SetTexture(iSlideNumber, pTexture, g_guiSettings.GetBool("slideshow.displayeffects") ? CSlideShowPic::EFFECT_RANDOM : CSlideShowPic::EFFECT_NONE);
      else
        m_Image[iPic].SetTexture(iSlideNumber, pTexture, CSlideShowPic::EFFECT_NO_TIMEOUT);
      m_Image[iPic].SetOriginalSize(iOriginalWidth, iOriginalHeight, bFullSize);
      m_Image[iPic].Zoom(m_iZoomFactor, true);

      m_Image[iPic].m_bIsComic = false;
      if (CUtil::IsInRAR(m_slides->Get(m_iCurrentSlide)->m_strPath) || CUtil::IsInZIP(m_slides->Get(m_iCurrentSlide)->m_strPath)) // move to top for cbr/cbz
      {
        CURL url(m_slides->Get(m_iCurrentSlide)->m_strPath);
        CStdString strHostName = url.GetHostName();
        if (CUtil::GetExtension(strHostName).Equals(".cbr", false) || CUtil::GetExtension(strHostName).Equals(".cbz", false))
        {
          m_Image[iPic].m_bIsComic = true;
          m_Image[iPic].Move((float)m_Image[iPic].GetOriginalWidth(),(float)m_Image[iPic].GetOriginalHeight());
        }
      }
    }
  }
  else
  { // Failed to load image.  What should be done??
    // We should wait for the current pic to finish rendering, then transistion it out,
    // release the texture, and try and reload this pic from scratch
    m_bErrorMessage = true;
  }
}

void CGUIWindowSlideShow::Shuffle()
{
  m_slides->Randomize();
  m_iCurrentSlide = 0;
  m_iNextSlide = 1;
}

int CGUIWindowSlideShow::NumSlides() const
{
  return m_slides->Size();
}

int CGUIWindowSlideShow::CurrentSlide() const
{
  return m_iCurrentSlide + 1;
}

void CGUIWindowSlideShow::AddFromPath(const CStdString &strPath,
                                      bool bRecursive, 
                                      SORT_METHOD method, SORT_ORDER order)
{
  if (strPath!="")
  {
    // reset the slideshow
    Reset();
    if (bRecursive)
    {
      path_set recursivePaths;
      AddItems(strPath, &recursivePaths, method, order);
    }
    else
      AddItems(strPath, NULL, method, order);
  }
}

void CGUIWindowSlideShow::RunSlideShow(const CStdString &strPath, bool bRecursive /* = false */, bool bRandom /* = false */, bool bNotRandom /* = false */, SORT_METHOD method /* = SORT_METHOD_LABEL */, SORT_ORDER order /* = SORT_ORDER_ASC */)
{
  // stop any video
  if (g_application.IsPlayingVideo())
    g_application.StopPlaying();

  AddFromPath(strPath, bRecursive, method, order);

  // mutually exclusive options
  // if both are set, clear both and use the gui setting
  if (bRandom && bNotRandom)
    bRandom = bNotRandom = false;

  // NotRandom overrides the window setting
  if ((!bNotRandom && g_guiSettings.GetBool("slideshow.shuffle")) || bRandom)
    Shuffle();

  StartSlideShow();
  if (NumSlides())
    g_windowManager.ActivateWindow(WINDOW_SLIDESHOW);
}

void CGUIWindowSlideShow::AddItems(const CStdString &strPath, path_set *recursivePaths, SORT_METHOD method, SORT_ORDER order)
{
  // check whether we've already added this path
  if (recursivePaths)
  {
    CStdString path(strPath);
    CUtil::RemoveSlashAtEnd(path);
    if (recursivePaths->find(path) != recursivePaths->end())
      return;
    recursivePaths->insert(path);
  }

  // fetch directory and sort accordingly
  CFileItemList items;
  if (!CDirectory::GetDirectory(strPath, items, g_settings.m_pictureExtensions,false,false,DIR_CACHE_ONCE,true,true))
    return;

  items.Sort(method, order);

  // need to go into all subdirs
  for (int i = 0; i < items.Size(); i++)
  {
    CFileItemPtr item = items[i];
    if (item->m_bIsFolder && recursivePaths)
    {
      AddItems(item->m_strPath, recursivePaths);
    }
    else if (!item->m_bIsFolder && !CUtil::IsRAR(item->m_strPath) && !CUtil::IsZIP(item->m_strPath))
    { // add to the slideshow
      Add(item.get());
    }
  }
}

void CGUIWindowSlideShow::GetCheckedSize(float width, float height, int &maxWidth, int &maxHeight)
{
#ifdef RELOAD_ON_ZOOM
  if (width * height > MAX_PICTURE_SIZE)
  {
    float fScale = sqrt((float)MAX_PICTURE_SIZE / (width * height));
    width = fScale * width;
    height = fScale * height;
  }
  maxWidth = (int)width;
  maxHeight = (int)height;
  if (maxWidth > g_Windowing.GetMaxTextureSize()) maxWidth = g_graphicsContext.GetMaxTextureSize();
  if (maxHeight > g_Windowing.GetMaxTextureSize()) maxHeight = g_graphicsContext.GetMaxTextureSize();
#else
  maxWidth = g_Windowing.GetMaxTextureSize();
  maxHeight = g_Windowing.GetMaxTextureSize();
#endif
}


