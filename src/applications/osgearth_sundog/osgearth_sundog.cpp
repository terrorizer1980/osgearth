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

#include <osgViewer/CompositeViewer>
#include <osgGA/StateSetManipulator>


#include <osgViewer/Viewer>
#include <osgDB/FileNameUtils>
#include <osgEarthUtil/ExampleResources>
#include <osgEarthUtil/EarthManipulator>
#include <osgEarthUtil/Controls>
#include <osgEarthSilverLining/SilverLiningNode>
#include <osgEarthTriton/TritonNode>
#include <osgEarth/NodeUtils>

#define LC "[osgearth_silverlining] "

using namespace osgEarth;
using namespace osgEarth::Util;
using namespace osgEarth::SilverLining;
using namespace osgEarth::Triton;

namespace ui = osgEarth::Util::Controls;


//general UI handler
template<typename T> struct Set : public ui::ControlEventHandler
{
	optional<T>& _var;
	Set(optional<T>& var) : _var(var) { }
	void onValueChanged(ui::Control*, T value) { _var = value; }
};

class SkyAndOceanIntegration
{
public:
	//Proxy class used to delegate callbacks from SilverLining  to SkyAndOceanEnvironment,
	//We use this proxy to avoid shutdown problems (SilverLiningNode will delete the callback-class in its destructor)
	struct SilverLininingCallbackProxy : public osgEarth::SilverLining::Callback
	{
		SilverLininingCallbackProxy(SkyAndOceanIntegration* callback) : _callback(callback) {}
		void onInitialize(Atmosphere& atmosphere)
		{
			_callback->onInitialize(atmosphere);
		}

		void onDrawSky(Atmosphere& atmosphere, osg::RenderInfo& renderInfo)
		{
			_callback->onDrawSky(atmosphere, renderInfo);
		}

		void onDrawClouds(Atmosphere& atmosphere, osg::RenderInfo& renderInfo)
		{
			_callback->onDrawClouds(atmosphere, renderInfo);
		}
		SkyAndOceanIntegration* _callback;
	};

	//Proxy class used to delegate callbacks from Triton
	class TritonCallbackProxy : public osgEarth::Triton::Callback
	{
	public:
		TritonCallbackProxy(SkyAndOceanIntegration* callback) : _callback(callback) {}
		void onInitialize(Environment& env, Ocean& ocean)
		{
			_callback->onInitialize(env, ocean);
		}
		void onDrawOcean(Environment& env, Ocean& ocean, osg::RenderInfo& renderInfo)
		{
			_callback->onDrawOcean(env, ocean, renderInfo);
		}
		SkyAndOceanIntegration* _callback;
	};

	SkyAndOceanIntegration(osgEarth::MapNode* mapNode) :
		_mapNode(mapNode),
		_silverLiningNode(nullptr),
		_tritonNode(nullptr),
		_drawCloudsInReflection(true),
		_updateEnvMapOnDraw(true)
	{

		_seaChop = 1;
		_seaState = 5;
		_minimumAmbient = 0;
		_drawCloudsInReflectionOption = false;
#if 1// DISABLE_SILVERLINING
		_silverLiningNode = _createSilverLining(mapNode);
#endif
#if 0// DISABLE_TRITON
		_tritonNode = _createTriton(mapNode);

		if (_silverLiningNode)
			_silverLiningNode->addChild(_tritonNode);
		else
			mapNode->addChild(_tritonNode);
#endif
	}

	//Triton Initialize callback
	void onInitialize(osgEarth::Triton::Environment& env, osgEarth::Triton::Ocean& ocean)
	{

	}

