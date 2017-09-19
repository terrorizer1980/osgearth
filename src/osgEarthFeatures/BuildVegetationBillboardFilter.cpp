/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2014 Pelican Mapping
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
#include <osgEarthFeatures/BuildVegetationBillboardFilter>
#include <osgEarthFeatures/VegetationBillboardShaders>
#include <osgEarthFeatures/LabelSource>
#include <osgEarthSymbology/TextSymbol>
#include <osgEarthFeatures/FeatureIndex>
#include <osgEarth/Clamping>
#include <osgEarth/VirtualProgram>
#include <osgEarth/Shaders>
#include <osgText/Text>
#include <osg/AlphaFunc>
#include <osg/BlendFunc>
#include <osg/Multisample>


#define LC "[BuildVegetationBillboardFilter] "

using namespace osgEarth;
using namespace osgEarth::Features;
using namespace osgEarth::Symbology;




BuildVegetationBillboardFilter::BuildVegetationBillboardFilter( const Style& style ) :
_style( style )
{
    //nop
}

osg::Node*
BuildVegetationBillboardFilter::push( FeatureList& input, FilterContext& context )
{
	computeLocalizers( context );

    osg::Node* result = processBillboards(input, context);

	// apply the delocalization matrix and return
	return delocalize( result);
}

//#define OLD_VEG_TECH

