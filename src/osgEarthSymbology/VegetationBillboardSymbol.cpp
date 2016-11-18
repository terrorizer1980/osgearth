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
#include <osgEarthSymbology/VegetationBillboardSymbol>
#include <osgEarthSymbology/Style>

using namespace osgEarth;
using namespace osgEarth::Symbology;

OSGEARTH_REGISTER_SIMPLE_SYMBOL(vegetationbillboard, VegetationBillboardSymbol);

VegetationBillboardSymbol::VegetationBillboardSymbol(const VegetationBillboardSymbol& rhs,const osg::CopyOp& copyop):
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

VegetationBillboardSymbol::VegetationBillboardSymbol( const Config& conf ) :
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
VegetationBillboardSymbol::getConfig() const
{
    Config conf = Symbol::getConfig();

	conf.key() = "vegetationbillboard";
	conf.updateIfSet( "vegetationbillboard-image", _imageURI );
	conf.updateIfSet( "vegetationbillboard-width", _width );
	conf.updateIfSet( "vegetationbillboard-height", _height );
	conf.updateIfSet( "vegetationbillboard-density", _density );
	conf.updateIfSet( "vegetationbillboard-scaleVariance", _scaleVariance);
	conf.updateIfSet( "vegetationbillboard-brightnessVariance", _brightnessVariance);
	conf.updateIfSet( "vegetationbillboard-randomSeed", _randomSeed );
	conf.updateIfSet( "vegetationbillboard-alphaRefValue", _alphaRefValue );
	conf.updateIfSet( "vegetationbillboard-alphaBlend", _alphaBlend);
	
    return conf;
}

void 
VegetationBillboardSymbol::mergeConfig( const Config& conf )
{
      conf.getIfSet( "vegetationbillboard-image", _imageURI );
	  conf.getIfSet( "vegetationbillboard-width", _width );
	  conf.getIfSet( "vegetationbillboard-height", _height );
	  conf.getIfSet( "vegetationbillboard-density", _density );
	  conf.getIfSet( "vegetationbillboard-scaleVariance", _scaleVariance);
	  conf.getIfSet( "vegetationbillboard-brightnessVariance", _brightnessVariance);
	  conf.getIfSet( "vegetationbillboard-randomSeed", _randomSeed );
	  conf.getIfSet( "vegetationbillboard-alphaRefValue", _alphaRefValue );
	  conf.getIfSet( "vegetationbillboard-alphaBlend", _alphaBlend);
}

void
VegetationBillboardSymbol::parseSLD(const Config& c, Style& style)
{
	if ( match(c.key(), "vegetationbillboard-image") ) {
		style.getOrCreate<VegetationBillboardSymbol>()->imageURI() = URI(c.value(),c.referrer());
		//style.getOrCreate<VegetationBillboardSymbol>()->imageURI()->setURIContext( c.referrer() );
	}   
    else if ( match(c.key(), "vegetationbillboard-width") ) {
        style.getOrCreate<VegetationBillboardSymbol>()->width() = as<float>(c.value(), 1.0f);
    }
	else if ( match(c.key(), "vegetationbillboard-height") ) {
		style.getOrCreate<VegetationBillboardSymbol>()->height() = as<float>(c.value(), 1.0f);
	}
	else if ( match(c.key(), "vegetationbillboard-density") ) {
		style.getOrCreate<VegetationBillboardSymbol>()->density() = as<float>(c.value(), 1.0f);
	}
	else if ( match(c.key(), "vegetationbillboard-scaleVariance") ) {
		style.getOrCreate<VegetationBillboardSymbol>()->scaleVariance() = as<float>(c.value(), 0.0f);
	}
	else if ( match(c.key(), "vegetationbillboard-brightnessVariance") ) {
		style.getOrCreate<VegetationBillboardSymbol>()->brightnessVariance() = as<float>(c.value(), 0.0f);
	}
	else if ( match(c.key(), "vegetationbillboard-randomSeed") ) {
		style.getOrCreate<VegetationBillboardSymbol>()->randomSeed() = as<unsigned>(c.value(), 0);
	}
	else if ( match(c.key(), "vegetationbillboard-alphaRefValue") ) {
		style.getOrCreate<VegetationBillboardSymbol>()->alphaRefValue() = as<float>(c.value(), 0);
	}
	else if ( match(c.key(), "vegetationbillboard-alphaBlend") ) {
		style.getOrCreate<VegetationBillboardSymbol>()->alphaBlend() = as<bool>(c.value(), 0);
	}
}