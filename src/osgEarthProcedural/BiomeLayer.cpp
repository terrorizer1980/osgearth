/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
* Copyright 2008-2012 Pelican Mapping
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
#include "BiomeLayer"
#include <osgEarth/Random>
#include <osgEarth/rtree.h>

using namespace osgEarth;
using namespace osgEarth::Procedural;

REGISTER_OSGEARTH_LAYER(biomes, BiomeLayer);

#define LC "[BiomeLayer] "

//...................................................................

void
BiomeLayer::Options::fromConfig(const Config& conf)
{
    blendRadius().setDefault(0.02);
    biomeidField().setDefault("biomeid");

    biomeCatalog() = std::make_shared<BiomeCatalog>(conf.child("biomecatalog"));
    controlVectors().get(conf, "control_vectors");
    conf.get("blend_radius", blendRadius());
    conf.get("biomeid_field", biomeidField());
}

Config
BiomeLayer::Options::getConfig() const
{
    Config conf = ImageLayer::Options::getConfig();
    OE_DEBUG << LC << __func__ << " not yet implemented" << std::endl;
    controlVectors().set(conf, "control_vectors");
    //TODO - biomeCatalog
    conf.set("blend_radius", blendRadius());
    conf.set("biomeid_field", biomeidField());
    return conf;
}

//...................................................................

#undef LC
#define LC "[BiomeLayer] " << getName() << ": "

namespace
{
    struct Record
    {
        Segment2d _segment;
        int _biome_index;
        double _radius;
    };

    typedef std::shared_ptr<Record> RecordPtr;
    typedef RTree<RecordPtr, double, 2> MySpatialIndex;

    struct PolygonRecord {
        int _biome_index;
        osg::ref_ptr<const Geometry> _polygon;
        double _buffer;
    };
    typedef std::shared_ptr<PolygonRecord> PolygonRecordPtr;
    typedef RTree<PolygonRecordPtr, double, 2> PolygonSpatialIndex;
    
    struct BiomeTrackerToken : public osg::Object
    {
        META_Object(osgEarth, BiomeTrackerToken);
        BiomeTrackerToken() { }
        BiomeTrackerToken(std::set<int>&& seen) : _biome_index_set(seen) { }
        BiomeTrackerToken(const BiomeTrackerToken& rhs, const osg::CopyOp& op) { }
        std::set<int> _biome_index_set;
    };
}

void
BiomeLayer::init()
{
    ImageLayer::init();

    // BiomeLayer is invisible AND shared by default.
    options().visible().setDefault(false);
    options().shared().setDefault(true);
    options().textureCompression().setDefault("none");

    _pointIndex = nullptr;
    _polygonIndex = nullptr;

    _autoBiomeManagement = true;

    setProfile(Profile::create(Profile::GLOBAL_GEODETIC));
}

Status
BiomeLayer::openImplementation()
{
    Status p = ImageLayer::openImplementation();
    if (p.isError())
        return p;

    Status csStatus = options().controlVectors().open(getReadOptions());
    if (csStatus.isError())
        return csStatus;

    // Warn the poor user if the configuration is missing
    if (getBiomeCatalog() == nullptr)
    {
        OE_WARN << LC << "No biome catalog found - could be trouble" << std::endl;
    }
    else if (getBiomeCatalog()->getAssets().empty())
    {
        OE_WARN << LC << "No asset catalog found - could be trouble" << std::endl;
    }

    return Status::OK();
}

Status
BiomeLayer::closeImplementation()
{
    if (_pointIndex)
    {
        delete static_cast<MySpatialIndex*>(_pointIndex);
        _pointIndex = nullptr;
    }

    if (_polygonIndex)
    {
        delete static_cast<PolygonSpatialIndex*>(_polygonIndex);
        _polygonIndex = nullptr;
    }

    options().controlVectors().close();

    return ImageLayer::closeImplementation();
}

void
BiomeLayer::addedToMap(const Map* map)
{
    options().controlVectors().addedToMap(map);

    if (!getControlSet())
    {
        setStatus(Status::ConfigurationError, "No control set found");
        return;
    }

    //loadPointControlSet();

    loadPolygonControlSet();
}