osg::Geode*
	BuildVegetationBillboardFilter::processBillboards(FeatureList& features, FilterContext& context)
{
	osg::Geode* geode = new osg::Geode();

	bool makeECEF = false;
	const SpatialReference* featureSRS = 0L;
	const SpatialReference* mapSRS = 0L;

	// set up referencing information:
	if ( context.isGeoreferenced() )
	{
		makeECEF   = context.getSession()->getMapInfo().isGeocentric();
		featureSRS = context.extent()->getSRS();
		mapSRS     = context.getSession()->getMapInfo().getProfile()->getSRS();
	}


	const VegetationBillboardSymbol* billboard_symb = _style.get<VegetationBillboardSymbol>();
	if ( !billboard_symb)
		return NULL;

	FilterContext localCX = context;

	
	std::vector<osg::Vec3d> allpoints;
	for( FeatureList::iterator f_iter = features.begin(); f_iter != features.end(); ++f_iter )
	{
		Feature* f = f_iter->get();
		if ( f && f->getGeometry() )
		{
			GeometryIterator iter(f->getGeometry());
			while(iter.hasMore())
			{
				const Geometry* geom = iter.next();
				allpoints.insert(allpoints.end(), geom->asVector().begin(), geom->asVector().end());
			}
		}
	}

	if ( allpoints.size() > 0 )
	{
			osg::ref_ptr<osg::Geometry> osgGeom = new osg::Geometry();
	
			osg::Vec3Array* verts = new osg::Vec3Array();	

			transformAndLocalize(allpoints , featureSRS, verts, mapSRS, _world2local, makeECEF );

			osg::Vec3Array* normals = new osg::Vec3Array(verts->size());
#ifdef OLD_VEG_TECH
			osg::Vec4Array* colors = new osg::Vec4Array(verts->size());
#endif
			Random rng;

			float scale_var = billboard_symb->scaleVariance().get();
			float int_var = billboard_symb->brightnessVariance().get();
			
		
			for (int i=0; i < allpoints.size(); i++)
			{
				double scale_factor = 1.0 + (rng.next()) * scale_var;
				double intensity = (0.5 + int_var*(rng.next() - 0.5))*0.3;
#ifdef OLD_VEG_TECH
				(*normals)[i] = osg::Vec3(0,0,1); //all points are localized, up-vector == +z  
				(*colors)[i].set( intensity, intensity, scale_factor, 1.0 );
#else
				(*normals)[i].set(intensity, intensity, scale_factor);
#endif			
			}
			
			osgGeom->setVertexArray( verts );
			osgGeom->setNormalArray( normals );
			osgGeom->setNormalBinding( osg::Geometry::BIND_PER_VERTEX );
#ifdef OLD_VEG_TECH
			osgGeom->setColorArray(colors);
			osgGeom->setColorBinding( osg::Geometry::BIND_PER_VERTEX);
			
#endif
			//osg::Vec2Array* texCoords = new osg::Vec2Array(verts->size());
			//osgGeom->setTexCoordArray(2,texCoords);

			osgGeom->addPrimitiveSet( new osg::DrawArrays( GL_POINTS, 0, verts->size() ) );
			
			//Load image
			osg::Image* image = billboard_symb->imageURI()->getImage();
			if(!image)
			{
				 OE_WARN << LC 
                        << "Failed to Load billboard image:"<< billboard_symb->imageURI().get().full() << std::endl;
				 return NULL;
			}

			osg::Texture2D* tex = new osg::Texture2D(image);
			tex->setResizeNonPowerOfTwoHint(false);
			tex->setFilter( osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR );
			tex->setFilter( osg::Texture::MAG_FILTER, osg::Texture::LINEAR );
			tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
			tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

			osgGeom->setName("BillboardPoints");
			geode->addDrawable(osgGeom);

			//set the texture related uniforms
			osg::StateSet* geode_ss = geode->getOrCreateStateSet();
			geode_ss->setTextureAttributeAndModes( 2, tex, 1 );
			geode_ss->addUniform(new osg::Uniform("oe_billboard_tex", 2));
			
#ifdef OLD_VEG_TECH
			geode_ss->getOrCreateUniform("billboard_tex", osg::Uniform::SAMPLER_2D)->set( 2 );
#endif

			float bbWidth = (float)tex->getImage()->s() / 2.0f;
			float bbHeight = (float)tex->getImage()->t();
			float aspect = (float)tex->getImage()->s() / (float)tex->getImage()->t();

			if (billboard_symb->height().isSet())
			{
				bbHeight = billboard_symb->height().get();
				if (!billboard_symb->width().isSet())
				{
					bbWidth = bbHeight * aspect / 2.0f;
				}
			}
			if (billboard_symb->width().isSet())
			{
				bbWidth = billboard_symb->width().get() / 2.0f;
				if (!billboard_symb->height().isSet())
				{
					bbHeight = billboard_symb->width().get() / aspect;
				}
			}

			geode_ss->getOrCreateUniform("oe_billboard_width", osg::Uniform::FLOAT)->set( bbWidth );
			geode_ss->getOrCreateUniform("oe_billboard_height", osg::Uniform::FLOAT)->set( bbHeight );
		

			if(!osg::DisplaySettings::instance()->getMultiSamples())
			{
				osg::AlphaFunc* alphaFunc = new osg::AlphaFunc;
				alphaFunc->setFunction(osg::AlphaFunc::GEQUAL, billboard_symb->alphaRefValue().get());

				geode_ss->setAttributeAndModes( alphaFunc, osg::StateAttribute::ON );

				if(billboard_symb->alphaBlend().get())
				{
					geode_ss->setMode(GL_BLEND, osg::StateAttribute::ON);
					//geode_ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
				}
				else
					geode_ss->setMode(GL_BLEND, osg::StateAttribute::OFF);
			}
			else
			{
				geode_ss->setAttributeAndModes(new osg::Multisample, osg::StateAttribute::ON);
				geode_ss->setMode(GL_MULTISAMPLE_ARB, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
				geode_ss->setMode(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB,  osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
				geode_ss->setAttributeAndModes(
					new osg::BlendFunc(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO),
					osg::StateAttribute::OVERRIDE );
			}
#ifdef OLD_VEG_TECH
			osg::Program* pgm = new osg::Program;
			pgm->setName("billboard_program");
			pgm->addShader( new osg::Shader( osg::Shader::VERTEX, billboardVertShader ) );
			pgm->addShader( new osg::Shader( osg::Shader::GEOMETRY, billboardGeomShader ) );
			pgm->addShader( new osg::Shader( osg::Shader::FRAGMENT, billboardFragmentShader ) );
			pgm->setParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 4 );
			pgm->setParameter( GL_GEOMETRY_INPUT_TYPE_EXT, GL_POINTS );
			pgm->setParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP );
			geode_ss->setAttribute(pgm);
#else
		/*	const char* vegetationGeomShader =
				//"#version 330 \n"
				"#version " GLSL_VERSION_STR "\n"
				"#pragma vp_location   geometry\n"
				"#pragma vp_name VegetationGS\n"
				"#pragma vp_entryPoint VegetationGeomShader\n"
				"#extension GL_EXT_geometry_shader4 : enable\n"
				"layout(points) in; \n"
				"layout(triangle_strip) out; \n"
				"layout(max_vertices = 4) out; \n"
				GLSL_DEFAULT_PRECISION_FLOAT "\n"
				"varying out float brightness;\n"
				"uniform float billboard_width; \n"
				"uniform float billboard_height; \n"
				"void VP_LoadVertex(in int); \n"
				"void VP_EmitViewVertex(); \n"
				"vec3 vp_Normal;\n"
				"vec4 vp_Color;\n"
				"out vec4 veg_texcoord;\n"
				"void VegetationGeomShader(void)\n"
				"{\n"

				"    vec4 bb_color = vec4(1.0, 1.0, 1.0, 1.0);\n"
				"    VP_LoadVertex(0); \n"
				"    brightness = vp_Normal.x*2;\n"
				"    float scale = vp_Normal.z;\n"
				"    vec3 normal = vec3(0.0, 0.0, 1.0);\n"
				"    vec4 base_pos = gl_ModelViewMatrix * gl_in[0].gl_Position;\n"
				"    vec4 top_pos = gl_ModelViewMatrix * (gl_in[0].gl_Position + vec4(normal*billboard_height*scale, 0.0));\n"
				"    \n"
				// TODO: this width calculation isn't great but works for now
				"    vec4 center_v = gl_ModelViewMatrix * vec4(0.,0.,0.,1.);\n"
				"    vec4 right_v = gl_ModelViewMatrix * vec4(billboard_width*scale,0.,0.,1.);\n"
				"    float width = distance(right_v, center_v);\n"
				"    \n"

				//"    brightness = gl_in[0].gl_Color.r*2.0;\n"
				//"    vp_Color = bb_color; \n"
				"    gl_Position = (top_pos + vec4(-width, 0.0, 0.0, 0.0)); \n"
				"    veg_texcoord = vec4(0.0, 1.0, 0.0, 0.0); \n"
				"	 vp_Normal =  normal;\n"
				"    vp_Color = bb_color; \n"
				"    VP_EmitViewVertex(); \n"

				//"    vp_Color = bb_color; \n"
				"    gl_Position = (top_pos + vec4(width, 0.0, 0.0, 0.0)); \n"
				"    veg_texcoord = vec4(1.0, 1.0, 0.0, 0.0); \n"
				//"    tex_coord = vec2(1.0, 1.0); \n"
				"	 vp_Normal =  normal;\n"
				"    vp_Color = bb_color; \n"
				"    VP_EmitViewVertex(); \n"

				//"    vp_Color = bb_color; \n"
				"    brightness = vp_Normal.x;\n"
				"    gl_Position = (base_pos + vec4(-width, 0.0, 0.0, 0.0)); \n"
				"    veg_texcoord = vec4(0.0, 0.0, 0.0, 0.0); \n"
				"	 vp_Normal =  normal;\n"
				"    vp_Color = bb_color; \n"
				"    VP_EmitViewVertex(); \n"
				
				//"    vp_Color = bb_color; \n"
				"    gl_Position = (base_pos + vec4(width, 0.0, 0.0, 0.0)); \n"
				"    veg_texcoord = vec4(1.0, 0.0, 0.0, 0.0); \n"
				"	 vp_Normal =  normal;\n"
				"    vp_Color = bb_color; \n"
				"    VP_EmitViewVertex(); \n"
				"    EndPrimitive(); \n"
				"}\n";


			const char* vegetationFragShaderColoring =
				"#version " GLSL_VERSION_STR "\n"
				"#pragma vp_name VegetationFS\n"
				"#pragma vp_entryPoint VegetationFragShaderColoring\n"
				"#pragma vp_location   fragment_coloring\n"
				"uniform sampler2D veg_texure;\n"
				"in vec4 veg_texcoord;\n"
				"varying float brightness;\n"
				"uniform float oe_thermal_mode;\n"
				"uniform float oe_veg_temperature_detail_strength = 1.0;\n"
				"uniform float oe_veg_temperature = 275;\n"
				"void VegetationFragShaderColoring(inout vec4 color) \n"
				"{ \n"
				//"    if (color.a < 0.2) discard; \n"
				"	 vec4 texel = texture2D(veg_texure, veg_texcoord.xy);\n"
				"    color = texel;\n"
				"    float contrast = clamp(1.0-brightness, 0.85, 1.0);\n"
				"    color.rgb = clamp(((color.rgb-0.5)*contrast + 0.5) * (1.0+brightness), 0.0, 1.0);\n"
				"    if(oe_thermal_mode > 0)\n"
				"    {\n"
				"		color.r  = ((1.0 - color.r) + (1.0 - color.g) + (1.0 - color.b))/3.0;\n"
				"		color.r = oe_veg_temperature + color.r*oe_veg_temperature_detail_strength;\n"
				"       color.g = color.r;\n"
				"       color.b = color.r;\n"
				"       if ( color.a < 0.2 ) discard; \n"
				"    }\n"
				"} \n";


			const char* vegetationVertexShader =
				"#version 330 compatibility\n"
				"#pragma vp_entryPoint VegetationVertexShader\n"
				"#pragma vp_location   vertex_view\n"
				"#pragma vp_varying vec4 oe_sg_texcoord2\n"
				"vec4 oe_sg_texcoord2;\n"
				"void VegetationVertexShader(inout vec4 vertex) { \n"
				"    oe_sg_texcoord2 = gl_MultiTexCoord2;\n"
				"} \n";
				*/
		
			VirtualProgram* vp = VirtualProgram::getOrCreate(geode_ss);
			//VirtualProgram* vp = new VirtualProgram();
			vp->getTemplate()->setParameter(GL_GEOMETRY_VERTICES_OUT_EXT, 4);
			vp->getTemplate()->setParameter(GL_GEOMETRY_INPUT_TYPE_EXT, GL_POINTS);
			vp->getTemplate()->setParameter(GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);

			osgEarth::Shaders pkg;
			pkg.load(vp, pkg.VegetationGeometry);
			pkg.load(vp, pkg.VegetationFragment);
		
			//vp->setFunction("VegetationVertexShader", vegetationVertexShader, osgEarth::ShaderComp::LOCATION_VERTEX_MODEL);
			//vp->setFunction("VegetationGeomShader", vegetationGeomShader, osgEarth::ShaderComp::LOCATION_GEOMETRY);
			//vp->setFunction("VegetationFragShaderColoring", vegetationFragShaderColoring, osgEarth::ShaderComp::LOCATION_FRAGMENT_COLORING);
			geode_ss->setAttribute(vp, osg::StateAttribute::ON);
			vp->setShaderLogging(true, "c:/temp/veg_shaders.glsl");
#endif
			geode_ss->setMode( GL_CULL_FACE, osg::StateAttribute::OFF );
			geode->setCullingActive(false);
		}
	return geode;
}


