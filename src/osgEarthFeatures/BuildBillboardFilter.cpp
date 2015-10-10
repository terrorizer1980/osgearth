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
#include <osgEarthFeatures/BuildBillboardFilter>
#include <osgEarthFeatures/BillboardShaders2>
#include <osgEarthFeatures/LabelSource>
#include <osgEarthSymbology/TextSymbol>
#include <osgEarthFeatures/FeatureIndex>
#include <osgEarth/Clamping>
#include <osgText/Text>
#include <osg/AlphaFunc>

#define LC "[BuildBillboardFilter] "

using namespace osgEarth;
using namespace osgEarth::Features;
using namespace osgEarth::Symbology;




BuildBillboardFilter::BuildBillboardFilter( const Style& style ) :
_style( style )
{
    //nop
}

osg::Node*
BuildBillboardFilter::push( FeatureList& input, FilterContext& context )
{
	computeLocalizers( context );

    osg::Node* result = processBillboards(input, context);

	// apply the delocalization matrix and return
	return delocalize( result);
}


osg::Geode*
	BuildBillboardFilter::processBillboards(FeatureList& features, FilterContext& context)
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


	const BillboardSymbol2* billboard_symb = _style.get<BillboardSymbol2>();
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
			osg::Vec4Array* colors = new osg::Vec4Array(verts->size());
			Random rng;

			float scale_var = billboard_symb->scaleVariance().get();
			float int_var = billboard_symb->brightnessVariance().get();
			
		
			for (int i=0; i < allpoints.size(); i++)
			{
				//osg::Vec3 world_pos = (*verts)[i];
				//world_pos.normalize();
				//osg::Vec3 normal = world_pos;
				//(*normals)[i] = normal;//osg::Matrix::transform3x3(normal, w2l);
				
				(*normals)[i] = osg::Vec3(0,0,1); //all points are localized, up-vector == +z  
				double intensity = 0.5 + int_var*(rng.next() - 0.5);
				double scale_factor = 1.0 + (rng.next()) * scale_var;
				(*colors)[i].set( intensity, intensity, scale_factor, 1 );
			}
			
			osgGeom->setVertexArray( verts );
			osgGeom->setNormalArray( normals );
			osgGeom->setNormalBinding( osg::Geometry::BIND_PER_VERTEX );
			osgGeom->setColorArray(colors);
			osgGeom->setColorBinding( osg::Geometry::BIND_PER_VERTEX );
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

			//osg::ref_ptr<StateSetCache> cache = new StateSetCache();
			//Registry::shaderGenerator().run(geode, cache.get());

			//set the texture related uniforms
			osg::StateSet* geode_ss = geode->getOrCreateStateSet();
			geode_ss->setTextureAttributeAndModes( 2, tex, 1 );
			geode_ss->getOrCreateUniform("billboard_tex", osg::Uniform::SAMPLER_2D)->set( 2 );

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

			geode_ss->getOrCreateUniform("billboard_width", osg::Uniform::FLOAT)->set( bbWidth );
			geode_ss->getOrCreateUniform("billboard_height", osg::Uniform::FLOAT)->set( bbHeight );
		
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
		
			//for now just using an osg::Program
			//TODO: need to add GeometryShader support to the shader comp setup
			osg::Program* pgm = new osg::Program;
			pgm->setName("billboard_program");
			pgm->addShader( new osg::Shader( osg::Shader::VERTEX, billboardVertShader ) );
			pgm->addShader( new osg::Shader( osg::Shader::GEOMETRY, billboardGeomShader ) );
			pgm->addShader( new osg::Shader( osg::Shader::FRAGMENT, billboardFragmentShader ) );
			pgm->setParameter( GL_GEOMETRY_VERTICES_OUT_EXT, 4 );
			pgm->setParameter( GL_GEOMETRY_INPUT_TYPE_EXT, GL_POINTS );
			pgm->setParameter( GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP );
			geode_ss->setAttribute(pgm);

			geode_ss->setMode( GL_CULL_FACE, osg::StateAttribute::OFF );
			geode->setCullingActive(false);
		}
	return geode;
}


