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

#include <Triton.h>
#include "TritonNode"
#include "TritonContext"
#include "TritonDrawable"
#include <osgEarth/CullingUtils>
#include <osgEarthUtil/Sky>

#define LC "[TritonNode] "

using namespace osgEarth::Triton;

TritonNode::TritonNode(osgEarth::MapNode*   mapNode,
                       const TritonOptions& options,
                       Callback*            callback) :
OceanNode( options ),
_options ( options )
{
    const osgEarth::Map* map = mapNode->getMap();
    if ( map )
        setSRS( map->getSRS() );

    _TRITON = new TritonContext( options );

    if ( map )
        _TRITON->setSRS( map->getSRS() );

	osgEarth::Util::SkyNode* skynode = NULL;
	if ( mapNode->getNumParents() > 0 )
	{
		skynode = osgEarth::findTopMostNodeOfType<osgEarth::Util::SkyNode>(mapNode->getParent(0));
	}
	else
	{
		skynode = osgEarth::findTopMostNodeOfType<osgEarth::Util::SkyNode>(mapNode);
	}

    TritonDrawable* tritonDrawable = new TritonDrawable(mapNode,_TRITON,skynode);
    _drawable = tritonDrawable;

	if ( callback )
        _TRITON->setCallback( callback );

    
    _alphaUniform = getOrCreateStateSet()->getOrCreateUniform("oe_ocean_alpha", osg::Uniform::FLOAT);
    _alphaUniform->set(getAlpha());

    osg::Geode* geode = new osg::Geode();
    geode->addDrawable( _drawable );
    geode->setNodeMask( TRITON_OCEAN_MASK );

    this->addChild( geode );

    this->setNumChildrenRequiringUpdateTraversal(1);

    // Place in the depth-sorted bin and set a rendering order.
    // We want Triton to render after the terrain.
    _drawable->getOrCreateStateSet()->setRenderBinDetails( options.renderBinNumber().get(), "DepthSortedBin" );
}

TritonNode::~TritonNode()
{
    //nop
}

void
TritonNode::onSetSeaLevel()
{
    if ( _TRITON->ready() )
    {
        _TRITON->getEnvironment()->SetSeaLevel( getSeaLevel() );
		//_TRITON->getEnvironment()->SetWaveBlendDepth(1);

		 
    }
    dirtyBound();
}

void
TritonNode::onSetAlpha()
{
    _alphaUniform->set(getAlpha());
}

osg::BoundingSphere
TritonNode::computeBound() const
{
    return osg::BoundingSphere();
}

void
TritonNode::traverse(osg::NodeVisitor& nv)
{
    if ( nv.getVisitorType() == nv.UPDATE_VISITOR && _TRITON->ready() )
    {
        _TRITON->update(nv.getFrameStamp()->getSimulationTime());
    }
    osgEarth::Util::OceanNode::traverse(nv);
}
