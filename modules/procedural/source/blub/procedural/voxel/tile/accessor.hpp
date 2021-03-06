#ifndef PROCEDURAL_VOXEL_TILE_ACCESSOR_HPP
#define PROCEDURAL_VOXEL_TILE_ACCESSOR_HPP

#include "blub/core/array.hpp"
#include "blub/core/globals.hpp"
#include "blub/core/scopedPtr.hpp"
#include "blub/math/vector2int.hpp"
#include "blub/math/vector3int.hpp"
#include "blub/serialization/access.hpp"
#include "blub/serialization/nameValuePair.hpp"


namespace blub
{
namespace procedural
{
namespace voxel
{
namespace tile
{


/**
 * @brief The accessor class caches all voxel needed by tile::surface for an extremly optimized and fast calculation for the same.
 * This class looks up all voxel needed for the modified marching cubes and if lod is enabled 6*arrays for every side of the cube (transvoxel).
 * See http://www.terathon.com/voxels/ and http://www.terathon.com/lengyel/Lengyel-VoxelTerrain.pdf
 */
template <class configType>
class accessor : public base<accessor<configType> >
{
public:
    typedef configType t_config;
    typedef base<accessor<t_config> > t_base;
    typedef typename t_config::t_data t_voxel;

#if defined(BOOST_NO_CXX11_CONSTEXPR)
    static const int32 voxelLength;
    static const int32 voxelLengthWithNormalCorrection;
    static const int32 voxelLengthLod;
    static const int32 voxelCount;
    static const int32 voxelCountLod;
    static const int32 voxelCountLodAll;
    static const int32 voxelLengthSurface;
    static const int32 voxelCountSurface;
#else
    static constexpr int32 voxelLength = t_config::voxelsPerTile;
    static constexpr int32 voxelLengthWithNormalCorrection = voxelLength+3;
    static constexpr int32 voxelLengthLod = (voxelLength+1)*2;
    static constexpr int32 voxelCount = voxelLengthWithNormalCorrection*voxelLengthWithNormalCorrection*voxelLengthWithNormalCorrection;
    static constexpr int32 voxelCountLod = voxelLengthLod*voxelLengthLod;
    static constexpr int32 voxelCountLodAll = 6*voxelCountLod;
    static constexpr int32 voxelLengthSurface = t_config::voxelsPerTile+1;
    static constexpr int32 voxelCountSurface = voxelLengthSurface*voxelLengthSurface*voxelLengthSurface;
#endif

    typedef vector<t_voxel> t_voxelArray;
    typedef vector<t_voxel> t_voxelArrayLod;

    /**
     * @brief create creates an instance.
     * @return Never nullptr.
     */
    static typename t_base::pointer create()
    {
        return typename t_base::pointer(new accessor());
    }

    /**
     * @brief ~accessor destructor.
     */
    virtual ~accessor()
    {
        if (m_calculateLod)
        {
            m_voxelsLod.reset();
        }
    }

    /**
     * @brief setVoxel set ancalculateIndex convertes a 3d voxel-pos to a 1d array-index.
     * @param pos -1 <= pos.xyz < voxelLengthWithNormalCorrection-1
     * @param toSet
     * @return returns true if anything changed.
     */
    bool setVoxel(const vector3int32& pos, const t_voxel& toSet)
    {
        BASSERT(pos.x >= -1);
        BASSERT(pos.y >= -1);
        BASSERT(pos.z >= -1);
        BASSERT(pos.x < voxelLengthWithNormalCorrection-1);
        BASSERT(pos.y < voxelLengthWithNormalCorrection-1);
        BASSERT(pos.z < voxelLengthWithNormalCorrection-1);

        if (toSet.getInterpolation() >= 0 && pos >= vector3int32(0) && pos < vector3int32(voxelLengthSurface))
        {
            ++m_numVoxelLargerZero;
        }

        const int32 index((pos.x+1)*voxelLengthWithNormalCorrection*voxelLengthWithNormalCorrection + (pos.y+1)*voxelLengthWithNormalCorrection + pos.z+1);
        const t_voxel oldValue(m_voxels[index]);
        m_voxels[index] = toSet;

        return oldValue != toSet;
    }

    /**
     * @brief setVoxelLod sets a voxel to a lod array.
     * @param pos
     * @param toSet
     * @param lod level of detail index
     * @return returns true if anything changed.
     * @see calculateCoordsLod()
     */
    bool setVoxelLod(const vector3int32& pos, const t_voxel& toSet, const int32& lod)
    {
        return setVoxelLod(calculateCoordsLod(pos, lod), toSet, lod);
    }