	//SilverLining Initialize callback
	void onInitialize(osgEarth::SilverLining::Atmosphere& atmosphere)
	{
#if 1
		CloudLayer cumulus = CloudLayerFactory::Create(CUMULUS_CONGESTUS);
		cumulus.SetIsInfinite(true);
		cumulus.SetBaseAltitude(3000);
		cumulus.SetThickness(50);
		cumulus.SetBaseLength(100000);
		cumulus.SetBaseWidth(100000);
		cumulus.SetDensity(0.5);
		cumulus.SetLayerPosition(0, 0);
		cumulus.SetFadeTowardEdges(true);
		cumulus.SetAlpha(0.8);
		cumulus.SetCloudAnimationEffects(0.1, false, 0, 0);
		cumulus.SeedClouds(atmosphere);
		atmosphere.GetConditions().AddCloudLayer(cumulus);
		atmosphere.EnableLensFlare(true);
#endif
	}

	void updateEnvMap(osgEarth::SilverLining::Atmosphere& atmosphere, osg::RenderInfo& renderInfo)
	{
		EnironmentMapInfo &env_info = _cameraEnvMapInfo[renderInfo.getCurrentCamera()];

		if(env_info._currentEnvMapFace == 0 && _updateEnvMapOnDraw.get())
		{
			env_info._requestEnvUpdate = true;
		}
			

#if 0 //Spread cubemap genereation over multiple (6) frames
		if (env_info._requestEnvUpdate)
		{
			GLuint tex_id;
			bool ret = atmosphere.GetEnvironmentMap(tex_id, 1, false, renderInfo.getCurrentCamera(), _drawCloudsInReflection, true);
			env_info._currentEnvMapFace++;
			if (ret &&   env_info._currentEnvMapFace == 6)
			{
				env_info._currentEnvMapFace = 0;
				env_info._envTexHandle = tex_id;
				env_info._requestEnvUpdate = false;
			}
		}
#else //generate all six cube map faces at once
		if (env_info._requestEnvUpdate && env_info._tritonActive)
		{
			GLuint tex_id;
			bool ret = atmosphere.GetEnvironmentMap(tex_id, 6, false, renderInfo.getCurrentCamera(), _drawCloudsInReflection, true);
			if (ret)
			{
				env_info._currentEnvMapFace = 0;
				env_info._envTexHandle = tex_id;
				env_info._requestEnvUpdate = false;
			}
		}
#endif
	}

	void requestEnvMapUpdate()
	{
		for (std::map<osg::Camera*, EnironmentMapInfo>::iterator iter = _cameraEnvMapInfo.begin(); iter != _cameraEnvMapInfo.end(); iter++)
		{
			if(iter->second._tritonActive)
				iter->second._requestEnvUpdate = true;
		}
	}

	bool cubeMapGenerationInProgress() const
	{
		bool cube_map_generation_in_progress = false;
		for (std::map<osg::Camera*, EnironmentMapInfo>::const_iterator iter = _cameraEnvMapInfo.begin(); iter != _cameraEnvMapInfo.end(); iter++)
		{
			return iter->second._currentEnvMapFace > 0;
		}
		return false;
	}

	void onDrawSky(osgEarth::SilverLining::Atmosphere& atmosphere, osg::RenderInfo& renderInfo)
	{
		//Check if time is set and that any previous cubemap rendering is completed
		if (_time.isSet())
		{
			if (!cubeMapGenerationInProgress())
			{
				DateTime now;
				DateTime skyTime(now.year(), now.month(), now.day(), _time.get());
				getSkyNode()->setDateTime(skyTime);
				_time.clear();
				//request environment update for all views
				if(!_updateEnvMapOnDraw.get())
					requestEnvMapUpdate();
			}
		}
		if (_minimumAmbient.isSet())
		{
			float value = _minimumAmbient.get();
			getSkyNode()->setMinimumAmbient(osg::Vec4(value, value, value, 1));
			//_minimumAmbient.clear();
		}
		//if(!cubeMapGenerationInProgress() && _updateEnvMapOnDraw.get())
		//	requestEnvMapUpdate();
		updateEnvMap(atmosphere, renderInfo);
	}

