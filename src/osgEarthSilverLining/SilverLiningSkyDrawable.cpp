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
_contextNode(node)
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
	osg::ref_ptr<SilverLiningContext>  sl_context = _contextNode->getOrCreateContext(renderInfo);
	sl_context->onDrawSky(renderInfo);
	/*osg::Camera* camera = renderInfo.getCurrentCamera();
	renderInfo.getState()->disableAllVertexArrays();
	sl_context->initialize(renderInfo);

	const osg::State* state = renderInfo.getState();

	osgEarth::NativeProgramAdapterCollection& adapters = _adapters[state->getContextID()]; // thread safe.
	if (adapters.empty())
	{
		adapters.push_back(new osgEarth::NativeProgramAdapter(state, sl_context->getAtmosphere()->GetSkyShader()));
		adapters.push_back(new osgEarth::NativeProgramAdapter(state, sl_context->getAtmosphere()->GetBillboardShader()));
		adapters.push_back(new osgEarth::NativeProgramAdapter(state, sl_context->getAtmosphere()->GetStarShader()));
		adapters.push_back(new osgEarth::NativeProgramAdapter(state, sl_context->getAtmosphere()->GetPrecipitationShader()));

		SL_VECTOR(unsigned) handles = sl_context->getAtmosphere()->GetActivePlanarCloudShaders();
		for (int i = 0; i < handles.size(); ++i)
			adapters.push_back(new osgEarth::NativeProgramAdapter(state, handles[i]));
	}
	adapters.apply(state);


	// convey the sky box size (far plane) to SL:
	double fovy, ar, znear, zfar;
	sl_context->setCamera(camera);
	camera->getProjectionMatrixAsPerspective(fovy, ar, znear, zfar);
	sl_context->setSkyBoxSize(zfar < 100000.0 ? zfar : 100000.0);

	//JH: moved here to avoid problems with Triton flickering, maybe one frame off... 
	sl_context->getAtmosphere()->SetProjectionMatrix(renderInfo.getState()->getProjectionMatrix().ptr());
	sl_context->getAtmosphere()->SetCameraMatrix(renderInfo.getCurrentCamera()->getViewMatrix().ptr());

	// invoke the user callback if it exists
	if (sl_context->getCallback())
		sl_context->getCallback()->onDrawSky(sl_context->getAtmosphereWrapper(), renderInfo);

	// draw the sky.
	sl_context->getAtmosphere()->DrawSky(
		true,
		sl_context->getSRS()->isGeographic(),
		sl_context->getSkyBoxSize(),
		true,
		false,
		true,
		camera);

	// Dirty the state and the program tracking to prevent GL state conflicts.
	renderInfo.getState()->dirtyAllVertexArrays();
	renderInfo.getState()->dirtyAllAttributes();
	renderInfo.getState()->dirtyAllModes();

#if 0
#if OSG_VERSION_GREATER_OR_EQUAL(3,4,0)
	osg::GLExtensions* api = renderInfo.getState()->get<osg::GLExtensions>();
#else
	osg::GL2Extensions* api = osg::GL2Extensions::Get(renderInfo.getState()->getContextID(), true);
#endif
	api->glUseProgram((GLuint)0);
	renderInfo.getState()->setLastAppliedProgramObject(0L);
#endif

	renderInfo.getState()->apply();*/
}

osg::BoundingBox
#if OSG_VERSION_GREATER_THAN(3,3,1)
SkyDrawable::computeBoundingBox() const
#else
SkyDrawable::computeBound() const
#endif
{
    osg::BoundingBox skyBoundBox;
	return skyBoundBox;
    /*if (_SL == NULL || !_SL->ready() )
        return skyBoundBox;

    ::SilverLining::Atmosphere* atmosphere = _SL->getAtmosphere();
    double skyboxSize = _SL->getSkyBoxSize();
    if ( skyboxSize == 0.0 )
        skyboxSize = 1000.0;

    osg::Vec3d radiusVec = osg::Vec3d(skyboxSize, skyboxSize, skyboxSize) * 0.5;
    osg::Vec3d camPos = _SL->getCameraPosition();
    if (_SL->getCamera())
    {
        osg::Vec3f eye, center, up;
        _SL->getCamera()->getViewMatrixAsLookAt(eye, center, up);
        camPos = osg::Vec3d(eye.x(), eye.y(), eye.z());
    }

    skyBoundBox.set( camPos-radiusVec, camPos+radiusVec );

    // this enables the "blue ring" around the earth when viewing from space.
    bool hasLimb = atmosphere->GetConfigOptionBoolean("enable-atmosphere-from-space");
    if ( hasLimb )
    {
        // Compute bounds of atmospheric limb centered at (0,0,0)
        double earthRadius = atmosphere->GetConfigOptionDouble("earth-radius-meters");
        double atmosphereHeight = earthRadius + atmosphere->GetConfigOptionDouble("atmosphere-height");
        double atmosphereThickness = atmosphere->GetConfigOptionDouble("atmosphere-scale-height-meters") + earthRadius;

        osg::BoundingBox atmosphereBox;
        osg::Vec3d atmMin(-atmosphereThickness, -atmosphereThickness, -atmosphereThickness);
        osg::Vec3d atmMax(atmosphereThickness, atmosphereThickness, atmosphereThickness);
        atmosphereBox.set( atmMin, atmMax );
        skyBoundBox.expandBy( atmosphereBox );
    }
    return skyBoundBox;*/
}
