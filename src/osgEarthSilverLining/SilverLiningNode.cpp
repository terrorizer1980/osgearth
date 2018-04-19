/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2016 Pelican Mapping
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

#include "SilverLiningNode"
#include "SilverLiningContext"
#include "SilverLiningSkyDrawable"
#include "SilverLiningCloudsDrawable"

#include <osg/Light>
#include <osg/LightSource>
#include <osg/Depth>

#include <osgEarth/CullingUtils>
#include <osgEarth/Lighting>
#include <osgEarth/NodeUtils>

#undef  LC
#define LC "[SilverLiningNode] "

using namespace osgEarth::SilverLining;

SilverLiningNode::SilverLiningNode(const osgEarth::SpatialReference*    mapSRS,
                                   const SilverLiningOptions& options,
                                   Callback*                  callback) :
_options(options),
_mapSRS(mapSRS),
_callback(callback)
{
    // Create a new Light for the Sun.
    _light = new LightGL3(0);
    _light->setDiffuse( osg::Vec4(1,1,1,1) );
    _light->setAmbient( osg::Vec4(0.2f, 0.2f, 0.2f, 1) );
    _light->setPosition( osg::Vec4(1, 0, 0, 0) ); // w=0 means infinity
    _light->setDirection( osg::Vec3(-1,0,0) );

    _lightSource = new osg::LightSource();
    _lightSource->setLight( _light.get() );
    _lightSource->setReferenceFrame(osg::LightSource::RELATIVE_RF);
    GenerateGL3LightingUniforms generateLightingVisitor;
    _lightSource->accept(generateLightingVisitor);

    // scene lighting
    osg::StateSet* stateset = this->getOrCreateStateSet();
    _lighting = new PhongLightingEffect();
    //_lighting->setCreateLightingUniform( false );
    _lighting->attach( stateset );

	// Geode to hold each of the SL drawables:
	_geode = new osg::Geode();
	_geode->setCullingActive(false);

	// Draws the sky before everything else
	_skyDrawable = new SkyDrawable(this);
	//_skyDrawable->getOrCreateStateSet()->setRenderBinDetails(-99, "RenderBin");
	_skyDrawable->getOrCreateStateSet()->setRenderBinDetails(99, "RenderBin");
	_skyDrawable->getOrCreateStateSet()->setAttributeAndModes(new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false));
	//_skyDrawable->getOrCreateStateSet()->setRenderBinDetails(-99, "RenderBin");
	_geode->addDrawable(_skyDrawable.get());

	// Clouds draw after everything else
	_cloudsDrawable = new CloudsDrawable(this);
	_cloudsDrawable->getOrCreateStateSet()->setRenderBinDetails(99, "DepthSortedBin");
	 _geode->addDrawable(_cloudsDrawable.get());


    // need update traversal.
    ADJUST_UPDATE_TRAV_COUNT(this, +1);
}


SilverLiningNode::~SilverLiningNode()
{
    if ( _lighting.valid() )
        _lighting->detach();
}

void
SilverLiningNode::attach(osg::View* view, int lightNum)
{
    _light->setLightNum( lightNum );
    view->setLight( _light.get() );
    //view->setLightingMode( osg::View::SKY_LIGHT );
    view->setLightingMode(osg::View::NO_LIGHT);
}

void
SilverLiningNode::onSetDateTime()
{
	::SilverLining::LocalTime utcTime;
	utcTime.SetFromEpochSeconds(getDateTime().asTimeStamp());

	ContextMap::iterator iter = _contextMap.begin();
	while(iter != _contextMap.end())
	{
		iter->second->getAtmosphere()->GetConditions()->SetTime(utcTime);
		iter++;
	}
}

void
SilverLiningNode::onSetMinimumAmbient()
{
	ContextMap::iterator iter = _contextMap.begin();
	while(iter != _contextMap.end())
	{
		iter->second->setMinimumAmbient(getMinimumAmbient());
		iter++;
	}
}

osg::ref_ptr<SilverLiningContext> SilverLiningNode::getOrCreateContext(osg::RenderInfo& renderInfo)
{
	Threading::ScopedMutexLock excl(_contextMapMutex);
	ContextKey key = getKey(renderInfo);
	ContextMap::iterator iter = _contextMap.find(key);
	if (iter != _contextMap.end())
	{
		return iter->second;
	}
	//Create context
	osg::ref_ptr<SilverLiningContext>  context = new SilverLiningContext(_options,  _mapSRS, _callback);
	//context->setMinimumAmbient(getMinimumAmbient());
	_contextMap[key] = context;
	return context;
}


void
SilverLiningNode::traverse(osg::NodeVisitor& nv)
{
    static Threading::Mutex s_mutex;

    if ( nv.getVisitorType() == nv.CULL_VISITOR )
    {
        osgUtil::CullVisitor* cv = Culling::asCullVisitor(nv);

		osg::Camera* camera = cv->getCurrentCamera();
		if (camera)
		{
			//osg::Camera* key = camera;
#ifdef  SL_CONTEXT_PER_CAMERA
			ContextKey key = camera;
#else
			ContextKey key = camera->getGraphicsContext()->getState()->getContextID();
#endif

			ContextMap::const_iterator iter = _contextMap.find(key);
			if (iter != _contextMap.end())
			{
				SilverLiningContext* context = iter->second;
				// TODO: make this multi-camera safe
				osgUtil::CullVisitor* cv = Culling::asCullVisitor(nv);
				context->updateLight(_light);
			}
		}
    }

    else if (nv.getVisitorType() == nv.UPDATE_VISITOR)
    {
		int frameNumber = nv.getFrameStamp()->getFrameNumber();
		_skyDrawable->dirtyBound();
    }

    if ( _lightSource.valid() )
    {
        _lightSource->accept(nv);
    }

	if (_geode.valid())
	{
		_geode->accept(nv);
	}

    osgEarth::Util::SkyNode::traverse(nv);
}