	void onDrawClouds(osgEarth::SilverLining::Atmosphere& atmosphere, osg::RenderInfo& renderInfo)
	{
		//Check if the cloud reflection option has changed.
		//We check this option here (and not in onDrawSky) to be sure 
		//that clouds have been rendered at least once in case 
		//we request environment map update.
		if (_drawCloudsInReflectionOption.isSet())
		{
			_drawCloudsInReflection = _drawCloudsInReflectionOption.get();
			_drawCloudsInReflectionOption.clear();
			requestEnvMapUpdate();
		}
	}

	void onDrawOcean(osgEarth::Triton::Environment& env, osgEarth::Triton::Ocean& ocean, osg::RenderInfo& renderInfo)
	{
		if (_seaChop.isSet())
		{
			ocean.SetChoppiness(_seaChop.get());
			_seaChop.clear();
		}

		if (_seaState.isSet())
		{
			env.SimulateSeaState(_seaState.get(), 0.0);
			_seaState.clear();
		}

		EnironmentMapInfo &env_info = _cameraEnvMapInfo[renderInfo.getCurrentCamera()];
		env_info._tritonActive = true;
		if (env_info._envTexHandle.isSet())
		{
			env.SetEnvironmentMap(env_info._envTexHandle.get());
			//don't clear handle, we need to set env-tex every frame if we have multiple views
			//env_info._envTexHandle.clear();
		}
	}
	
	osgEarth::Util::SkyNode* getSkyNode() const
	{
		return _silverLiningNode;
	}

	void createUI(ui::VBox* canvas)
	{
		if (canvas == nullptr)
			return;
		ui::Grid* grid = canvas->addControl(new ui::Grid());
		int r = 0;
		grid->setControl(0, r, new ui::LabelControl("Time"));
		ui::HSliderControl* time = grid->setControl(1, r, new ui::HSliderControl(0, 24, _time.get(), new Set<double>(_time)));
		grid->setControl(2, r, new ui::LabelControl(time));
		++r;
		grid->setControl(0, r, new ui::LabelControl("Sea Chop"));
		ui::HSliderControl* chop = grid->setControl(1, r, new ui::HSliderControl(0, 3, _seaChop.get(), new Set<double>(_seaChop)));
		grid->setControl(2, r, new ui::LabelControl(chop));
		++r;
		grid->setControl(0, r, new ui::LabelControl("Sea State"));
		ui::HSliderControl* seaState = grid->setControl(1, r, new ui::HSliderControl(0, 12, _seaState.get(), new Set<double>(_seaState)));
		grid->setControl(2, r, new ui::LabelControl(seaState));
		++r;
		grid->setControl(0, r, new ui::LabelControl("Min Ambient"));
		ui::HSliderControl* min_ambient = grid->setControl(1, r, new ui::HSliderControl(0, 1, _minimumAmbient.get(), new Set<float>(_minimumAmbient)));
		grid->setControl(2, r, new ui::LabelControl(min_ambient));
		++r;
		grid->getControl(1, r - 1)->setHorizFill(true, 200);

		grid->setControl(0, r, new ui::LabelControl("Draw Clouds in reflection"));
		grid->setControl(1, r, new ui::CheckBoxControl(_drawCloudsInReflectionOption.get(), new Set<bool>(_drawCloudsInReflectionOption)));
		++r;
		grid->setControl(0, r, new ui::LabelControl("Update Reflection"));
		grid->setControl(1, r, new ui::CheckBoxControl(_updateEnvMapOnDraw.get(), new Set<bool>(_updateEnvMapOnDraw)));
		++r;
	
		
	}

private:
	TritonNode* _createTriton(osgEarth::MapNode* mapNode)
	{
		osgEarth::Triton::TritonOptions tritonOptions;
		tritonOptions.user() = "my_user_name";
		tritonOptions.licenseCode() = "my_license_code";
		tritonOptions.maxAltitude() = 100000;
		tritonOptions.useHeightMap() = false;
		tritonOptions.renderBinNumber() = 0;

		const char* ev_t = ::getenv("TRITON_PATH");
		if (ev_t)
		{
			tritonOptions.resourcePath() = osgDB::concatPaths(
				std::string(ev_t),
				"Resources");

			OE_INFO << LC
				<< "Setting resource path to << " << tritonOptions.resourcePath().get()
				<< std::endl;
		}
		else
		{
			OE_WARN << LC
				<< "No resource path! Triton might not initialize properly. "
				<< "Consider setting the TRITON_PATH environment variable."
				<< std::endl;
		}

		// Create TritonNode from TritonOptions
		TritonNode* triton_node = new TritonNode(tritonOptions, new TritonCallbackProxy(this));
		triton_node->setMapNode(mapNode);
		return triton_node;
	}

