/***************************************************************************
 *            compositor.cpp
 *
 *  Wed Jun 18 12:00:00 CEST 2017
 *  Copyright 2017 Lars Muldjord
 *  muldjordlars@gmail.com
 ****************************************************************************/
/*
 *  This file is part of skyscraper.
 *
 *  skyscraper is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  skyscraper is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with skyscraper; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <cmath>
#include <QSettings>
#include <QPainter>

#include "compositor.h"
#include "strtools.h"

#include "fxshadow.h"

Compositor::Compositor(Settings &config)
{
  this->config = config;
}

bool Compositor::processXml()
{
  Layer newOutputs;
  
  QXmlStreamReader xml(config.artworkXml);
  
  // Init recursive parsing
  addLayer(newOutputs, xml);

  // Assign global outputs to these new outputs
  outputs = newOutputs;
  return true;
}

void Compositor::addLayer(Layer &layer, QXmlStreamReader &xml)
{
  while(xml.readNext() && !xml.atEnd()) {
    Layer newLayer;
    if(xml.isStartElement() && xml.name() == "output") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("type")) {
	newLayer.resource = attribs.value("", "type").toString();
	newLayer.type = T_OUTPUT;
      }
      if(attribs.hasAttribute("width"))
	newLayer.width = attribs.value("", "width").toInt();
      if(attribs.hasAttribute("height"))
	newLayer.height = attribs.value("", "height").toInt();

      if(newLayer.type != T_NONE) {
	addLayer(newLayer, xml);
	layer.layers.append(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "layer") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("resource")) {
	newLayer.resource = attribs.value("", "resource").toString();
	newLayer.type = T_LAYER;
	if(attribs.hasAttribute("width"))
	  newLayer.width = attribs.value("", "width").toInt();
	if(attribs.hasAttribute("height"))
	  newLayer.height = attribs.value("", "height").toInt();
	if(attribs.hasAttribute("align"))
	  newLayer.align = attribs.value("", "align").toString();
	if(attribs.hasAttribute("valign"))
	  newLayer.valign = attribs.value("", "valign").toString();
	if(attribs.hasAttribute("x"))
	  newLayer.x = attribs.value("", "x").toInt();
	if(attribs.hasAttribute("y"))
	  newLayer.y = attribs.value("", "y").toInt();
	addLayer(newLayer, xml);
	layer.layers.append(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "shadow") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("distance") &&
	 attribs.hasAttribute("softness") &&
	 attribs.hasAttribute("opacity")) {
	newLayer.type = T_SHADOW;
	newLayer.shadowDistance = attribs.value("distance").toInt();
	newLayer.shadowSoftness = attribs.value("softness").toInt();
	newLayer.shadowOpacity = attribs.value("opacity").toInt();
	layer.layers.append(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "mask") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("file")) {
	newLayer.type = T_MASK;
	newLayer.resource = attribs.value("file").toString();
	if(attribs.hasAttribute("width"))
	  newLayer.width = attribs.value("", "width").toInt();
	if(attribs.hasAttribute("height"))
	  newLayer.height = attribs.value("", "height").toInt();
	layer.layers.append(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "frame") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("file")) {
	newLayer.type = T_FRAME;
	newLayer.resource = attribs.value("file").toString();
	if(attribs.hasAttribute("width"))
	  newLayer.width = attribs.value("", "width").toInt();
	if(attribs.hasAttribute("height"))
	  newLayer.height = attribs.value("", "height").toInt();
	layer.layers.append(newLayer);
      }
    } else if(xml.isEndElement() && xml.name() == "layer") {
      return;
    } else if(xml.isEndElement() && xml.name() == "output") {
      return;
    }
  }
}

void Compositor::saveAll(GameEntry &game, QString completeBaseName)
{
  foreach(Layer output, outputs.layers) {
    QImage canvas;
    if(output.resource == "cover") {
      canvas = game.coverData;
    } else if(output.resource == "screenshot") {
      canvas = game.screenshotData;
    } else if(output.resource == "wheel") {
      canvas = game.wheelData;
    } else if(output.resource == "marquee") {
      canvas = game.marqueeData;
    }

    canvas = canvas.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    
    if(output.width == -1 && output.height != -1) {
      canvas = canvas.scaledToHeight(output.height, Qt::SmoothTransformation);
    } else if(output.width != -1 && output.height == -1) {
      canvas = canvas.scaledToWidth(output.width, Qt::SmoothTransformation);
    } else if(output.width != -1 && output.height != -1) {
      canvas = canvas.scaled(output.width, output.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if(!output.layers.isEmpty()) {
      // Reset canvas since composite layers exist
      canvas.fill(Qt::transparent);
      // Initiate recursive compositing
      compositeLayer(game, canvas, output);
    }

    if(output.resource == "cover") {
      if(canvas.convertToFormat(QImage::Format_ARGB6666_Premultiplied).save(config.coversFolder + "/" + completeBaseName + ".png"))
	game.coverFile = StrTools::xmlUnescape(config.coversFolder + "/" +
					       completeBaseName + ".png");
    } else if(output.resource == "screenshot") {
      if(canvas.convertToFormat(QImage::Format_ARGB6666_Premultiplied).save(config.screenshotsFolder + "/" + completeBaseName + ".png"))
	game.screenshotFile = StrTools::xmlUnescape(config.screenshotsFolder + "/" +
						    completeBaseName + ".png");
    } else if(output.resource == "wheel") {
      if(canvas.convertToFormat(QImage::Format_ARGB6666_Premultiplied).save(config.wheelsFolder + "/" + completeBaseName + ".png"))
	game.wheelFile = StrTools::xmlUnescape(config.wheelsFolder + "/" +
					       completeBaseName + ".png");
    } else if(output.resource == "marquee") {
      if(canvas.convertToFormat(QImage::Format_ARGB6666_Premultiplied).save(config.marqueesFolder + "/" + completeBaseName + ".png"))
	game.marqueeFile = StrTools::xmlUnescape(config.marqueesFolder + "/" +
						 completeBaseName + ".png");
    }
  }
}

void Compositor::compositeLayer(GameEntry &game, QImage &canvas, Layer &layer)
{
  for(int a = layer.layers.length() - 1; a >= 0 ; --a) {
    QImage thisCanvas;
    Layer thisLayer = layer.layers.at(a);

    if(thisLayer.type == T_LAYER) {
      if(thisLayer.resource == "cover") {
	thisCanvas = game.coverData;
      } else if(thisLayer.resource == "screenshot") {
	thisCanvas = game.screenshotData;
      } else if(thisLayer.resource == "wheel") {
	thisCanvas = game.wheelData;
      } else if(thisLayer.resource == "marquee") {
	thisCanvas = game.marqueeData;
      } else {
	thisCanvas = QImage("resources/" + thisLayer.resource);
      }

      if(thisLayer.width == -1 && thisLayer.height != -1) {
	thisCanvas = thisCanvas.scaledToHeight(thisLayer.height, Qt::SmoothTransformation);
      } else if(thisLayer.width != -1 && thisLayer.height == -1) {
	thisCanvas = thisCanvas.scaledToWidth(thisLayer.width, Qt::SmoothTransformation);
      } else if(thisLayer.width != -1 && thisLayer.height != -1) {
	thisCanvas = thisCanvas.scaled(thisLayer.width, thisLayer.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
      }
      // Update width + height as we will need them for easier placement and alignment
      thisLayer.width = thisCanvas.width();
      thisLayer.height = thisCanvas.height();
      
      if(!thisLayer.layers.isEmpty()) {
	compositeLayer(game, thisCanvas, thisLayer);
      }
    } else if(thisLayer.type == T_SHADOW) {
      FxShadow shadowFx;
      canvas = shadowFx.applyShadow(canvas, thisLayer);
    } else if(thisLayer.type == T_MASK) {
      canvas = applyMask(canvas, thisLayer);
    } else if(thisLayer.type == T_FRAME) {
      canvas = applyFrame(canvas, thisLayer);
    }
    QPainter painter;
    painter.begin(&canvas);
    
    int x = 0;
    if(thisLayer.align == "center") {
      x = (layer.width / 2) - (thisLayer.width / 2);
    } else if(thisLayer.align == "right") {
      x = layer.width - thisLayer.width;
    }
    x += thisLayer.x;
    
    int y = 0;
    if(thisLayer.valign == "middle") {
      y = (layer.height / 2) - (thisLayer.height / 2);
    } else if(thisLayer.valign == "bottom") {
      y = layer.height - thisLayer.height;
    }
    y += thisLayer.y;
    
    painter.drawImage(x, y, thisCanvas);
    painter.end();
  }
}

QImage Compositor::applyMask(QImage &image, Layer &layer)
{
  QString file = layer.resource;

  QImage mask("resources/" + file);
  mask = mask.convertToFormat(QImage::Format_ARGB32_Premultiplied);

  if(layer.width == -1 && layer.height == -1) {
    mask = mask.scaled(image.width(), image.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  } else if(layer.width == -1 && layer.height != -1) {
    mask = mask.scaledToHeight(layer.height, Qt::SmoothTransformation);
  } else if(layer.width != -1 && layer.height == -1) {
    mask = mask.scaledToWidth(layer.width, Qt::SmoothTransformation);
  } else if(layer.width != -1 && layer.height != -1) {
    mask = mask.scaled(layer.width, layer.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }
  
  QPainter painter;
  painter.begin(&image);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationAtop);
  painter.drawImage(0, 0, mask);
  painter.end();

  return image;
}

QImage Compositor::applyFrame(QImage &image, Layer &layer)
{
  QString file = layer.resource;

  QImage frame("resources/" + file);
  frame = frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);

  if(layer.width == -1 && layer.height == -1) {
    frame = frame.scaled(image.width(), image.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  } else if(layer.width == -1 && layer.height != -1) {
    frame = frame.scaledToHeight(layer.height, Qt::SmoothTransformation);
  } else if(layer.width != -1 && layer.height == -1) {
    frame = frame.scaledToWidth(layer.width, Qt::SmoothTransformation);
  } else if(layer.width != -1 && layer.height != -1) {
    frame = frame.scaled(layer.width, layer.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }
  
  QPainter painter;
  painter.begin(&image);
  painter.drawImage(0, 0, frame);
  painter.end();

  return image;
}
