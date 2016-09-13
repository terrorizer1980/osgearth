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
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/


#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgViewer/CompositeViewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgEarth/MapNode>
#include <osgEarthUtil/EarthManipulator>
#include <osgEarthUtil/ExampleResources>
#include <osgEarthUtil/Sky>

#define LC "[viewer] "

using namespace osgEarth::Util;

int
main(int argc, char** argv)
{
    osg::ArgumentParser arguments(&argc,argv);

    osgViewer::CompositeViewer viewer(arguments);
    viewer.setThreadingModel( osgViewer::CompositeViewer::SingleThreaded );
	osg::DisplaySettings::instance()->setNumMultiSamples( 4 );
    // query the screen size.
    osg::GraphicsContext::ScreenIdentifier si;
    si.readDISPLAY();
    if ( si.displayNum < 0 ) si.displayNum = 0;
    osg::GraphicsContext::WindowingSystemInterface* wsi = osg::GraphicsContext::getWindowingSystemInterface();
    unsigned width, height;
    wsi->getScreenResolution( si, width, height );
    unsigned b = 50;

    osgViewer::View* mainView = new osgViewer::View();
    mainView->getCamera()->setNearFarRatio(0.00002);
    EarthManipulator* em = new EarthManipulator();
    em->getSettings()->setMinMaxPitch(-90, 0);
    mainView->setCameraManipulator( em );
    mainView->setUpViewInWindow( b, b, (width/2)-b*2, (height-b*4) );
    viewer.addView( mainView );

    osgViewer::View* overlayView = new osgViewer::View();
    overlayView->getCamera()->setNearFarRatio(0.00002);
    //overlayView->getCamera()->setProjectionMatrixAsOrtho2D(-1,1,-1,1);
	EarthManipulator* em2 = new EarthManipulator();
    overlayView->setCameraManipulator( em2 );
    
    overlayView->setUpViewInWindow( (width/2), b, (width/2)-b*2, (height-b*4) );
    overlayView->addEventHandler(new osgGA::StateSetManipulator(overlayView->getCamera()->getOrCreateStateSet()));
    viewer.addView( overlayView );

    std::string pathfile;
    double animationSpeed = 1.0;
    if (arguments.read("-p", pathfile))
    {
        mainView->setCameraManipulator( new osgGA::AnimationPathManipulator(pathfile) );
    }

    osg::Node* node = MapNodeHelper().load( arguments, mainView );
    if ( node )
    {
        mainView->setSceneData( node );

		osg::Group* group = new osg::Group();

		osgEarth::Util::SkyNode* skyNode = NULL;
		MapNode* mapNode =  MapNode::get(node);
		if ( node->getNumParents() > 0 )
		{
			skyNode = osgEarth::findTopMostNodeOfType<osgEarth::Util::SkyNode>(node->getParent(0));
			if(skyNode)
				group->addChild( skyNode);
			else
				group->addChild( mapNode);
		}
		else
		{
			group->addChild( mapNode);
		}
		
        overlayView->setSceneData( group );


		Viewpoint viewp(
			"Home",
			-71.0763, 42.34425, 15000,   // longitude, latitude, altitude
			0, -90, 3450.0);

		// zoom to a good startup position
		em->setViewpoint( viewp,2.0);
		em2->setViewpoint( viewp,2.0);

        return viewer.run();
    }
    else return -1;
}
