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
#include "SilverLiningCloudsDrawable"
#include "SilverLiningContextNode"
#include "SilverLiningContext"
#include <osgEarth/SpatialReference>

#undef  LC
#define LC "[SilverLining:CloudsDrawable] "

using namespace osgEarth::SilverLining;


CloudsDrawable::CloudsDrawable(SilverLiningContextNode *context_node, SilverLiningContext* SL) :
_SL( SL ),
	_contextNode(context_node)
{
	// call this to ensure draw() gets called every frame.
	setSupportsDisplayList( false );

	// not MT-safe (camera updates, etc)
	this->setDataVariance(osg::Object::DYNAMIC);
}

void
	CloudsDrawable::drawImplementation(osg::RenderInfo& renderInfo) const
{
	if( _SL->ready() )
	{
		osg::Camera* camera = renderInfo.getCurrentCamera();
		if ( camera && _contextNode == dynamic_cast<SilverLiningContextNode *>(camera->getUserData()))
		{
			#ifdef SL_USE_MUTEX
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock( _contextNode->_mutex );
			#endif
			osg::State* state = renderInfo.getState();

			osgEarth::NativeProgramAdapterCollection& adapters = _adapters[ state->getContextID() ]; // thread safe.
			if ( adapters.empty() )
			{
				adapters.push_back( new osgEarth::NativeProgramAdapter(state, _SL->getAtmosphere()->GetSkyShader()) );
				adapters.push_back( new osgEarth::NativeProgramAdapter(state, _SL->getAtmosphere()->GetBillboardShader()) );
				adapters.push_back( new osgEarth::NativeProgramAdapter(state, _SL->getAtmosphere()->GetStarShader()) );
				adapters.push_back( new osgEarth::NativeProgramAdapter(state, _SL->getAtmosphere()->GetPrecipitationShader()) );

				SL_VECTOR(unsigned) handles = _SL->getAtmosphere()->GetActivePlanarCloudShaders();
				for(int i=0; i<handles.size(); ++i)          
					adapters.push_back( new osgEarth::NativeProgramAdapter(state, handles[i]) );
			}

			adapters.apply( state );

			renderInfo.getState()->disableAllVertexArrays();
			
			_SL->getAtmosphere()->DrawObjects( true, true, true );


			_SL->updateEnvMap();


			// Dirty the state and the program tracking to prevent GL state conflicts.

			//BEFORE MERGE
			/*state->dirtyAllVertexArrays();
			state->dirtyAllAttributes();
			osg::GL2Extensions* api = osg::GL2Extensions::Get(state->getContextID(), true);
			api->glUseProgram((GLuint)0);
			state->setLastAppliedProgramObject(0L);    */

			// Restore the GL state to where it was before.
			state->dirtyAllVertexArrays();
			state->dirtyAllAttributes();
			//osg::GL2Extensions* api = osg::GL2Extensions::Get(state->getContextID(), true);
			//api->glUseProgram((GLuint)0);
			//state->setLastAppliedProgramObject(0L);
			state->apply();

		}
    }
}

osg::BoundingBox
#if OSG_VERSION_GREATER_THAN(3,3,1)
	CloudsDrawable::computeBoundingBox() const
#else
	CloudsDrawable::computeBound() const
#endif
{
#ifdef SL_USE_MUTEX
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock( _contextNode->_mutex );
#endif
	osg::BoundingBox cloudBoundBox;
	if ( !_SL->ready() )
		return cloudBoundBox;

	double minX, minY, minZ, maxX, maxY, maxZ;
	_SL->getAtmosphere()->GetCloudBounds( minX, minY, minZ, maxX, maxY, maxZ );
	cloudBoundBox.set( osg::Vec3d(minX, minY, minZ), osg::Vec3d(maxX, maxY, maxZ) );
	return cloudBoundBox;
}
