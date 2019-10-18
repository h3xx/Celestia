
#include "astrodb.h"

using namespace Eigen;

constexpr array<const char *, AstroDatabase::MaxBuiltinCatalog> AstroDatabase::CatalogPrefix;

AstroDatabase::AstroDatabase() :
    m_autoIndex(AutoIndexMax),
    m_starOctree(Vector3d(0, 0, 0), OctreeNode::MaxScale, nullptr),
    m_dsoOctree(Vector3d(0, 0, 0),  OctreeNode::MaxScale, nullptr)
{
    createBuiltinCatalogs();
    NameInfo::runTranslation();
}

AstroDatabase::~AstroDatabase()
{
    cout << "AstroDatabase\n";
    NameInfo::stopTranslation();
}

AstroObject *AstroDatabase::getObject(AstroCatalog::IndexNumber nr) const
{
    MainIndex::const_iterator it = m_mainIndex.find(nr);
    if (it == m_mainIndex.end())
        return nullptr;
    return it->second;
}

AstroObject *AstroDatabase::getObject(const Name &name, bool tryGreek, bool smart) const
{
    auto info = smart ? m_nameIndex.getNameInfo(name, tryGreek, false, true) : m_nameIndex.getNameInfo(name, tryGreek);
    AstroObject *obj = info == nullptr ? nullptr : (AstroObject*)info->getObject();
    if (obj != nullptr)
        return obj;
    if (smart)
    {
        obj = m_nameIndex.findObjectByName(name, tryGreek);
        if (obj != nullptr)
            return obj;
    }
    for (const auto& ci : m_catalogs)
    {
        AstroCatalog::IndexNumber inr = ci.second->nameToCatalogNumber(name.str());
        if (inr == AstroCatalog::InvalidIndex)
            continue;
        AstroCatalog::IndexNumber nr = catalogNumberToIndex(ci.first, inr);
        obj = getObject(nr);
        if (obj != nullptr)
            return obj;
    }
    return nullptr;
}

Star *AstroDatabase::getStar(AstroCatalog::IndexNumber nr) const
{
    AstroObject *star = getObject(nr);
    if (star == nullptr)
        return nullptr;
    return star->toSelection().star();
}

Star *AstroDatabase::getStar(const Name &name, bool tryGreek, bool smart) const
{
    AstroObject *star = getObject(name, tryGreek, smart);
    if (star == nullptr)
        return nullptr;
    return star->toSelection().star();
}

DeepSkyObject *AstroDatabase::getDSO(AstroCatalog::IndexNumber nr) const
{
    AstroObject *dso = getObject(nr);
    if (dso == nullptr)
        return nullptr;
    return dso->toSelection().deepsky();
}

DeepSkyObject *AstroDatabase::getDSO(const Name &name, bool tryGreek, bool smart) const
{
    AstroObject *dso = getObject(name, tryGreek, smart);
    if (dso == nullptr)
        return nullptr;
    return dso->toSelection().deepsky();
}

AstroCatalog::IndexNumber AstroDatabase::catalogNumberToIndex(int catalog, AstroCatalog::IndexNumber nr) const
{
    std::map<int, CrossIndex*>::const_iterator it = m_catxindex.find(catalog);
    if (it == m_catxindex.end())
        return AstroCatalog::InvalidIndex;
    return it->second->get(nr);
}

AstroCatalog::IndexNumber AstroDatabase::indexToCatalogNumber(int catalog, AstroCatalog::IndexNumber nr) const
{
    std::map<int, CrossIndex*>::const_iterator it = m_celxindex.find(catalog);
    if (it == m_celxindex.end())
        return AstroCatalog::InvalidIndex;
    return it->second->get(nr);
}

AstroCatalog::IndexNumber AstroDatabase::nameToIndex(const Name& name, bool tryGreek, bool smart) const
{
    AstroObject *obj = getObject(name, tryGreek, smart);
    if (obj == nullptr)
        return AstroCatalog::InvalidIndex;
    return obj->getIndex();
}

AstroCatalog::IndexNumber AstroDatabase::starnameToIndex(const Name& name, bool tryGreek) const
{
    AstroObject *obj = m_nameIndex.findObjectByName(name, tryGreek);
    if (obj == nullptr)
        return AstroCatalog::InvalidIndex;
    return obj->getIndex();
}

std::string AstroDatabase::catalogNumberToString(AstroCatalog::IndexNumber nr) const
{
    return fmt::sprintf("#%u", nr);
}