void
BiomeLayer::loadPointControlSet()
{
    // DEPRECATED?

    OE_INFO << LC << "Loading control set..." << std::endl;

    MySpatialIndex* index = new MySpatialIndex();
    _pointIndex = index;

    const std::string& biomeid_field = options().biomeidField().get();

    // Populate the in-memory spatial index with all the control points
    int count = 0;
    osg::ref_ptr<FeatureCursor> cursor = getControlSet()->createFeatureCursor(Query(), nullptr);
    if (cursor.valid())
    {
        cursor->fill(_features);

        for (auto& feature : _features)
        {
            int biomeid = feature->getInt(biomeid_field);
            double radius = feature->getDouble("radius", 0.0);

            const Geometry* g = feature->getGeometry();

            if (g->isPointSet())
            {
                ConstGeometryIterator iter(g);
                while (iter.hasMore())
                {
                    const Geometry* part = iter.next();
                    for (auto& p : *part)
                    {
                        double a_m[2] = { p.x(), p.y() };
                        index->Insert(a_m, a_m, RecordPtr(new Record{ Segment2d(p, p), biomeid, radius }));
                    }
                }
            }
            else
            {
                ConstGeometryIterator g_iter(g);
                while (g_iter.hasMore())
                {
                    ConstSegmentIterator s_iter(g_iter.next());
                    while (s_iter.hasMore())
                    {
                        Segment s = s_iter.next();
                        double a_min[2] = { std::min(s.first.x(), s.second.x()), std::min(s.first.y(), s.second.y()) };
                        double a_max[2] = { std::max(s.first.x(), s.second.x()), std::max(s.first.y(), s.second.y()) };
                        index->Insert(a_min, a_max, RecordPtr(new Record{ Segment2d(s.first, s.second), biomeid, radius }));
                    }
                }
            }
        }
    }
    OE_INFO << LC << "Loaded control set and found " << count << " features" << std::endl;
}

void
BiomeLayer::loadPolygonControlSet()
{
    OE_INFO << LC << "Loading polygon control set..." << std::endl;

    PolygonSpatialIndex* index = new PolygonSpatialIndex();
    _polygonIndex = index;

    const std::string& biomeid_field = options().biomeidField().get();

    // Populate the in-memory spatial index with all the control points
    int count = 0;
    osg::ref_ptr<FeatureCursor> cursor = getControlSet()->createFeatureCursor(Query(), nullptr);
    if (cursor.valid())
    {
        cursor->fill(_features);

        for (auto& feature : _features)
        {
            int biomeid = feature->getInt(biomeid_field);
            double buffer = feature->getDouble("buffer", 0.0);

            Geometry* g = feature->getGeometry();
            GeometryIterator iter(g, false);
            while (iter.hasMore())
            {
                Geometry* part = iter.next();
                if (part->isPolygon())
                {
                    part->open();
                    part->removeDuplicates();
                    part->removeColinearPoints();

                    Bounds b = part->getBounds();
                    double a_min[2] = { b.xMin(), b.yMin() };
                    double a_max[2] = { b.xMax(), b.yMax() };

                    index->Insert(
                        a_min, a_max,
                        PolygonRecordPtr(new PolygonRecord({ 
                            biomeid,
                            part,
                            buffer })));
                }
            }
        }
    }
    OE_INFO << LC << "Loaded control set and found " << count << " features" << std::endl;
}


FeatureSource*
BiomeLayer::getControlSet() const
{
    return options().controlVectors().getLayer();
}

std::shared_ptr<const BiomeCatalog>
BiomeLayer::getBiomeCatalog() const
{
    return options().biomeCatalog();
}

const Biome*
BiomeLayer::getBiomeByIndex(int index) const
{
    const auto cat = getBiomeCatalog();
    if (cat)
        return cat->getBiomeByIndex(index);
    else
        return nullptr;
}

void
BiomeLayer::setAutoBiomeManagement(bool value)
{
    _autoBiomeManagement = value;
    
    if (_autoBiomeManagement == false)
    {
        _biomeMan.reset();
    }
}

bool
BiomeLayer::getAutoBiomeManagement() const
{
    return _autoBiomeManagement;
}

