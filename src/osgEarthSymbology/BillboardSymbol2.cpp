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
#include <osgEarthSymbology/BillboardSymbol2>
#include <osgEarthSymbology/Style>

using namespace osgEarth;
using namespace osgEarth::Symbology;

OSGEARTH_REGISTER_SIMPLE_SYMBOL(billboard2, BillboardSymbol2);

BillboardSymbol2::BillboardSymbol2(const BillboardSymbol2& rhs,const osg::CopyOp& copyop):
Symbol(rhs, copyop),
_width(rhs._width),
_height(rhs._height),
_density(rhs._density),
_scaleVariance(rhs._scaleVariance),
_brightnessVariance(rhs._brightnessVariance),
_randomSeed(rhs._randomSeed),
_alphaRefValue(rhs._alphaRefValue),
_alphaBlend(rhs._alphaBlend),
_imageURI(rhs._imageURI)
{
}

BillboardSymbol2::BillboardSymbol2( const Config& conf ) :
Symbol( conf ),
_width ( 2.0f ),
_height( 5.0f ),
_density( 20.0f ),
_scaleVariance(0.3),
_brightnessVariance(0.3),
_randomSeed(0),
_alphaRefValue(0.5),
_alphaBlend(true)
{
    mergeConfig(conf);
}

Config 
BillboardSymbol2::getConfig() const
{
    Config conf = Symbol::getConfig();

	conf.key() = "billboard2";
	conf.updateIfSet( "billboard2-image", _imageURI );
	conf.updateIfSet( "billboard2-width", _width );
	conf.updateIfSet( "billboard2-height", _height );
	conf.updateIfSet( "billboard2-density", _density );
	conf.updateIfSet( "billboard2-scaleVariance", _scaleVariance);
	conf.updateIfSet( "billboard2-brightnessVariance", _brightnessVariance);
	conf.updateIfSet( "billboard2-randomSeed", _randomSeed );
	conf.updateIfSet( "billboard2-alphaRefValue", _alphaRefValue );
	conf.updateIfSet( "billboard2-alphaBlend", _alphaBlend);
	
    return conf;
}

void 
BillboardSymbol2::mergeConfig( const Config& conf )
{
      conf.getIfSet( "billboard2-image", _imageURI );
	  conf.getIfSet( "billboard2-width", _width );
	  conf.getIfSet( "billboard2-height", _height );
	  conf.getIfSet( "billboard2-density", _density );
	  conf.getIfSet( "billboard2-scaleVariance", _scaleVariance);
	  conf.getIfSet( "billboard2-brightnessVariance", _brightnessVariance);
	  conf.getIfSet( "billboard2-randomSeed", _randomSeed );
	  conf.getIfSet( "billboard2-alphaRefValue", _alphaRefValue );
	  conf.getIfSet( "billboard2-alphaBlend", _alphaBlend);
}

void
BillboardSymbol2::parseSLD(const Config& c, Style& style)
{
	if ( match(c.key(), "billboard2-image") ) {
		style.getOrCreate<BillboardSymbol2>()->imageURI() = URI(c.value(),c.referrer());
		//style.getOrCreate<BillboardSymbol2>()->imageURI()->setURIContext( c.referrer() );
	}   
    else if ( match(c.key(), "billboard2-width") ) {
        style.getOrCreate<BillboardSymbol2>()->width() = as<float>(c.value(), 1.0f);
    }
	else if ( match(c.key(), "billboard2-height") ) {
		style.getOrCreate<BillboardSymbol2>()->height() = as<float>(c.value(), 1.0f);
	}
	else if ( match(c.key(), "billboard2-density") ) {
		style.getOrCreate<BillboardSymbol2>()->density() = as<float>(c.value(), 1.0f);
	}
	else if ( match(c.key(), "billboard2-scaleVariance") ) {
		style.getOrCreate<BillboardSymbol2>()->scaleVariance() = as<float>(c.value(), 0.0f);
	}
	else if ( match(c.key(), "billboard2-brightnessVariance") ) {
		style.getOrCreate<BillboardSymbol2>()->brightnessVariance() = as<float>(c.value(), 0.0f);
	}
	else if ( match(c.key(), "billboard2-randomSeed") ) {
		style.getOrCreate<BillboardSymbol2>()->randomSeed() = as<unsigned>(c.value(), 0);
	}
	else if ( match(c.key(), "billboard2-alphaRefValue") ) {
		style.getOrCreate<BillboardSymbol2>()->alphaRefValue() = as<float>(c.value(), 0);
	}
	else if ( match(c.key(), "billboard2-alphaBlend") ) {
		style.getOrCreate<BillboardSymbol2>()->alphaBlend() = as<bool>(c.value(), 0);
	}
}