std::string AstroDatabase::catalogNumberToString(int catalog, AstroCatalog::IndexNumber nr) const
{
    auto it = m_catalogs.find(catalog);
    if (it != m_catalogs.end())
        return it->second->catalogNumberToName(nr);
    return "";
}

Name AstroDatabase::getObjectName(AstroCatalog::IndexNumber nr, bool i18n) const
{
    AstroObject *obj = getObject(nr);
    if (obj == nullptr)
        return string();
    if (i18n && obj->hasLocalizedName())
        return obj->getLocalizedName();
    if (obj->hasName())
        return obj->getName();
    return catalogNumberToString(nr);
}

std::vector<Name> AstroDatabase::getObjectNameList(AstroCatalog::IndexNumber nr, int max) const
{
    std::vector<Name> ret;
    AstroObject *obj = getObject(nr);
    if (obj == nullptr || obj->getNameInfos() == nullptr)
        return ret;
    for(const auto &iter : *obj->getNameInfos())
    {
        if (max == 0)
            return ret;
        ret.push_back(iter->getCanon());
        --max;
    }

    if (max == 0)
        return ret;
    for (const auto it : m_catalogs)
    {
        if (max == 0)
            break;
        AstroCatalog::IndexNumber inr = indexToCatalogNumber(it.first, nr);
        if (inr == AstroCatalog::InvalidIndex)
        {
//             cout << "Invalid cross index entry for catalog " << it.first << "[" << nr << "]\n";
            continue;
        }
        ret.push_back(it.second->catalogNumberToName(inr));
        --max;
    }
    return ret;
}

std::string AstroDatabase::getObjectNames(AstroCatalog::IndexNumber nr, bool i18n, int max) const
{
    string names;
    names.reserve(max); // optimize memory allocation
    AstroObject *obj = getObject(nr);
    if (obj == nullptr || obj->getNameInfos() == nullptr)
        return "";
    for (const auto &name : *obj->getNameInfos())
    {
        if (max == 0)
            return names;
        if (!names.empty())
            names += " / ";

        names += i18n ? name->getLocalized().str() : name->getCanon().str();
        --max;
    }

    if (max == 0)
        return names;
    for (const auto it : m_catalogs)
    {
        if (max == 0)
            break;
        AstroCatalog::IndexNumber inr = indexToCatalogNumber(it.first, nr);
        if (inr == AstroCatalog::InvalidIndex)
        {
//             cout << "Invalid cross index entry for catalog " << it.first << "[" << nr << "]\n";
            continue;
        }
        if (names.size() > 0)
            names += " / ";
        names += it.second->catalogNumberToName(inr);
        --max;
    }
//     fmt::fprintf(cout, "AstroDatabase::getObjectNames(): %s\n", names);
    return names;
}

void AstroDatabase::removeName(const NameInfo::SharedConstPtr &name)
{
    if (name->getSystem() == nullptr)
        m_nameIndex.erase(name->getCanon());
    else
        fmt::fprintf(cerr, "Trying to remove local name \"%s\" of object nr %u!\n", name->getCanon().str(), name->getObject()->getIndex());
}

void AstroDatabase::removeNames(AstroCatalog::IndexNumber nr)
{
    AstroObject *obj = getObject(nr);
    if (obj != nullptr)
        removeNames(obj);
}

void AstroDatabase::removeNames(AstroObject *obj)
{
    if (obj->getNameInfos() == nullptr)
        return;
    for (const auto &info : *obj->getNameInfos())
    {
        if (info->getSystem() == nullptr)
            removeName(info);
    }
}

bool AstroDatabase::addAstroCatalog(int id, AstroCatalog *catalog)
{
    if (m_catalogs.count(id) > 0)
        return false;
    m_catalogs.insert(std::make_pair(id, catalog));
    return true;
}

bool AstroDatabase::addCatalogNumber(AstroCatalog::IndexNumber celnr, int catalog, AstroCatalog::IndexNumber catnr, bool overwrite)
{
    return addCatalogRange(celnr, catalog, catnr - celnr, 1, overwrite);
}

bool AstroDatabase::addCatalogRange(AstroCatalog::IndexNumber nr, int catalog, int shift, size_t length, bool overwrite)
{
    if (m_catalogs.count(catalog) == 0)
    {
        fmt::fprintf(cerr, "Catalog %i not registered!\n", catalog);
        return false;
    }
    if (m_celxindex.count(catalog) == 0)
        m_celxindex.insert(std::make_pair(catalog, new CrossIndex));
    CrossIndex *i = m_celxindex.find(catalog)->second;
//     fmt::fprintf(cout, "Adding cel-cat range %i => [%i, %i]...\n", nr, shift, length);
    if (!i->set(nr, shift, length, overwrite))
        return false;
    nr += shift;
    shift = -shift;
    if (m_catxindex.count(catalog) == 0)
        m_catxindex.insert(std::make_pair(catalog, new CrossIndex));
    i = m_catxindex.find(catalog)->second;
//     fmt::fprintf(cout, "Adding cat-cel range %i => [%i, %i]...\n", nr, shift, length);
    return i->set(nr, shift, length, overwrite);
}

