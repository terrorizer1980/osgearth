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

#include "SilverLiningSkyNode"
#include "SilverLiningContext"
#include "SilverLiningSkyDrawable"
#include "SilverLiningCloudsDrawable"

#include <osg/Light>
#include <osg/LightSource>
#include <osgEarth/CullingUtils>
#include <osg/Depth>
#undef  LC
#define LC "[SilverLiningSkyNode] "

#define USE_ENV_MAP

using namespace osgEarth::SilverLining;

SilverLiningSkyNode::SilverLiningSkyNode(osg::Light* light, const osgEarth::Map*       map,
                                   const SilverLiningOptions& options):
_options     (options),
_lastAltitude(DBL_MAX)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock( _mutex );
    // The main silver lining data:
    _SL = new SilverLiningContext( options );
    _SL->setLight( light);
    _SL->setSRS  ( map->getSRS() );

    // Geode to hold each of the SL drawables:
    _geode = new osg::Geode();
    _geode->setCullingActive( false );

    // Draws the sky before everything else
    _skyDrawable = new SkyDrawable( this,_SL.get() );
   _skyDrawable->getOrCreateStateSet()->setRenderBinDetails( -99, "RenderBin" );
   //_skyDrawable->getOrCreateStateSet()->setRenderBinDetails( 91, "RenderBin" );
   //_skyDrawable->getOrCreateStateSet()->setAttributeAndModes( new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false) );
    _geode->addDrawable( _skyDrawable );

    // Clouds draw after everything else

	if(options.drawClouds().get())
	{
		_cloudsDrawable = new CloudsDrawable( this,_SL.get() );
		//_cloudsDrawable->getOrCreateStateSet()->setRenderingHint( osg::StateSet::TRANSPARENT_BIN );
		_cloudsDrawable->getOrCreateStateSet()->setRenderBinDetails( 17, "RenderBin" );
		//_cloudsDrawable->setCullCallback( new AlwaysKeepCallback );  // This seems to avoid cloud to twinkle sometimes
		//_cloudsDrawable->getOrCreateStateSet()->setRenderBinDetails( 99, "DepthSortedBin" );
		//_geode->addDrawable( _cloudsDrawable );
	}
	
    // ensure it's depth sorted and draws after the terrain
    //stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    //getOrCreateStateSet()->setRenderBinDetails( 100, "DepthSortedBin" );

    // SL requires an update pass.
    ADJUST_UPDATE_TRAV_COUNT(this, +1);

	_updateEnvMap = false;
	if(options.updateEnvMap().isSet())
	{
		_updateEnvMap = options.updateEnvMap().get();
	}
	_SL->setUpdateEnvMap(_updateEnvMap);
}

SilverLiningSkyNode::~SilverLiningSkyNode()
{

}

/*int SilverLiningSkyNode::getEnvMapID() const 
{
	if(_updateEnvMap)
		return _SL->getEnvMapID();
	else
		return 0;
}*/

void
SilverLiningSkyNode::traverse(osg::NodeVisitor& nv)
{
    if ( _SL && _SL->ready() )
    {
        if ( nv.getVisitorType() == nv.UPDATE_VISITOR )
        {
			int frameNumber = nv.getFrameStamp()->getFrameNumber();
            _skyDrawable->dirtyBound();
			
            if( _cloudsDrawable )
            {
				//_cloudsDrawable->dirtyBound();
			    if ( _lastAltitude <= *_options.cloudsMaxAltitude() )
                {
                    if ( _cloudsDrawable->getNumParents() == 0 )
					{
                        _geode->addDrawable( _cloudsDrawable.get() );
					}
					_cloudsDrawable->dirtyBound();
                }
                else
                {
                    if ( _cloudsDrawable->getNumParents() > 0 )
                        _geode->removeDrawable( _cloudsDrawable.get() );
                }
            }
        }

        else if ( nv.getVisitorType() == nv.CULL_VISITOR )
        {

            // TODO: make this multi-camera safe
            osgUtil::CullVisitor* cv = Culling::asCullVisitor(nv);
			osg::Camera* camera  = cv->getCurrentCamera();
			if ( camera )
			{
				SilverLiningSkyNode *sky_node = dynamic_cast<SilverLiningSkyNode *>(camera->getUserData());
				if(sky_node == this)
				{
					OpenThreads::ScopedLock<OpenThreads::Mutex> lock( _mutex );
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
				}
			}
        }
    }
   
    if ( _geode.valid() )
    {
        _geode->accept(nv);
    }
}
