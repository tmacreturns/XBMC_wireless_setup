/*
 *      Copyright (C) 2005-2010 Team XBMC
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

#pragma once

/*!
 \file GUIStaticItem.h
 \brief
 */

#include "GUIInfoTypes.h"
#include "../xbmc/FileItem.h"

class TiXmlElement;

/*!
 \ingroup lists,items
 \brief wrapper class for a static item in a list container
 
 A wrapper class for the items in a container specified via the <content>
 flag.  Handles constructing items from XML and updating item labels, icons
 and properties.
 
 \sa CFileItem, CGUIBaseContainer
 */
class CGUIStaticItem : public CFileItem
{
public:
  /*! \brief constructor
   Construct an item based on an XML description:
     <item>
       <label>$INFO[MusicPlayer.Artist]</label>
       <label2>$INFO[MusicPlayer.Album]</label2>
       <thumb>bar.png</thumb>
       <icon>foo.jpg</icon>
       <onclick>ActivateWindow(Home)</onclick>
     </item>
   
   \param element XML element to construct from
   \param contextWindow window context to use for any info labels
   */
  CGUIStaticItem(const TiXmlElement *element, int contextWindow);
  virtual ~CGUIStaticItem() {};
  virtual CGUIListItem *Clone() const { return new CGUIStaticItem(*this); };
  
  /*! \brief update any infolabels in the items properties
   Runs through all the items properties, updating any that should be
   periodically recomputed
   \param contextWindow window context to use for any info labels
   */
  void UpdateProperties(int contextWindow);
private:
  typedef std::vector< std::pair<CGUIInfoLabel, CStdString> > InfoVector;
  InfoVector m_info;
};

typedef boost::shared_ptr<CGUIStaticItem> CGUIStaticItemPtr;