	SilverLiningNode* _createSilverLining(osgEarth::MapNode* mapNode)
	{
		// Create SilverLiningNode from SilverLiningOptions
		osgEarth::SilverLining::SilverLiningOptions slOptions;
		slOptions.user() = "my_user_name";
		slOptions.licenseCode() = "my_license_code";
		slOptions.cloudsMaxAltitude() = 300000;

		const char* ev_sl = ::getenv("SILVERLINING_PATH");
		if (ev_sl)
		{
			slOptions.resourcePath() = osgDB::concatPaths(
				std::string(ev_sl),
				"Resources");
		}
		else
		{
			OE_WARN << LC
				<< "No resource path! SilverLining might not initialize properly. "
				<< "Consider setting the SILVERLINING_PATH environment variable."
				<< std::endl;
		}

		// TODO: uncommenting the callback on the following line results in a crash when SeedClouds is called.
		SilverLiningNode* sl_node = new SilverLiningNode(
			mapNode->getMapSRS(),
			slOptions,
			new SilverLininingCallbackProxy(this));
		// insert the new sky above the map node.
		osgEarth::insertParent(sl_node, mapNode);
		return sl_node;
	}
private:
	struct EnironmentMapInfo
	{
		EnironmentMapInfo() : _tritonActive(false), _requestEnvUpdate(false), _currentEnvMapFace(0){}
		osgEarth::optional<GLuint> _envTexHandle;
		bool _requestEnvUpdate;
		bool _tritonActive;
		int _currentEnvMapFace;
	};
	osgEarth::SilverLining::SilverLiningNode* _silverLiningNode;
	osgEarth::Triton::TritonNode* _tritonNode;
	osgEarth::optional<double> _seaChop;
	osgEarth::optional<double> _seaState;
	osgEarth::optional<float> _minimumAmbient;
	osgEarth::optional<double> _time;
	
	osgEarth::optional<bool> _drawCloudsInReflectionOption;
	osgEarth::optional<bool> _updateEnvMapOnDraw;
	bool _drawCloudsInReflection;
	osgEarth::MapNode* _mapNode;

	std::map<osg::Camera*, EnironmentMapInfo> _cameraEnvMapInfo;
};

int
usage(const char* name)
{
	OE_DEBUG
		<< "\nUsage: " << name << " file.earth" << std::endl
		<< osgEarth::Util::MapNodeHelper().usage() << std::endl;

	return 0;
}


