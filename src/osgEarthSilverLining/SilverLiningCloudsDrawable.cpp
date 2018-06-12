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
#include "SilverLiningCloudsDrawable"
#include "SilverLiningContext"
#include "SilverLiningNode"
#include <osgEarth/SpatialReference>

#undef  LC
#define LC "[SilverLining:SkyDrawable] "

using namespace osgEarth::SilverLining;


CloudsDrawable::CloudsDrawable(SilverLiningNode* node) :
    _SLNode(node)
{
    // call this to ensure draw() gets called every frame.
    setSupportsDisplayList( false );
    
    // not MT-safe (camera updates, etc)
    this->setDataVariance(osg::Object::DYNAMIC);
    
    setName("SilverLining::CloudsDrawable");
}

void
CloudsDrawable::drawImplementation(osg::RenderInfo& renderInfo) const
{
    osg::ref_ptr<SilverLiningContext>  sl_context = _SLNode->getOrCreateContext(renderInfo);
    sl_context->onDrawClouds(renderInfo);
}

osg::BoundingBox
#if OSG_VERSION_GREATER_THAN(3,3,1)
CloudsDrawable::computeBoundingBox() const
#else
CloudsDrawable::computeBound() const
#endif
{
    //return undefined cloud bounds to support multiple views with infinit cloud layers
    return _cloudBoundBox;
}

void CloudsDrawable::updateBounds(osg::Camera* camera, SilverLiningContext* sl_context)
{
    ::SilverLining::Atmosphere* atmosphere = sl_context->getAtmosphere();
    double minX, minY, minZ, maxX, maxY, maxZ;
    atmosphere->GetCloudBounds(minX, minY, minZ, maxX, maxY, maxZ);
    _cloudBoundBox.set(osg::Vec3d(minX, minY, minZ), osg::Vec3d(maxX, maxY, maxZ));
    dirtyBound();
}

