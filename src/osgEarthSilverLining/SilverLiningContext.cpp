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
#include <SilverLining.h> // SilverLinking SDK
#include "SilverLiningContext"
#include "SilverLiningNode"
#include <osg/Light>
#include <osgDB/FileNameUtils>
#include <osgEarth/SpatialReference>
#include <cstdlib>

#define LC "[SilverLiningContext] "

using namespace osgEarth::SilverLining;


/**
 * Adapter that converts the return value of osgEarth::SilverLining::Callback::getMilliseconds()
 * into a usable value for SilverLining using the SilverLining MillisecondTimer callback.
 */
class MillisecondTimerAdapter : public ::SilverLining::MillisecondTimer
{
public:
    MillisecondTimerAdapter(SilverLiningContext* context) :
    _context(context),
    _defaultTimer(new ::SilverLining::MillisecondTimer)
    {
    }

    virtual ~MillisecondTimerAdapter()
    {
        delete _defaultTimer;
    }

    virtual unsigned long SILVERLINING_API GetMilliseconds() const
    {
        osg::ref_ptr<SilverLiningContext> context;
        unsigned long milliseconds = 0;
        if (_context.lock(context))
        {
            osg::ref_ptr<Callback> callback = context->getCallback();
            if (callback.valid())
                milliseconds = callback->getMilliseconds();
        }

        // As per documentation, use the default SilverLining timer instead of returning 0
        if (milliseconds != 0)
            return milliseconds;
        return _defaultTimer->GetMilliseconds();
    }

private:
    osg::observer_ptr<SilverLiningContext> _context;
    ::SilverLining::MillisecondTimer* _defaultTimer;
};

SilverLiningContext::SilverLiningContext(const SilverLiningOptions& options) :
_options              ( options ),
_initAttempted        ( false ),
_initFailed           ( false ),
_maxAmbientLightingAlt( -1.0 ),
_atmosphere           ( 0L ),
_clouds               ( 0L ),
_minAmbient           ( 0,0,0,0 ),
_envMapID			  ( 0 ),
_updateEnvMap		  ( false ),
_useFixedRotation     (false),
_fixedLocation     (0,0,0),
_cumulusCongestusLayer (-1),
_setupClouds(false)
{
    // Create a SL atmosphere (the main SL object).
    _msTimer = new MillisecondTimerAdapter(this);

    // Create a SL atmosphere (the main SL object).
    _atmosphere = new ::SilverLining::Atmosphere(
        options.user()->c_str(),
        options.licenseCode()->c_str() );

    _atmosphereWrapper = new Atmosphere((uintptr_t)_atmosphere);
	optional<osg::Vec3f> cl = options.cloudLocation();
	_fixedLocation.set(cl.get().y(),cl.get().x(),cl.get().z());

	if(_fixedLocation.x() != 0)
		_useFixedRotation = true;
}

SilverLiningContext::~SilverLiningContext()
{
        delete _atmosphereWrapper;
        delete _atmosphere;
    delete _msTimer;

    OE_INFO << LC << "Destroyed\n";
}

void SilverLiningContext::updateEnvMap()
{
	if(_updateEnvMap)
	{
		void* pid;
		bool ret = _atmosphere->GetEnvironmentMap(pid);
		_envMapID = (GLuint) pid;
		_updateEnvMap = false;
	}
}

void
SilverLiningContext::setCallback(Callback* cb)
{
    _callback = cb;
}

void
SilverLiningContext::setLight(osg::Light* light)
{
    _light = light;
}

void
SilverLiningContext::setSRS(const osgEarth::SpatialReference* srs)
{
    _srs = srs;
}

void
SilverLiningContext::setMinimumAmbient(const osg::Vec4f& value)
{
    _minAmbient = value;
}

