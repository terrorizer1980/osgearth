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
#include "SilverLiningSkyDrawable"
#include "SilverLiningContext"
#include "SilverLiningNode"
#include <osgEarth/SpatialReference>

#define LC "[SilverLining:SkyDrawable] "

using namespace osgEarth::SilverLining;


SkyDrawable::SkyDrawable(SilverLiningNode* node) :
    _SLNode(node)
{
    // call this to ensure draw() gets called every frame.
    setSupportsDisplayList( false );

    // not MT-safe (camera updates, etc)
    this->setDataVariance( osg::Object::DYNAMIC );

    setName("SilverLining::SkyDrawable");;
}

void
SkyDrawable::drawImplementation(osg::RenderInfo& renderInfo) const
{
    osg::ref_ptr<SilverLiningContext>  sl_context = _SLNode->getOrCreateContext(renderInfo);
    sl_context->onDrawSky(renderInfo);
}

osg::BoundingBox
#if OSG_VERSION_GREATER_THAN(3,3,1)
SkyDrawable::computeBoundingBox() const
#else
SkyDrawable::computeBound() const
#endif
{
    //Return undefined bounds to support multiple views.
    return _skyBoundBox;
}

void SkyDrawable::updateBounds(osg::Camera* camera, SilverLiningContext* sl_context)
{
    ::SilverLining::Atmosphere* atmosphere = sl_context->getAtmosphere();
    double skyboxSize = sl_context->getSkyBoxSize();
    if (skyboxSize == 0.0)
        skyboxSize = 1000.0;

    osg::Vec3d radiusVec = osg::Vec3d(skyboxSize, skyboxSize, skyboxSize);// *0.5;
    osg::Vec3f eye, center, up;
    camera->getViewMatrixAsLookAt(eye, center, up);
    osg::Vec3d camPos = osg::Vec3d(eye.x(), eye.y(), eye.z());
    _skyBoundBox.set(camPos - radiusVec, camPos + radiusVec);

    // this enables the "blue ring" around the earth when viewing from space.
    bool hasLimb = atmosphere->GetConfigOptionBoolean("enable-atmosphere-from-space");
    if (hasLimb)
    {
        // Compute bounds of atmospheric limb centered at (0,0,0)
        double earthRadius = atmosphere->GetConfigOptionDouble("earth-radius-meters");
        double atmosphereHeight = earthRadius + atmosphere->GetConfigOptionDouble("atmosphere-height");
        double atmosphereThickness = atmosphere->GetConfigOptionDouble("atmosphere-scale-height-meters") + earthRadius;

        osg::BoundingBox atmosphereBox;
        osg::Vec3d atmMin(-atmosphereThickness, -atmosphereThickness, -atmosphereThickness);
        osg::Vec3d atmMax(atmosphereThickness, atmosphereThickness, atmosphereThickness);
        atmosphereBox.set(atmMin, atmMax);
        _skyBoundBox.expandBy(atmosphereBox);
    }
    dirtyBound();
}
