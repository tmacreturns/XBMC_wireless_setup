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

#include "BaseRenderer.h"
#include "Settings.h"
#include "GUISettings.h"
#include "GraphicContext.h"
#include "utils/log.h"
#include "MathUtils.h"


CBaseRenderer::CBaseRenderer()
{
  m_sourceFrameRatio = 1.0f;
  m_sourceWidth = 720;
  m_sourceHeight = 480;
  m_resolution = RES_DESKTOP;
}

CBaseRenderer::~CBaseRenderer()
{
}

void CBaseRenderer::ChooseBestResolution(float fps)
{
  m_resolution = g_guiSettings.m_LookAndFeelResolution;
  if ( m_resolution == RES_WINDOW )
    m_resolution = RES_DESKTOP;
  // Adjust refreshrate to match source fps
#if !defined(__APPLE__)
  if (g_guiSettings.GetBool("videoplayer.adjustrefreshrate"))
  {
    // Find closest refresh rate
    for (size_t i = (int)RES_CUSTOM; i < g_settings.m_ResInfo.size(); i++)
    {
      RESOLUTION_INFO &curr = g_settings.m_ResInfo[m_resolution];
      RESOLUTION_INFO &info = g_settings.m_ResInfo[i];

      if (info.iWidth  != curr.iWidth
      ||  info.iHeight != curr.iHeight
      ||  info.iScreen != curr.iScreen)
        continue;

      // we assume just a tad lower fps since this calculation will discard
      // any refreshrate that is smaller by just the smallest amount
      int c_weight = (int)(1000 * fmodf(curr.fRefreshRate, fps - 0.01f) / curr.fRefreshRate);
      int i_weight = (int)(1000 * fmodf(info.fRefreshRate, fps - 0.01f) / info.fRefreshRate);

      // Closer the better, prefer higher refresh rate if the same
      if ((i_weight <  c_weight)
      ||  (i_weight == c_weight && info.fRefreshRate > curr.fRefreshRate))
        m_resolution = (RESOLUTION)i;
    }

    CLog::Log(LOGNOTICE, "Display resolution ADJUST : %s (%d)", g_settings.m_ResInfo[m_resolution].strMode.c_str(), m_resolution);
  }
  else
#endif
    CLog::Log(LOGNOTICE, "Display resolution %s : %s (%d)", m_resolution == RES_DESKTOP ? "DESKTOP" : "USER", g_settings.m_ResInfo[m_resolution].strMode.c_str(), m_resolution);
}

RESOLUTION CBaseRenderer::GetResolution() const
{
  if (g_graphicsContext.IsFullScreenRoot() && (g_graphicsContext.IsFullScreenVideo() || g_graphicsContext.IsCalibrating()))
    return m_resolution;

  return g_graphicsContext.GetVideoResolution();
}

float CBaseRenderer::GetAspectRatio() const
{
  float width = (float)m_sourceWidth - g_settings.m_currentVideoSettings.m_CropLeft - g_settings.m_currentVideoSettings.m_CropRight;
  float height = (float)m_sourceHeight - g_settings.m_currentVideoSettings.m_CropTop - g_settings.m_currentVideoSettings.m_CropBottom;
  return m_sourceFrameRatio * width / height * m_sourceHeight / m_sourceWidth;
}

void CBaseRenderer::GetVideoRect(CRect &source, CRect &dest)
{
  source = m_sourceRect;
  dest = m_destRect;
}