    /**
     * @brief getVoxel return ref to voxel.
     * @param pos -1 <= pos.xyz < voxelLengthWithNormalCorrection-1
     * @return
     */
    const t_voxel& getVoxel(const vector3int32& pos) const
    {
        BASSERT(pos.x >= -1);
        BASSERT(pos.y >= -1);
        BASSERT(pos.z >= -1);
        BASSERT(pos.x < voxelLengthWithNormalCorrection-1);
        BASSERT(pos.y < voxelLengthWithNormalCorrection-1);
        BASSERT(pos.z < voxelLengthWithNormalCorrection-1);

        const int32 index((pos.x+1)*voxelLengthWithNormalCorrection*voxelLengthWithNormalCorrection + (pos.y+1)*voxelLengthWithNormalCorrection + pos.z+1);
        return m_voxels[index];
    }
    /**
     * @brief getVoxelLod returns ref to lod-voxel
     * @param pos
     * @param lod level of detail index
     * @return
     * @see calculateCoordsLod()
     */
    const t_voxel& getVoxelLod(const vector3int32& pos, const int32& lod) const
    {
        return getVoxelLod(calculateCoordsLod(pos, lod), lod);
    }

    /**
     * @brief isEmpty returns true if all voxel are minimum.
     * @return
     * @see procedural::voxel::data
     */
    bool isEmpty() const
    {
        return m_numVoxelLargerZero == 0;
    }

    /**
     * @brief isFull returns true if all voxel are maximum.
     * @return
     * @see procedural::voxel::data
     */
    bool isFull() const
    {
        return m_numVoxelLargerZero == voxelCountSurface;
    }

    /**
     * @brief getCalculateLod returns true if surface later shall calculate level-of-detail
     * @return
     */
    const bool& getCalculateLod() const
    {
        return m_calculateLod;
    }

    /**
     * @brief getNumVoxelLargerZero returns number of voxel not minimum.
     * @return
     * @see procedural::voxel::data
     */
    const int32& getNumVoxelLargerZero() const
    {
        return m_numVoxelLargerZero;
    }

    /**
     * @brief getNumVoxelLargerZeroLod returns number of voxel in lod not minimum.
     * @return
     */
    const int32& getNumVoxelLargerZeroLod() const
    {
        return m_numVoxelLargerZeroLod;
    }

    /**
     * @brief getVoxelArray return voxel-array
     * @return
     */
    const t_voxelArray& getVoxelArray() const
    {
        return m_voxels;
    }
    /**
     * @brief getVoxelArray return voxel-array
     * @return
     */
    t_voxelArray& getVoxelArray()
    {
        return m_voxels;
    }
    /**
     * @brief getVoxelArrayLod returns nullptr if no lod shall get calculated else 6-lod-arrays of voxels.
     * @return
     * @see getCalculateLod()
     */
    t_voxelArrayLod* getVoxelArrayLod() const
    {
        return m_voxelsLod.get();
    }
    /**
     * @brief getVoxelArrayLod returns nullptr if no lod shall get calculated else 6-lod-arrays of voxels.
     * @return returns
     * @see getCalculateLod()
     */
    t_voxelArrayLod* getVoxelArrayLod()
    {
        return m_voxelsLod.get();
    }

    /**
     * @brief setCalculateLod enables or disables lod calculation and voxel buffering for it.
     * @param lod
     */
    void setCalculateLod(const bool& lod)
    {
        if (m_calculateLod == lod)
        {
            // do nothing
//            BWARNING("m_calculateLod == lod");
            return;
        }
        m_calculateLod = lod;
        if (m_calculateLod)
        {
            BASSERT(m_voxelsLod == nullptr);
            m_voxelsLod.reset(new t_voxelArrayLod(6*voxelCountLod)); // [6*voxelCountLod]
        }
        else
        {
            m_voxelsLod.reset();
        }
    }

    /**
     * @brief setNumVoxelLargerZero internally used for extern sync. (optimisation)
     * @param toSet
     */
    void setNumVoxelLargerZero(const int32& toSet)
    {
        m_numVoxelLargerZero = toSet;
    }
    /**
     * @brief setNumVoxelLargerZero internally used for extern sync. (optimisation)
     * @param toSet
     */
    void setNumVoxelLargerZeroLod(const int32& toSet)
    {
        m_numVoxelLargerZeroLod = toSet;
    }

protected:
    /**
     * @brief accessor constructor
     */
    accessor()
        : m_voxels(voxelCount)
        , m_voxelsLod(nullptr)
        , m_calculateLod(false)
        , m_numVoxelLargerZero(0)
        , m_numVoxelLargerZeroLod(0)
    {
    }

    /**
     * @see setVoxelLod()
     */
    bool setVoxelLod(const vector2int32& index, const t_voxel& toSet, const int32& lod)
    {
        BASSERT(index >= 0);
        BASSERT(index < voxelCountLod);
        BASSERT(m_calculateLod);
        BASSERT(m_voxelsLod.get() != nullptr);

        if (toSet.getInterpolation() >= 0)
        {
            ++m_numVoxelLargerZeroLod;
        }
        const uint32 index_(lod*voxelCountLod + index.x*voxelLengthLod + index.y);
        const t_voxel oldValue((*m_voxelsLod)[index_]);
        (*m_voxelsLod)[index_] = toSet;

        return oldValue != toSet;
    }
    /**
     * @see getVoxelLod()
     */
    const t_voxel& getVoxelLod(const vector2int32& index, const int32& lod) const
    {
        BASSERT(index >= 0);
        BASSERT(index < voxelCountLod);
        BASSERT(m_calculateLod);
        return (*m_voxelsLod)[lod*voxelCountLod + index.x*voxelLengthLod + index.y];
    }