bool AstroDatabase::addObject(AstroObject *obj)
{
    if (obj->getIndex() == AstroCatalog::InvalidIndex)
        obj->setIndex(getAutoIndex());
    if (obj->getIndex() == AstroCatalog::InvalidIndex)
    {
        fmt::fprintf(cerr, "Error: Cannot allocate new index number!\n");
        return false;
    }
    if (m_mainIndex.count(obj->getIndex()) > 0)
    {
        fmt::fprintf(cerr, "Error: object nr %u already exists!\n", obj->getIndex());
        return false;
    }
    obj->setDatabase(this);
    m_mainIndex.insert(std::make_pair(obj->getIndex(), obj));
    if (obj->getNameInfos() == nullptr)
        return true;
    for(const auto& info : *obj->getNameInfos())
    {
        if (info->getSystem() == nullptr)
            m_nameIndex.add(info);
    }
    return true;
}

bool AstroDatabase::addStar(Star *star)
{
    if (!addObject(star))
        return false;
    m_stars.insert(star);
    m_starOctree.insertObject(star);
//    fmt::fprintf(cout, "Added star  with magnitude %f.\n", star->getAbsoluteMagnitude());
    return true;
}

bool AstroDatabase::addDSO(DeepSkyObject *dso)
{
    if (!addObject(dso))
        return false;
    m_dsos.insert(dso);
    m_dsoOctree.insertObject(dso);
    return true;
}

bool AstroDatabase::addBody(Body *body)
{
    if (!addObject(body))
        return false;
    m_bodies.insert(body);
    return true;
}

bool AstroDatabase::removeObject(AstroObject *obj)
{
    if (obj == nullptr)
        cerr << "Null object removing!" << endl;
    Selection sel = obj->toSelection();
    switch(sel.getType())
    {
        case Selection::Type_Star:
            m_stars.erase(sel.star());
            break;
        case Selection::Type_DeepSky:
            m_dsos.erase(sel.deepsky());
    }
    m_mainIndex.erase(obj->getIndex());
    removeNames(obj);
    obj->setDatabase(nullptr);
    return true;
}

bool AstroDatabase::removeObject(AstroCatalog::IndexNumber nr)
{
    AstroObject *obj = getObject(nr);
    if (obj == nullptr)
        return false;
    return removeObject(obj->getIndex());
}

bool AstroDatabase::addName(AstroCatalog::IndexNumber nr, const Name& name)
{
    AstroObject *o = getObject(nr);
    if (o == nullptr)
        return false;
    return o->addName(name);
}

bool AstroDatabase::addName(const NameInfo::SharedConstPtr &info)
{
    return m_nameIndex.add(info);
//     fmt::fprintf(cerr, "Adding name \"%s\" to object nr %u.\n", info.getCanon().str(), obj->getIndex());
}

bool AstroDatabase::addLocalizedName(const NameInfo::SharedConstPtr &info)
{
    return m_nameIndex.addLocalized(info);
//     fmt::fprintf(cerr, "Adding name \"%s\" to object nr %u.\n", info.getCanon().str(), obj->getIndex());
}

void AstroDatabase::addNames(AstroCatalog::IndexNumber nr, const string &names)
{
    AstroObject *obj = getObject(nr);
    if (obj != nullptr)
        obj->addNames(names);
    else
        fmt::fprintf(cerr, "No object nr %u to add names \"%s\"!\n", nr, names);
}

bool AstroDatabase::addSystem(SolarSystem *sys, AstroCatalog::IndexNumber nr)
{
    AstroObject *o = getObject(nr);
    if (o == nullptr)
    {
        fmt::fprintf(cerr, "Trying to add SolarSystem to null object!\n");
        return false;
    }
    m_systems.insert(make_pair(nr, sys));
    return true;
}

SolarSystem *AstroDatabase::getSystem(AstroCatalog::IndexNumber nr) const
{
    auto it = m_systems.find(nr);
    if (it == m_systems.end())
        return nullptr;
    return it->second;
}

bool AstroDatabase::removeSystem(AstroCatalog::IndexNumber nr)
{
    return m_systems.erase(nr) > 0 ? true : false;
}