void CBaseRenderer::CalcNormalDisplayRect(float offsetX, float offsetY, float screenWidth, float screenHeight, float inputFrameRatio, float zoomAmount)
{
  // scale up image as much as possible
  // and keep the aspect ratio (introduces with black bars)
  // calculate the correct output frame ratio (using the users pixel ratio setting
  // and the output pixel ratio setting)

  float outputFrameRatio = inputFrameRatio / g_settings.m_ResInfo[GetResolution()].fPixelRatio;

  // allow a certain error to maximize screen size
  float fCorrection = screenWidth / screenHeight / outputFrameRatio - 1.0f;
  float fAllowed    = g_guiSettings.GetInt("videoplayer.errorinaspect") * 0.01f;
  if(fCorrection >   fAllowed) fCorrection =   fAllowed;
  if(fCorrection < - fAllowed) fCorrection = - fAllowed;

  outputFrameRatio *= 1.0f + fCorrection;

  // maximize the movie width
  float newWidth = screenWidth;
  float newHeight = newWidth / outputFrameRatio;

  if (newHeight > screenHeight)
  {
    newHeight = screenHeight;
    newWidth = newHeight * outputFrameRatio;
  }

  // Scale the movie up by set zoom amount
  newWidth *= zoomAmount;
  newHeight *= zoomAmount;

  // Centre the movie
  float posY = (screenHeight - newHeight) / 2;
  float posX = (screenWidth - newWidth) / 2;

  m_destRect.x1 = (float)MathUtils::round_int(posX + offsetX);
  m_destRect.x2 = m_destRect.x1 + MathUtils::round_int(newWidth);
  m_destRect.y1 = (float)MathUtils::round_int(posY + offsetY);
  m_destRect.y2 = m_destRect.y1 + MathUtils::round_int(newHeight);

  // clip as needed
  if (!(g_graphicsContext.IsFullScreenVideo() || g_graphicsContext.IsCalibrating()))
  {
    CRect original(m_destRect);
    m_destRect.Intersect(CRect(offsetX, offsetY, offsetX + screenWidth, offsetY + screenHeight));
    if (m_destRect != original)
    {
      float scaleX = m_sourceRect.Width() / original.Width();
      float scaleY = m_sourceRect.Height() / original.Height();
      m_sourceRect.x1 += (m_destRect.x1 - original.x1) * scaleX;
      m_sourceRect.y1 += (m_destRect.y1 - original.y1) * scaleY;
      m_sourceRect.x2 += (m_destRect.x2 - original.x2) * scaleX;
      m_sourceRect.y2 += (m_destRect.y2 - original.y2) * scaleY;
    }
  }
}

//***************************************************************************************
// CalculateFrameAspectRatio()
//
// Considers the source frame size and output frame size (as suggested by mplayer)
// to determine if the pixels in the source are not square.  It calculates the aspect
// ratio of the output frame.  We consider the cases of VCD, SVCD and DVD separately,
// as these are intended to be viewed on a non-square pixel TV set, so the pixels are
// defined to be the same ratio as the intended display pixels.
// These formats are determined by frame size.
//***************************************************************************************
void CBaseRenderer::CalculateFrameAspectRatio(unsigned int desired_width, unsigned int desired_height)
{
  m_sourceFrameRatio = (float)desired_width / desired_height;

  // Check whether mplayer has decided that the size of the video file should be changed
  // This indicates either a scaling has taken place (which we didn't ask for) or it has
  // found an aspect ratio parameter from the file, and is changing the frame size based
  // on that.
  if (m_sourceWidth == (unsigned int) desired_width && m_sourceHeight == (unsigned int) desired_height)
    return ;

  // mplayer is scaling in one or both directions.  We must alter our Source Pixel Ratio
  float imageFrameRatio = (float)m_sourceWidth / m_sourceHeight;

  // OK, most sources will be correct now, except those that are intended
  // to be displayed on non-square pixel based output devices (ie PAL or NTSC TVs)
  // This includes VCD, SVCD, and DVD (and possibly others that we are not doing yet)
  // For this, we can base the pixel ratio on the pixel ratios of PAL and NTSC,
  // though we will need to adjust for anamorphic sources (ie those whose
  // output frame ratio is not 4:3) and for SVCDs which have 2/3rds the
  // horizontal resolution of the default NTSC or PAL frame sizes

  // The following are the defined standard ratios for PAL and NTSC pixels
  // NOTE: These aren't technically (in terms of BT601) correct - the commented values are,
  //       but it seems that many DVDs nowadays are mastered incorrectly, so two wrongs
  //       may indeed make a right.  The "wrong" values here ensure the output frame is
  //       4x3 (or 16x9)
  const float PALPixelRatio = 16.0f / 15.0f;      // 128.0f / 117.0f;
  const float NTSCPixelRatio = 8.0f / 9.0f;       // 4320.0f / 4739.0f;

  // Calculate the correction needed for anamorphic sources
  float Non4by3Correction = m_sourceFrameRatio / (4.0f / 3.0f);

  // Finally, check for a VCD, SVCD or DVD frame size as these need special aspect ratios
  if (m_sourceWidth == 352)
  { // VCD?
    if (m_sourceHeight == 240) // NTSC
      m_sourceFrameRatio = imageFrameRatio * NTSCPixelRatio;
    if (m_sourceHeight == 288) // PAL
      m_sourceFrameRatio = imageFrameRatio * PALPixelRatio;
  }
  if (m_sourceWidth == 480)
  { // SVCD?
    if (m_sourceHeight == 480) // NTSC
      m_sourceFrameRatio = imageFrameRatio * 3.0f / 2.0f * NTSCPixelRatio * Non4by3Correction;
    if (m_sourceHeight == 576) // PAL
      m_sourceFrameRatio = imageFrameRatio * 3.0f / 2.0f * PALPixelRatio * Non4by3Correction;
  }
  if (m_sourceWidth == 720)
  { // DVD?
    if (m_sourceHeight == 480) // NTSC
      m_sourceFrameRatio = imageFrameRatio * NTSCPixelRatio * Non4by3Correction;
    if (m_sourceHeight == 576) // PAL
      m_sourceFrameRatio = imageFrameRatio * PALPixelRatio * Non4by3Correction;
  }
}

