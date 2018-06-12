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
#include "SilverLiningDrawable"


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
    _skyDrawable->getOrCreateStateSet()->setRenderBinDetails(-99, "RenderBin");
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

    Threading::ScopedMutexLock excl(_contextMapMutex);
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
    Threading::ScopedMutexLock excl(_contextMapMutex);
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
    ContextKey key = renderInfo.getCurrentCamera();
    ContextMap::iterator iter = _contextMap.find(key);
    if (iter != _contextMap.end())
    {
        return iter->second;
    }
    //Create context
    osg::ref_ptr<SilverLiningContext>  context = new SilverLiningContext(_options,  _mapSRS, _callback);
    if (context->initialize(renderInfo))
    {
        //set min ambient
        context->setMinimumAmbient(getMinimumAmbient());
        //Set current time
        ::SilverLining::LocalTime utcTime;
        utcTime.SetFromEpochSeconds(getDateTime().asTimeStamp());
        context->getAtmosphere()->GetConditions()->SetTime(utcTime);
    }
    _contextMap[key] = context;
    return context;
}

SilverLiningContext* SilverLiningNode::findContext(ContextKey key)
{
    Threading::ScopedMutexLock excl(_contextMapMutex);
    ContextMap::const_iterator iter = _contextMap.find(key);
    SilverLiningContext* context = NULL;
    if (iter != _contextMap.end())
    {
        context = iter->second;
    }
    return context;
}


void
SilverLiningNode::traverse(osg::NodeVisitor& nv)
{
    if ( nv.getVisitorType() == nv.CULL_VISITOR )
    {
        osgUtil::CullVisitor* cv = Culling::asCullVisitor(nv);

        osg::Camera* camera = cv->getCurrentCamera();
        if (camera)
        {
            //Get SL-context for current camera.
            //Note: context's are created in first draw call so
            //we don't expect to find a context in the first frame.
            SilverLiningContext* context = findContext(camera);
            if(context)
            {
                //reflect SilverLining light conditions to our global scene lighting.
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
        //update bounds for current camera
		if (nv.getVisitorType() == nv.CULL_VISITOR)
		{
			osgUtil::CullVisitor* cv = Culling::asCullVisitor(nv);
			osg::Camera* camera = cv->getCurrentCamera();
			if (camera)
			{
				SilverLiningContext* context = findContext(camera);
				if (context)
				{
					Threading::ScopedMutexLock excl(_travereMutex);
					SkyDrawable* sd = dynamic_cast<SkyDrawable*>(_skyDrawable.get());
					sd->updateBounds(camera, context);
					CloudsDrawable* cd = dynamic_cast<CloudsDrawable*>(_cloudsDrawable.get());
					cd->updateBounds(camera, context);
					_geode->accept(nv);
				}
				else
					_geode->accept(nv);
			}
		}
		else 
         _geode->accept(nv);
    }

    
    {
        //Lock to avoid race condition in HorizonClipPlane::operator() setDefine("OE_CLIPPLANE_NUM"...)
        Threading::ScopedMutexLock excl(_travereMutex);
        osgEarth::Util::SkyNode::traverse(nv);
    }
}