int
main(int argc, char** argv)
{
#if 0
	osg::ArgumentParser arguments(&argc, argv);

	// help?
	if (arguments.read("--help"))
		return usage(argv[0]);

	// create a viewer:
	osgViewer::Viewer viewer(arguments);

	// Tell the database pager to not modify the unref settings
	viewer.getDatabasePager()->setUnrefImageDataAfterApplyPolicy(false, false);

	// install our default manipulator (do this before calling load)
	viewer.setCameraManipulator(new osgEarth::Util::EarthManipulator());

	//viewer.getCamera()->setProjectionMatrixAsPerspective(45, 1.0, 10.0, 50000.0);
	//viewer.getCamera()->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);

	VBox* canvas = new VBox();
	canvas->setBackColor(0, 0, 0, 0.5);

	// load an earth file, and support all or our example command-line options
	// and earth file <external> tags    
	osg::Group* node = MapNodeHelper().load(arguments, &viewer, canvas);
	if (node)
	{
		// Make sure we don't already have a sky.
		SkyNode* skyNode = osgEarth::findTopMostNodeOfType<SkyNode>(node);
		if (skyNode)
		{
			OE_WARN << LC << "Earth file already has a Sky. This example requires an "
				"earth file that does not use a sky already.\n";
			return -1;
		}

		viewer.getCamera()->setNearFarRatio(0.00002);
		viewer.getCamera()->setSmallFeatureCullingPixelSize(-1.0f);

		MapNode* mapNode = MapNode::findMapNode(node);

		SkyAndOceanIntegration* environment =  new SkyAndOceanIntegration(mapNode);
		environment->createUI(canvas);

		// connects the sky's light to the viewer.
		if (SkyNode* skyNode = environment->getSkyNode())
			skyNode->attach(&viewer);

		// use the topmost node.
		viewer.setSceneData(osgEarth::findTopOfGraph(node));

		return viewer.run();
	}
	else
	{
		return usage(argv[0]);
	}
#else
	osg::ArgumentParser arguments(&argc, argv);

	osgViewer::CompositeViewer viewer(arguments);
	//viewer.setThreadingModel(osgViewer::CompositeViewer::SingleThreaded);
	//viewer.setThreadingModel(osgViewer::CompositeViewer::CullDrawThreadPerContext);
	viewer.setThreadingModel(osgViewer::CompositeViewer::CullThreadPerCameraDrawThreadPerContext);

	// query the screen size.
	osg::GraphicsContext::ScreenIdentifier si;
	si.readDISPLAY();
	if (si.displayNum < 0) si.displayNum = 0;
	osg::GraphicsContext::WindowingSystemInterface* wsi = osg::GraphicsContext::getWindowingSystemInterface();
	unsigned width, height;
	wsi->getScreenResolution(si, width, height);
	unsigned b = 50;

	//Setup main view
	osgViewer::View* mainView = new osgViewer::View();
	mainView->getCamera()->setNearFarRatio(0.00002);
	EarthManipulator* em = new EarthManipulator();
	em->getSettings()->setMinMaxPitch(-90, 0);
	mainView->setCameraManipulator(em);
	mainView->setUpViewInWindow( b, b, (width/2)-b*2, (height-b*4) );
	//mainView->setUpViewInWindow(b, b, (width)-b * 2, (height - b * 4));
	viewer.addView(mainView);

	//setup second view
	osgViewer::View* secondView = new osgViewer::View();
	secondView->getCamera()->setNearFarRatio(0.00002);
	secondView->setCameraManipulator(new EarthManipulator());
#if 1
	secondView->setUpViewInWindow((width / 2), b, (width / 2) - b * 2, (height - b * 4));
#else
	//
	secondView->getCamera()->setViewport((width / 2), b, (width / 2) - b * 2, (height - b * 4));
	secondView->addEventHandler(new osgGA::StateSetManipulator(secondView->getCamera()->getOrCreateStateSet()));
	secondView->getCamera()->setGraphicsContext(mainView->getCamera()->getGraphicsContext());
#endif
	viewer.addView(secondView);
	VBox* canvas = new VBox();
	canvas->setBackColor(0, 0, 0, 0.5);

	osg::Node* node = MapNodeHelper().load(arguments, &viewer, canvas);

	if (node)
	{
		MapNode* mapNode = MapNode::findMapNode(node);
		SkyAndOceanIntegration* environment = new SkyAndOceanIntegration(mapNode);
		environment->createUI(canvas);

		// connects the sky's light to the viewer.
		SkyNode* skyNode = environment->getSkyNode();
		if (skyNode)
			skyNode->attach(mainView);

		// use the topmost node.
		mainView->setSceneData(osgEarth::findTopOfGraph(node));


		osg::Group* group = new osg::Group();

		if (skyNode)
		{
			group->addChild(skyNode);
			skyNode->attach(secondView, 0);
		}
		else
		{
			group->addChild(MapNode::get(node));
		}
		secondView->setSceneData(group);
		return viewer.run();
	}
	else return -1;
#endif
}