void
SilverLiningContext::initialize(osg::RenderInfo& renderInfo)
{
	if(_setupClouds)
	{
		setupClouds();
		_setupClouds = false;
	}
    if ( !_initAttempted && !_initFailed )
    {
        // lock/double-check:
        Threading::ScopedMutexLock excl(_initMutex);
        if ( !_initAttempted && !_initFailed )
        {
            _initAttempted = true;

            // constant random seed ensures consistent clouds across windows
            // TODO: replace this with something else since this is global! -gw
            ::srand(1234);
            //srand(0);
            std::string resourcePath = _options.resourcePath().get();
            if (resourcePath.empty() && ::getenv("SILVERLINING_PATH"))
            {
                resourcePath = osgDB::concatPaths(::getenv("SILVERLINING_PATH"), "Resources");
            }

            int result = _atmosphere->Initialize(
                ::SilverLining::Atmosphere::OPENGL,
                resourcePath.c_str(),
                true,
                0 );

            if ( result != ::SilverLining::Atmosphere::E_NOERROR )
            {
                _initFailed = true;
                OE_WARN << LC << "SilverLining failed to initialize: " << result << std::endl;
            }
            else
            {
                OE_INFO << LC << "SilverLining initialized OK!" << std::endl;

				if(_useFixedRotation)
				{
					//osg::Vec3d latLonAlt(-122.169,46.510,10000);
					osg::Vec3d worldpos;
					_srs->transformToWorld(_fixedLocation, worldpos);
					osg::Vec3d up = worldpos;
					up.normalize();
					osg::Vec3d north = osg::Vec3d(0, 1, 0);
					osg::Vec3d east = north ^ up;

					// Check for edge case of north or south pole
					if (east.length2() == 0)
					{
						east = osg::Vec3d(1, 0, 0);
					}
					east.normalize();

					_atmosphere->SetUpVector(up.x(), up.y(), up.z());
					_atmosphere->SetRightVector(east.x(), east.y(), east.z());
				}
				else
				{
					// Defaults for a projected terrain. ECEF terrain vectors are set
					// in updateLocation().
					_atmosphere->SetUpVector( 0.0, 0.0, 1.0 );
					_atmosphere->SetRightVector( 1.0, 0.0, 0.0 );
				}

                // Configure the timer used for animations
                _atmosphere->GetConditions()->SetMillisecondTimer(_msTimer);

#if 0 // todo: review this
                _maxAmbientLightingAlt = 
                    _atmosphere->GetConfigOptionDouble("atmosphere-height");
#endif
                if ( _options.drawClouds() == true )
                {
                    OE_INFO << LC << "Initializing clouds\n";
                    setupClouds();
                }

                // user callback for initialization
                if (_callback.valid())
                {
                    _callback->onInitialize( *_atmosphereWrapper );
                }
            }
        }
    }
}

void
SilverLiningContext::setupClouds()
{
	if(_cumulusCongestusLayer > -1)
		_atmosphere->GetConditions()->RemoveCloudLayer(_cumulusCongestusLayer);

    ::SilverLining::CloudLayer* clouds = ::SilverLining::CloudLayerFactory::Create( ::CUMULUS_CONGESTUS );
    clouds->SetIsInfinite( true );
    clouds->SetFadeTowardEdges(true);
    clouds->SetBaseAltitude( 2000 );
    clouds->SetThickness( 200 );
    clouds->SetBaseLength( 100000 );
    clouds->SetBaseWidth( 100000 );
    clouds->SetDensity( 0.6 );
    clouds->SetAlpha( 0.8 );
    clouds->GenerateShadowMaps( false );
    clouds->SetLayerPosition(0, 0);
	//clouds->SetWind(0,0);
	srand(1234);
	clouds->SeedClouds( *_atmosphere );
    _cumulusCongestusLayer = _atmosphere->GetConditions()->AddCloudLayer( clouds );
	_clouds = clouds;
}

