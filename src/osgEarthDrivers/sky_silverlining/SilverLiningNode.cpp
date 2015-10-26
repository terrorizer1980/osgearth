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

#include "SilverLiningNode"
#include "SilverLiningContextNode"

#include "SilverLiningContext"
#include "SilverLiningSkyDrawable"
#include "SilverLiningCloudsDrawable"

#include <osg/Light>
#include <osg/LightSource>
#include <osgEarth/CullingUtils>

#undef  LC
#define LC "[SilverLiningNode] "

using namespace osgEarth::SilverLining;

SilverLiningNode::SilverLiningNode(const osgEarth::Map*       map,
	const SilverLiningOptions& options) : SkyNode(options),
	_options     (options),
	//_lastAltitude(DBL_MAX),
	_map(map)
{
	// Create a new Light for the Sun.
	_light = new osg::Light();
	_light->setLightNum( 0 );
	_light->setDiffuse( osg::Vec4(1,1,1,1) );
	_light->setAmbient( osg::Vec4(0.2f, 0.2f, 0.2f, 1) );
	_light->setPosition( osg::Vec4(1, 0, 0, 0) ); // w=0 means infinity
	_light->setDirection( osg::Vec3(-1,0,0) );

	_lightSource = new osg::LightSource();
	_lightSource->setLight( _light.get() );
	_lightSource->setReferenceFrame(osg::LightSource::RELATIVE_RF);

	// scene lighting
	osg::StateSet* stateset = this->getOrCreateStateSet();
	_lighting = new PhongLightingEffect();
	_lighting->setCreateLightingUniform( false );
	_lighting->attach( stateset );


	//::srand(1234);

	// initialize date/time
	//onSetDateTime();
	//onSetMinimumAmbient();
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
	view->setLightingMode( osg::View::SKY_LIGHT );
}

void
	SilverLiningNode::onSetDateTime()
{
	// set the SL local time to UTC/epoch.
	::SilverLining::LocalTime utcTime;
	utcTime.SetFromEpochSeconds( getDateTime().asTimeStamp() );
	//   _SL->getAtmosphere()->GetConditions()->SetTime( utcTime );
	//	_SL->setUpdateEnvMap(_updateEnvMap);

	/*for(size_t i = 0; i < _contextNodes.size(); i++)
	{
		_contextNodes[i]->getContext()->getAtmosphere()->GetConditions()->SetTime( utcTime );
		//_contextNodes[i]->getContext()->setUpdateEnvMap(_updateEnvMap);
	}*/
}

void
	SilverLiningNode::onSetMinimumAmbient()
{
	/*for(size_t i = 0; i < _contextNodes.size(); i++)
	{
		_contextNodes[i]->getContext()->setMinimumAmbient( getMinimumAmbient() );
	}*/
	//  _SL->setMinimumAmbient( getMinimumAmbient() );
}

int SilverLiningNode::getEnvMapID() const 
{
	//	if(_updateEnvMap)
	//		return _SL->getEnvMapID();
	//	else
	return 0;
}

void
	SilverLiningNode::traverse(osg::NodeVisitor& nv)
{
	if ( nv.getVisitorType() == nv.UPDATE_VISITOR )
	{
		//for(size_t i =0 ; i< _contextNodes.size(); i++)
		//	_contextNodes[i]->traverse(nv);
		for (osg::NodeList::iterator itr = _children.begin();
		itr != _children.end();
			++itr)
		{
			(*itr)->accept(nv);
		}
	}
	else	if ( nv.getVisitorType() == nv.CULL_VISITOR )
	{
		osgUtil::CullVisitor* cv = Culling::asCullVisitor(nv);

		osg::Camera* camera  = cv->getCurrentCamera();
		if ( camera )
		{
			SilverLiningContextNode *sky_node = dynamic_cast<SilverLiningContextNode *>(camera->getUserData());
			if (!sky_node) 
			{
				static bool first_camera = true;
				//if(_contextNodes.size() == 0)
				if (first_camera)
				{
					sky_node = new SilverLiningContextNode(_light.get(), _map, _options);
					first_camera = false;
				}
				else
					sky_node = new SilverLiningContextNode(NULL,_map,_options);

				//_contextNodes.push_back(sky_node);

				static int nodeMask = 0x1;
				sky_node->_geode->setNodeMask(nodeMask);
				camera->setNodeMask(nodeMask);
				nodeMask = nodeMask << 1;
				camera->setUserData(sky_node);
				addChild(sky_node);
			}

			::SilverLining::LocalTime utcTime;
			utcTime.SetFromEpochSeconds(getDateTime().asTimeStamp());
			sky_node->getContext()->getAtmosphere()->GetConditions()->SetTime(utcTime);
			sky_node->getContext()->setMinimumAmbient(getMinimumAmbient());

			
		}
	}
	osgEarth::Util::SkyNode::traverse( nv );


	if ( _lightSource.valid() )
	{
		_lightSource->accept(nv);
	}
}