void AstroDatabase::createBuiltinCatalogs()
{
    m_catalogs.insert(std::make_pair(HenryDraper, new HenryDrapperCatalog()));
    m_catalogs.insert(std::make_pair(Gliese, new GlieseAstroCatalog()));
    m_catalogs.insert(std::make_pair(SAO, new SAOAstroCatalog()));
    m_catalogs.insert(std::make_pair(Hipparcos, new HipparcosAstroCatalog()));
    m_catalogs.insert(std::make_pair(Tycho, new TychoAstroCatalog()));

    addCatalogRange(1, Hipparcos, 0, HipparcosAstroCatalog::MaxCatalogNumber - 1);
    addCatalogRange(HipparcosAstroCatalog::MaxCatalogNumber + 1, Tycho, 0, TychoAstroCatalog::MaxCatalogNumber -  HipparcosAstroCatalog::MaxCatalogNumber - 1);
}

AstroCatalog::IndexNumber AstroDatabase::getAutoIndex()
{
    if (m_autoIndex > AutoIndexMin)
    {
        AstroCatalog::IndexNumber ret = m_autoIndex;
        m_autoIndex--;
        return ret;
    }
    return AstroCatalog::InvalidIndex;
}

float AstroDatabase::avgDsoMag() const
{
    float avg = 0;
    size_t n = m_dsos.size();
    for(const auto & dso : m_dsos)
    {
        if (dso->getAbsoluteMagnitude() > 8)
            avg += dso->getAbsoluteMagnitude();
        else
            n--;
    }
    avg /= n;
    return avg;
}

CrossIndex *AstroDatabase::getCelestiaCrossIndex(int catalog)
{
    map<int, CrossIndex*>::iterator it = m_celxindex.find(catalog);
    if (it == m_celxindex.end())
        return nullptr;
    return it->second;
}

const CrossIndex *AstroDatabase::getCelestiaCrossIndex(int catalog) const
{
    map<int, CrossIndex*>::const_iterator it = m_celxindex.find(catalog);
    if (it == m_celxindex.end())
        return nullptr;
    return it->second;
}

CrossIndex *AstroDatabase::getCatalogCrossIndex(int catalog)
{
    map<int, CrossIndex*>::iterator it = m_catxindex.find(catalog);
    if (it == m_catxindex.end())
        return nullptr;
    return it->second;
}

const CrossIndex *AstroDatabase::getCatalogCrossIndex(int catalog) const
{
    map<int, CrossIndex*>::const_iterator it = m_catxindex.find(catalog);
    if (it == m_catxindex.end())
        return nullptr;
    return it->second;
}

static inline float toM(size_t n) { return n / 1024.f / 1024.f; }

void AstroDatabase::dumpStats() const
{
    fmt::fprintf(cout,
                 "AstroDatabase::dumpStats()\n"
                 " Objects: %u (* %u B = %f MB)\n"
                 " Stars: %u (* %u B = %f MB)\n"
                 " DSOs: %u (* %u B = %f MB)\n"
                 " Bodies: %u (* %u B = %f MB)\n"
                 " Names: %u\n"
                 " Localized names: %u\n"
                 " Octree nodes: %u (* %u B = %f MB)\n",
                 m_mainIndex.size(),
                 sizeof(AstroObject),
                 toM(sizeof(AstroObject) * m_mainIndex.size()),
                 m_stars.size(),
                 sizeof(Star),
                 toM(sizeof(Star) * m_stars.size()),
                 m_dsos.size(),
                 sizeof(DeepSkyObject),
                 toM(sizeof(DeepSkyObject) * m_dsos.size()),
                 m_bodies.size(),
                 sizeof(Body),
                 toM(sizeof(Body) * m_bodies.size()),
                 m_nameIndex.getNamesNumber(),
                 m_nameIndex.getLocalizedNamesNumber(),
                 OctreeNode::getNodesNumber(),
                 sizeof(OctreeNode),
                 toM(OctreeNode::getNodesNumber() * sizeof(OctreeNode))
                );
    size_t xindexRecords = 0;
    for (const auto &it : m_catxindex)
        xindexRecords += it.second->getRecords().size();
    for (const auto &it : m_celxindex)
        xindexRecords += it.second->getRecords().size();
    fmt::fprintf(cout,
                 " CrossIndex records: %u (* %u B = %f MB)\n",
                 xindexRecords,
                 sizeof(CrossIndex::CrossIndexRange),
                 toM(sizeof(CrossIndex::CrossIndexRange) * xindexRecords)
                );
}