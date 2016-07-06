/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2015 Pelican Mapping
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
#include <SilverLining.h>

#include "SilverLiningContextNode"
#include "SilverLiningNode"
#include "SilverLiningContext"
#include "SilverLiningSkyDrawable"
#include "SilverLiningCloudsDrawable"

#include <osgEarth/MapNode>

#include <osg/Light>
#include <osg/LightSource>
#include <osgEarth/CullingUtils>
#include <osg/Depth>
#include <osg/Fog>
#undef  LC
#define LC "[SilverLiningContextNode] "

using namespace osgEarth::SilverLining;

SilverLiningContextNode::SilverLiningContextNode(SilverLiningNode* node,
												osg::Light* light, 
												const osgEarth::MapNode*       map,
												const SilverLiningOptions& options):
_silverLiningNode(node),
_options     (options),
_lastAltitude(DBL_MAX),
_map(map)
{
#ifdef SL_USE_MUTEX
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock( _mutex );
#endif
    // The main silver lining data:
    _SL = new SilverLiningContext( options );
    _SL->setLight( light);
    _SL->setSRS  ( map->getMap()->getSRS() );
	
    // Geode to hold each of the SL drawables:
    _geode = new osg::Geode();
    _geode->setCullingActive( false );

    // Draws the sky before everything else
    _skyDrawable = new SkyDrawable( this,_SL.get() );
    _skyDrawable->getOrCreateStateSet()->setRenderBinDetails( -99, "RenderBin" );
    _geode->addDrawable( _skyDrawable );

    // Clouds draw after everything else
	if(options.drawClouds().get())
	{
		_cloudsDrawable = new CloudsDrawable( this,_SL.get() );
		char* render_bin = ::getenv("OSGEARTH_SL_CLOUDS_RB");
		if(render_bin)
		{
			int render_bin_num = atoi(render_bin);
			_cloudsDrawable->getOrCreateStateSet()->setRenderBinDetails( render_bin_num, "DepthSortedBin" );
		}
		else
			_cloudsDrawable->getOrCreateStateSet()->setRenderBinDetails( 11, "DepthSortedBin" );
		 _geode->addDrawable( _cloudsDrawable.get() );
	}

	_updateEnvMap = false;
	if(options.updateEnvMap().isSet())
	{
		_updateEnvMap = options.updateEnvMap().get();
	}
	_SL->setUpdateEnvMap(_updateEnvMap);
    
    // SL requires an update pass.
    ADJUST_UPDATE_TRAV_COUNT(this, +1);

	// initialize date/time
	onSetDateTime();
}

SilverLiningContextNode::~SilverLiningContextNode()
{

}

void
	SilverLiningContextNode::onSetDateTime()
{
	// set the SL local time to UTC/epoch.
	::SilverLining::LocalTime utcTime;
	utcTime.SetFromEpochSeconds( _silverLiningNode->getDateTime().asTimeStamp() );
	_SL->getAtmosphere()->GetConditions()->SetTime( utcTime );
}

void
	SilverLiningContextNode::onSetMinimumAmbient()
{
	_SL->setMinimumAmbient( _silverLiningNode->getMinimumAmbient() );
}


int SilverLiningContextNode::getEnvMapTextureID() 
{ 
	int id = 0;
	if(_updateEnvMap)
	{
		id = _SL->getEnvMapID();
		//force new update
		_SL->setUpdateEnvMap(true);
	}
	return id;
}

void
SilverLiningContextNode::traverse(osg::NodeVisitor& nv)
{
    if ( _SL && _SL->ready() )
    {
        if ( nv.getVisitorType() == nv.UPDATE_VISITOR )
        {
			int frameNumber = nv.getFrameStamp()->getFrameNumber();
            _skyDrawable->dirtyBound();
			
            if( _cloudsDrawable )
            {
				_cloudsDrawable->dirtyBound();
			   /*if ( _lastAltitude <= *_options.cloudsMaxAltitude() )
                {
                    if ( _cloudsDrawable->getNumParents() == 0 )
					{
						//recreate clouds
						//_SL->_setupClouds = true;
                        _geode->addDrawable( _cloudsDrawable.get() );
					}
					_cloudsDrawable->dirtyBound();
                }
                else
                {
                    if ( _cloudsDrawable->getNumParents() > 0 )
                        _geode->removeDrawable( _cloudsDrawable.get() );
                }*/
            }
        }

        else if ( nv.getVisitorType() == nv.CULL_VISITOR )
        {
            osgUtil::CullVisitor* cv = Culling::asCullVisitor(nv);
			osg::Camera* camera  = cv->getCurrentCamera();
			if ( camera )
			{
				SilverLiningContextNode *sky_node = dynamic_cast<SilverLiningContextNode *>(camera->getUserData());
				if(sky_node == this)
				{
#ifdef SL_USE_MUTEX
					OpenThreads::ScopedLock<OpenThreads::Mutex> lock( _mutex );
#endif
					_SL->setCamera(camera);
					_SL->setCameraPosition( nv.getEyePoint() );
					_SL->getAtmosphere()->SetCameraMatrix( cv->getModelViewMatrix()->ptr() );
					_SL->getAtmosphere()->SetProjectionMatrix( cv->getProjectionMatrix()->ptr() );

					_lastAltitude = _SL->getSRS()->isGeographic() ?
						cv->getEyePoint().length() - _SL->getSRS()->getEllipsoid()->getRadiusEquator() :
					cv->getEyePoint().z();

					_SL->updateLocation();
					_SL->updateLight();

					//_SL->getAtmosphere()->UpdateSkyAndClouds();
					//_SL->getAtmosphere()->CullObjects();


					//update fog
					
					if(_map)
					{
						osg::Fog* fog = (osg::Fog *) _map->getStateSet()->getAttribute(osg::StateAttribute::FOG);

						if (fog)
						{
							float hazeDensity = 1.0 / 100000;

							// Decrease fog density with altitude, to avoid fog effects through the vacuum of space.
							static const double H = 8435.0; // Pressure scale height of Earth's atmosphere
							double isothermalEffect = exp(-(_SL->getAtmosphere()->GetConditions()->GetLocation().GetAltitude() / H));
							if (isothermalEffect <= 0) isothermalEffect = 1E-9;
							if (isothermalEffect > 1.0) isothermalEffect = 1.0;
							hazeDensity *= isothermalEffect;

							float r, g, b;
							
							// Note, the fog color returned is already lit
							//float density;
							//_SL->getAtmosphere()->GetFogSettings(&density, &r, &g, &b);
							_SL->getAtmosphere()->GetHorizonColor(0, 0, &r, &g, &b);
							fog->setColor(osg::Vec4(r, g, b, 1.0));
							fog->setDensity(hazeDensity);
						}
					}
				}
			}
        }
    }
   
    if ( _geode.valid() )
    {
        _geode->accept(nv);
    }
}