void CBaseRenderer::ManageDisplay()
{
  const CRect& view = g_graphicsContext.GetViewWindow();

  m_sourceRect.x1 = (float)g_settings.m_currentVideoSettings.m_CropLeft;
  m_sourceRect.y1 = (float)g_settings.m_currentVideoSettings.m_CropTop;
  m_sourceRect.x2 = (float)m_sourceWidth - g_settings.m_currentVideoSettings.m_CropRight;
  m_sourceRect.y2 = (float)m_sourceHeight - g_settings.m_currentVideoSettings.m_CropBottom;

  CalcNormalDisplayRect(view.x1, view.y1, view.Width(), view.Height(), GetAspectRatio() * g_settings.m_fPixelRatio, g_settings.m_fZoomAmount);
}

void CBaseRenderer::SetViewMode(int viewMode)
{
  if (viewMode < VIEW_MODE_NORMAL || viewMode > VIEW_MODE_CUSTOM)
    viewMode = VIEW_MODE_NORMAL;

  g_settings.m_currentVideoSettings.m_ViewMode = viewMode;

  // get our calibrated full screen resolution
  RESOLUTION res = GetResolution();
  float screenWidth = (float)(g_settings.m_ResInfo[res].Overscan.right - g_settings.m_ResInfo[res].Overscan.left);
  float screenHeight = (float)(g_settings.m_ResInfo[res].Overscan.bottom - g_settings.m_ResInfo[res].Overscan.top);
  // and the source frame ratio
  float sourceFrameRatio = GetAspectRatio();

  bool is43 = (sourceFrameRatio < 8.f/(3.f*sqrt(3.f)) &&
              g_settings.m_currentVideoSettings.m_ViewMode == VIEW_MODE_NORMAL);

  g_settings.m_bNonLinStretch = false;

  if ( g_settings.m_currentVideoSettings.m_ViewMode == VIEW_MODE_ZOOM ||
       (is43 && g_guiSettings.GetInt("videoplayer.stretch43") == VIEW_MODE_ZOOM))
  { // zoom image so no black bars
    g_settings.m_fPixelRatio = 1.0;
    // calculate the desired output ratio
    float outputFrameRatio = sourceFrameRatio * g_settings.m_fPixelRatio / g_settings.m_ResInfo[res].fPixelRatio;
    // now calculate the correct zoom amount.  First zoom to full height.
    float newHeight = screenHeight;
    float newWidth = newHeight * outputFrameRatio;
    g_settings.m_fZoomAmount = newWidth / screenWidth;
    if (newWidth < screenWidth)
    { // zoom to full width
      newWidth = screenWidth;
      newHeight = newWidth / outputFrameRatio;
      g_settings.m_fZoomAmount = newHeight / screenHeight;
    }
  }
  else if (g_settings.m_currentVideoSettings.m_ViewMode == VIEW_MODE_STRETCH_4x3)
  { // stretch image to 4:3 ratio
    g_settings.m_fZoomAmount = 1.0;
    if (res == RES_PAL_4x3 || res == RES_PAL60_4x3 || res == RES_NTSC_4x3 || res == RES_HDTV_480p_4x3)
    { // stretch to the limits of the 4:3 screen.
      // incorrect behaviour, but it's what the users want, so...
      g_settings.m_fPixelRatio = (screenWidth / screenHeight) * g_settings.m_ResInfo[res].fPixelRatio / sourceFrameRatio;
    }
    else
    {
      // now we need to set g_settings.m_fPixelRatio so that
      // fOutputFrameRatio = 4:3.
      g_settings.m_fPixelRatio = (4.0f / 3.0f) / sourceFrameRatio;
    }
  }
  else if ( g_settings.m_currentVideoSettings.m_ViewMode == VIEW_MODE_WIDE_ZOOM ||
           (is43 && g_guiSettings.GetInt("videoplayer.stretch43") == VIEW_MODE_WIDE_ZOOM))
  { // super zoom
    float stretchAmount = (screenWidth / screenHeight) * g_settings.m_ResInfo[res].fPixelRatio / sourceFrameRatio;
    g_settings.m_fPixelRatio = pow(stretchAmount, float(2.0/3.0));
    g_settings.m_fZoomAmount = pow(stretchAmount, float((stretchAmount < 1.0) ? -1.0/3.0 : 1.0/3.0));
    g_settings.m_bNonLinStretch = true;
  }
  else if ( g_settings.m_currentVideoSettings.m_ViewMode == VIEW_MODE_STRETCH_16x9 ||
           (is43 && g_guiSettings.GetInt("videoplayer.stretch43") == VIEW_MODE_STRETCH_16x9))
  { // stretch image to 16:9 ratio
    g_settings.m_fZoomAmount = 1.0;
    if (res == RES_PAL_4x3 || res == RES_PAL60_4x3 || res == RES_NTSC_4x3 || res == RES_HDTV_480p_4x3)
    { // now we need to set g_settings.m_fPixelRatio so that
      // outputFrameRatio = 16:9.
      g_settings.m_fPixelRatio = (16.0f / 9.0f) / sourceFrameRatio;
    }
    else
    { // stretch to the limits of the 16:9 screen.
      // incorrect behaviour, but it's what the users want, so...
      g_settings.m_fPixelRatio = (screenWidth / screenHeight) * g_settings.m_ResInfo[res].fPixelRatio / sourceFrameRatio;
    }
  }
  else  if (g_settings.m_currentVideoSettings.m_ViewMode == VIEW_MODE_ORIGINAL)
  { // zoom image so that the height is the original size
    g_settings.m_fPixelRatio = 1.0;
    // get the size of the media file
    // calculate the desired output ratio
    float outputFrameRatio = sourceFrameRatio * g_settings.m_fPixelRatio / g_settings.m_ResInfo[res].fPixelRatio;
    // now calculate the correct zoom amount.  First zoom to full width.
    float newWidth = screenWidth;
    float newHeight = newWidth / outputFrameRatio;
    if (newHeight > screenHeight)
    { // zoom to full height
      newHeight = screenHeight;
      newWidth = newHeight * outputFrameRatio;
    }
    // now work out the zoom amount so that no zoom is done
    g_settings.m_fZoomAmount = (m_sourceHeight - g_settings.m_currentVideoSettings.m_CropTop - g_settings.m_currentVideoSettings.m_CropBottom) / newHeight;
  }
  else if (g_settings.m_currentVideoSettings.m_ViewMode == VIEW_MODE_CUSTOM)
  {
    g_settings.m_fZoomAmount = g_settings.m_currentVideoSettings.m_CustomZoomAmount;
    g_settings.m_fPixelRatio = g_settings.m_currentVideoSettings.m_CustomPixelRatio;
    g_settings.m_bNonLinStretch = g_settings.m_currentVideoSettings.m_CustomNonLinStretch;
  }
  else // if (g_settings.m_currentVideoSettings.m_ViewMode == VIEW_MODE_NORMAL)
  {
    g_settings.m_fPixelRatio = 1.0;
    g_settings.m_fZoomAmount = 1.0;
  }
}