void
SilverLiningContext::updateLight()
{
    if ( !ready() || !_light.valid() || !_srs.valid() )
        return;

    float ra, ga, ba, rd, gd, bd, x, y, z;

    // Clamp the camera's altitude while fetching the colors so the
    // lighting's ambient component doesn't fade to black at high altitude.
    ::SilverLining::Location savedLoc = _atmosphere->GetConditions()->GetLocation();
    ::SilverLining::Location clampedLoc = savedLoc;
    if ( _maxAmbientLightingAlt > 0.0 )
    {
        clampedLoc.SetAltitude( std::min(clampedLoc.GetAltitude(), _maxAmbientLightingAlt) );
        _atmosphere->GetConditions()->SetLocation( clampedLoc );
    }

    _atmosphere->GetAmbientColor( &ra, &ga, &ba );
    _atmosphere->GetSunColor( &rd, &gd, &bd );

    // Restore the actual altitude.
    if ( _maxAmbientLightingAlt > 0.0 )
    {
        _atmosphere->GetConditions()->SetLocation( savedLoc );
    }

    if ( _srs->isGeographic() )
    {
        _atmosphere->GetSunPositionGeographic( &x, &y, &z );
    }
    else
    {
        _atmosphere->GetSunPosition(&x, &y, &z);
    }

    osg::Vec3 direction(x, y, z);
    direction.normalize();

    /*osg::Vec4 ambient(
        osg::clampAbove(ra, _minAmbient.r()),
        osg::clampAbove(ba, _minAmbient.g()),
        osg::clampAbove(ga, _minAmbient.b()),
        1.0);*/

	osg::Vec4 ambient(
        ra * _minAmbient.r(),
        ba * _minAmbient.g(),
        ga * _minAmbient.b(),
        1.0);

    _light->setAmbient( ambient );
    _light->setDiffuse( osg::Vec4(rd, gd, bd, 1.0f) );
    _light->setPosition( osg::Vec4(direction, 0.0f) ); //w=0 means "at infinity"
}

void
SilverLiningContext::updateLocation()
{
    if ( !ready() || !_srs.valid() )
        return;

    if ( _srs->isGeographic() )
    {
        
		if(!_useFixedRotation)
		{
			// Get new local orientation
			osg::Vec3d up = _cameraPos;

			up.normalize();
			osg::Vec3d north = osg::Vec3d(0, 1, 0);
			osg::Vec3d east = north ^ up;

			// Check for edge case of north or south pole
			if (east.length2() == 0)
			{
				east = osg::Vec3d(1, 0, 0);
			}

			east.normalize();

			_atmosphere->SetUpVector(up.x(), up.y(), up.z());
			_atmosphere->SetRightVector(east.x(), east.y(), east.z());
		}

        // Get new lat / lon / altitude
        osg::Vec3d latLonAlt;
        _srs->transformFromWorld(_cameraPos, latLonAlt);

        ::SilverLining::Location loc;
       //loc.SetAltitude (osg::clampBelow(latLonAlt.z(), 5000.0 )); //hack to avoid cloud flickering at high altitude
		loc.SetAltitude ( latLonAlt.z() ); //hack to avoid cloud flickering at high altitude
        loc.SetLongitude( latLonAlt.x() );
        loc.SetLatitude ( latLonAlt.y() );
		
		//loc.SetAltitude ( 10000); 
		//loc.SetLongitude( -122.1696677949447 );
		//loc.SetLatitude ( 46.5107100574977 );

        _atmosphere->GetConditions()->SetLocation( loc );
		//if(_clouds)
			//_clouds->SetLayerPosition(0,0);
			//_clouds->SetLayerPosition(latLonAlt.y() , latLonAlt.x());
			//_clouds->MoveClouds(0,0,0);


#if 0 //TODO: figure out why we need to call this a couple times before
        if ( _clouds )
        {

      //      it takes effect. -gw
            static int c = 2;
            if ( c > 0 ) {
                --c;
                _clouds->SetLayerPosition(0, 0);
            }
        }
#endif

    }
}