    /**
     * @brief calculateCoordsLod converts a 3d lod-coord to a 2d. Transvoxel needs per quader-side (6) only one 2d half-size/detail array.
     * See http://www.terathon.com/voxels/ and http://www.terathon.com/lengyel/Lengyel-VoxelTerrain.pdf
     * @param pos
     * @param lod
     * @return
     */
    static vector2int32 calculateCoordsLod(const vector3int32& pos, const int32& lod)
    {
        BASSERT(lod >= 0);
        BASSERT(lod < 6);
        BASSERT(pos.x >= 0);
        BASSERT(pos.y >= 0);
        BASSERT(pos.z >= 0);
        BASSERT(pos.x < voxelLengthLod);
        BASSERT(pos.y < voxelLengthLod);
        BASSERT(pos.z < voxelLengthLod);
        BASSERT(pos.x == 0 || pos.y == 0 || pos.z == 0); // 2d!

        switch (lod)
        {
        case 0: // x
        case 1:
            BASSERT(pos.x == 0);
            return vector2int32(pos.y, pos.z);
        case 2: // y
        case 3:
            BASSERT(pos.y == 0);
            return vector2int32(pos.x, pos.z);
        case 4: // z
        case 5:
            BASSERT(pos.z == 0);
            return vector2int32(pos.x, pos.y);
        default:
            BASSERT(false);
        }
        return vector2int32();
    }

private:
    BLUB_SERIALIZATION_ACCESS

    template <class formatType>
    void save(formatType & readWrite, const uint32& version) const
    {
        using namespace serialization;

        (void)version;

        readWrite << nameValuePair::create("calculateLod", m_calculateLod);
    }
    template <class formatType>
    void load(formatType & readWrite, const uint32& version)
    {
        using namespace serialization;

        (void)version;

        bool calculateLod;
        readWrite >> nameValuePair::create("calculateLod", calculateLod);
        setCalculateLod(calculateLod);
    }

    template <class formatType>
    void serialize(formatType & readWrite, const uint32& version)
    {
        using namespace serialization;

        (void)version;

        readWrite & nameValuePair::create("numVoxelLargerZero", m_numVoxelLargerZero);
        readWrite & nameValuePair::create("numVoxelLargerZeroLod", m_numVoxelLargerZeroLod);
        saveLoad(readWrite, *this, version); // handle m_calculateLod
        readWrite & nameValuePair::create("voxels", m_voxels); // OPTIMISE gives twice the size in binary format (2 instead of 1)

        if (m_calculateLod)
        {
            BASSERT(!m_voxelsLod.isNull());
            readWrite & nameValuePair::create("voxelsLod", *m_voxelsLod);
        }
    }



private:
    t_voxelArray m_voxels;
    blub::scopedPointer<t_voxelArrayLod> m_voxelsLod;

    bool m_calculateLod;
    int32 m_numVoxelLargerZero;
    int32 m_numVoxelLargerZeroLod;


};


#if defined(BOOST_NO_CXX11_CONSTEXPR)
template <class voxelType> const int32 accessor<voxelType>::voxelLength = t_config::voxelsPerTile;
template <class voxelType> const int32 accessor<voxelType>::voxelLengthWithNormalCorrection = voxelLength+3;
template <class voxelType> const int32 accessor<voxelType>::voxelLengthLod = (voxelLength+1)*2;
template <class voxelType> const int32 accessor<voxelType>::voxelCount = voxelLengthWithNormalCorrection*voxelLengthWithNormalCorrection*voxelLengthWithNormalCorrection;
template <class voxelType> const int32 accessor<voxelType>::voxelCountLod = voxelLengthLod*voxelLengthLod;
template <class voxelType> const int32 accessor<voxelType>::voxelCountLodAll = 6*voxelCountLod;
template <class voxelType> const int32 accessor<voxelType>::voxelLengthSurface = t_config::voxelsPerTile+1;
template <class voxelType> const int32 accessor<voxelType>::voxelCountSurface = voxelLengthSurface*voxelLengthSurface*voxelLengthSurface;
#else
template <class voxelType> constexpr int32 accessor<voxelType>::voxelLength;
template <class voxelType> constexpr int32 accessor<voxelType>::voxelLengthWithNormalCorrection;
template <class voxelType> constexpr int32 accessor<voxelType>::voxelLengthLod;
template <class voxelType> constexpr int32 accessor<voxelType>::voxelCount;
template <class voxelType> constexpr int32 accessor<voxelType>::voxelCountLod;
template <class voxelType> constexpr int32 accessor<voxelType>::voxelCountLodAll;
template <class voxelType> constexpr int32 accessor<voxelType>::voxelLengthSurface;
template <class voxelType> constexpr int32 accessor<voxelType>::voxelCountSurface;
#endif


}
}
}
}




#endif // PROCEDURAL_VOXEL_TILE_ACCESSOR_HPP