GeoImage
BiomeLayer::createImageImplementation(
    const TileKey& key,
    ProgressCallback* progress) const
{
    MySpatialIndex* pointIndex = static_cast<MySpatialIndex*>(_pointIndex);
    if (pointIndex)
    {
        // allocate a 16-bit image so we can represent 32K biomes
        osg::ref_ptr<osg::Image> image = new osg::Image();
        image->allocateImage(
            getTileSize(),
            getTileSize(),
            1,
            GL_RED,
            GL_FLOAT);
        image->setInternalTextureFormat(GL_R16F);

        ImageUtils::PixelWriter write(image.get());

        osg::Vec4 value;
        float noise = 1.0f;
        std::vector<RecordPtr> hits;
        std::vector<double> ranges_squared;
        Random prng(key.hash());
        double radius = options().blendRadius().get();
        std::set<int> biomeids_seen;

        GeoImageIterator iter(GeoImage(image.get(), key.getExtent()));

        iter.forEachPixelOnCenter([&]()
            {
                int biome_index = 0;

                // randomly permute the coordinates in order to blend across biomes
                double x = iter.x() + radius * (prng.next()*2.0 - 1.0);
                double y = iter.y() + radius * (prng.next()*2.0 - 1.0);

                hits.clear();

                // find the closest biome vector to the point:
                pointIndex->KNNSearch(
                    osg::Vec3d(x, y, 0).ptr(),
                    &hits,
                    nullptr,
                    1u,
                    0.0);

                if (hits.size() > 0)
                {
                    biome_index = hits[0]->_biome_index;

                    if (biome_index > 0)
                        biomeids_seen.insert(biome_index);
                }

                value.r() = biome_index;

                write(value, iter.s(), iter.t());
            });

        GeoImage result(image.get(), key.getExtent());

        trackImage(result, key, biomeids_seen);

        return std::move(result);
    }

    PolygonSpatialIndex* polygonIndex = static_cast<PolygonSpatialIndex*>(_polygonIndex);
    if (polygonIndex)
    {
        // allocate a 16-bit image so we can represent 32K biomes
        osg::ref_ptr<osg::Image> image = new osg::Image();
        image->allocateImage(
            getTileSize(),
            getTileSize(),
            1,
            GL_RED,
            GL_FLOAT);
        image->setInternalTextureFormat(GL_R16F);

        ImageUtils::PixelWriter write(image.get());

        osg::Vec4 value;
        float noise = 1.0f;
        std::vector<PolygonRecordPtr> hits;
        std::vector<double> ranges_squared;
        Random prng(key.hash());
        double radius = options().blendRadius().get();
        std::set<int> biome_indexes_seen;

        GeoImage temp(image.get(), key.getExtent());
        GeoImageIterator iter(temp);

        iter.forEachPixelOnCenter([&]()
            {
                int biome_index = 0;

                // randomly permute the coordinates in order to blend across biomes
                double x = iter.x();
                double y = iter.y();

                if (radius > temp.getUnitsPerPixel())
                {
                    //double dx = (prng.next()*2.0 - 1.0), dy = (prng.next()*2.0 - 1.0);
                    //double len = sqrt(dx*dx + dy*dy);
                    //dx /= len, dy /= len;
                    //x += radius * dx, y += radius * dy;
                    x += radius * (prng.next()*2.0 - 1.0);
                    y += radius * (prng.next()*2.0 - 1.0);
                }

                double a_point[2] = { x, y };

                hits.clear();

                polygonIndex->Search(
                    a_point, a_point,
                    &hits,
                    999u);

                for (auto& hit : hits)
                {
                    if (hit->_polygon->contains2D(x, y))
                    {
                        biome_index = hit->_biome_index;

                        if (biome_index > 0)
                            biome_indexes_seen.insert(biome_index);

                        break;
                    }
                }

                value.r() = biome_index;

                write(value, iter.s(), iter.t());
            });

        GeoImage result(image.get(), key.getExtent());

        trackImage(result, key, biome_indexes_seen);

        return std::move(result);
    }

    return GeoImage::INVALID;
}

void
BiomeLayer::postCreateImageImplementation(
    GeoImage& createdImage,
    const TileKey& key,
    ProgressCallback* progress) const
{
    if (getAutoBiomeManagement() &&
        createdImage.getTrackingToken() == nullptr)
    {
        // if there's no tracking token (e.g., this image came from the cache)
        // build and attach one now.
        GeoImageIterator iter(createdImage);
        ImageUtils::PixelReader read(createdImage.getImage());

        std::set<int> biome_indexes_seen;
        osg::Vec4 pixel;

        iter.forEachPixel([&]()
            {
                int biomeid = 0;
                read(pixel, iter.s(), iter.t());
                int biome_index = (int)pixel.r();
                biome_indexes_seen.insert(biome_index);
            });

        trackImage(createdImage, key, biome_indexes_seen);
    }
}

void
BiomeLayer::trackImage(
    GeoImage& image,
    const TileKey& key,
    std::set<int>& biome_index_set) const
{
    // inform the biome manager that we are using the biomes corresponding
    // to the biome ID's we collected
    for (auto biome_index : biome_index_set)
    {
        const Biome* biome = getBiomeCatalog()->getBiomeByIndex(biome_index);
        if (biome)
            const_cast<BiomeManager*>(&_biomeMan)->ref(biome);
    }

    // Create a "token" object that we can track for destruction.
    // This will inform us when the image created by this call
    // destructs, and we can unref the usage in the BiomeManager accordingly.
    // This works, but reverses the flow of control, so maybe
    // there is a better solution -gw
    osg::Object* token = new BiomeTrackerToken(std::move(biome_index_set));
    token->setName(Stringify() << "BiomeLayer " << key.str());
    image.setTrackingToken(token);
    token->addObserver(const_cast<BiomeLayer*>(this));
    _tracker.scoped_lock([&]() { _tracker[token] = key; });
}

void
BiomeLayer::objectDeleted(void* value)
{
    BiomeTrackerToken* token = static_cast<BiomeTrackerToken*>(value);

    _tracker.scoped_lock([&]() 
        {
            OE_DEBUG << LC << "Unloaded " << token->getName() << std::endl;

            if (getAutoBiomeManagement())
            {
                for (auto biome_index : token->_biome_index_set)
                {
                    const Biome* biome = getBiomeCatalog()->getBiomeByIndex(biome_index);
                    if (biome)
                    {
                        _biomeMan.unref(biome);
                    }
                }
            }
            _tracker.erase(token);
        });
}
