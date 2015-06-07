/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2014 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include <osgEarthSymbology/BillboardSymbol>
#include <osgEarthSymbology/Style>

using namespace osgEarth;
using namespace osgEarth::Symbology;

OSGEARTH_REGISTER_SIMPLE_SYMBOL(billboard, BillboardSymbol);

BillboardSymbol::BillboardSymbol(const BillboardSymbol& rhs,const osg::CopyOp& copyop):
Symbol(rhs, copyop),
_width(rhs._width),
_height(rhs._height),
_density(rhs._density),
_imageURI(rhs._imageURI)
{
}

BillboardSymbol::BillboardSymbol( const Config& conf ) :
Symbol( conf ),
_width ( 2.0f ),
_height( 5.0f ),
_density( 20.0f )
{
    mergeConfig(conf);
}

Config 
BillboardSymbol::getConfig() const
{
    Config conf = Symbol::getConfig();

	conf.key() = "billboard";
	conf.updateIfSet( "billboard-image", _imageURI );
	conf.updateIfSet( "billboard-width", _width );
	conf.updateIfSet( "billboard-height", _height );
	conf.updateIfSet( "billboard-density", _density );
  
    return conf;
}

void 
BillboardSymbol::mergeConfig( const Config& conf )
{
      conf.getIfSet( "billboard-image", _imageURI );
	  conf.getIfSet( "billboard-width", _width );
	  conf.getIfSet( "billboard-height", _height );
	  conf.getIfSet( "billboard-density", _density );
}

void
BillboardSymbol::parseSLD(const Config& c, Style& style)
{
	if ( match(c.key(), "billboard-image") ) {
		style.getOrCreate<BillboardSymbol>()->imageURI() = c.value();
	}   
    else if ( match(c.key(), "billboard-width") ) {
        style.getOrCreate<BillboardSymbol>()->width() = as<float>(c.value(), 1.0f);
    }
	else if ( match(c.key(), "billboard-height") ) {
		style.getOrCreate<BillboardSymbol>()->height() = as<float>(c.value(), 1.0f);
	}
	else if ( match(c.key(), "billboard-density") ) {
		style.getOrCreate<BillboardSymbol>()->density() = as<float>(c.value(), 1.0f);
	}
}

